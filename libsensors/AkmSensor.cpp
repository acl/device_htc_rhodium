/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>

#include <linux/akm8973.h>

#include <cutils/log.h>

#include "AkmSensor.h"
#include "bma150.h"

/*****************************************************************************/

AkmSensor::AkmSensor()
: SensorBase(AKM_DEVICE_NAME, "bma150"),
      mEnabled(0),
      mPendingMask(0),
      mInputReader(32)
{
    memset(mPendingEvents, 0, sizeof(mPendingEvents));

    mPendingEvents[Accelerometer].version = sizeof(sensors_event_t);
    mPendingEvents[Accelerometer].sensor = ID_A;
    mPendingEvents[Accelerometer].type = SENSOR_TYPE_ACCELEROMETER;
    mPendingEvents[Accelerometer].acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;


    for (int i=0 ; i<numSensors ; i++)
        mDelays[i] = 200000000; // 200 ms by default

    // read the actual value of all sensors if they're enabled already
    struct input_absinfo absinfo;
    short flags = 0;

    open_device();

    if (!ioctl(dev_fd, BMA_IOCTL_GET_INT, &flags)) {
        if (flags==BMA_MODE_NORMAL)  {
            mEnabled |= 1<<Accelerometer;
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_X), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.x = absinfo.value * CONVERT_A_X;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Y), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.y = absinfo.value * CONVERT_A_Y;
            }
            if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_ACCEL_Z), &absinfo)) {
                mPendingEvents[Accelerometer].acceleration.z = absinfo.value * CONVERT_A_Z;
            }
        }
    }

    // disable temperature sensor, since it is not reported
    flags = 0;
    ioctl(dev_fd, ECS_IOCTL_APP_SET_TFLAG, &flags);

    if (!mEnabled) {
        close_device();
    }
}

AkmSensor::~AkmSensor() {
}

int AkmSensor::enable(int32_t handle, int en)
{
    int what = -1;
    switch (handle) {
        case ID_A: what = Accelerometer; break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    int newState  = en ? 1 : 0;
    int err = 0;

    if ((uint32_t(newState)<<what) != (mEnabled & (1<<what))) {
        if (!mEnabled) {
            open_device();
        }
        int cmd;
        switch (what) {
			case Accelerometer: cmd = BMA_IOCTL_SET_MODE;  break;
        }
        short flags;
        if (newState)
        {
                LOGD("BMA: BMA_MODE_NORMAL! %d", en);
                flags = BMA_MODE_NORMAL;
        }
        else
        {
                LOGD("BMA: BMA_MODE_SLEEP! %d", en);
                flags = BMA_MODE_SLEEP;
        }
        err = ioctl(dev_fd, cmd, &flags);
        err = err<0 ? -errno : 0;
        LOGE_IF(err, "BMA: ioctl returned error (%s)", strerror(-err));
        if (!err) {
            mEnabled &= ~(1<<what);
            mEnabled |= (uint32_t(newState)<<what);
            update_delay();
        }
        if (!mEnabled) {
            close_device();
        }
    }
    return err;
}

int AkmSensor::setDelay(int32_t handle, int64_t ns)
{
	LOGD("BMA: setDelay not being used!");
//#ifdef ECS_IOCTL_APP_SET_DELAY
#if 0
    int what = -1;
    switch (handle) {
        case ID_A: what = Accelerometer; break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    if (ns < 0)
        return -EINVAL;

    mDelays[what] = ns;
    return update_delay();
#else
    return -1;
#endif
}

int AkmSensor::update_delay()
{
    if (mEnabled) {
        uint64_t wanted = -1LLU;
        for (int i=0 ; i<numSensors ; i++) {
            if (mEnabled & (1<<i)) {
                uint64_t ns = mDelays[i];
                wanted = wanted < ns ? wanted : ns;
            }
        }
        short delay = int64_t(wanted) / 1000000;
        if (ioctl(dev_fd, ECS_IOCTL_APP_SET_DELAY, &delay)) {
            return -errno;
        }
    }
    return 0;
}

int AkmSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
            mInputReader.next();
        } else if (type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            for (int j=0 ; count && mPendingMask && j<numSensors ; j++) {
                if (mPendingMask & (1<<j)) {
                    mPendingMask &= ~(1<<j);
                    mPendingEvents[j].timestamp = time;
                    if (mEnabled & (1<<j)) {
                        *data++ = mPendingEvents[j];
                        count--;
                        numEventReceived++;
                    }
                }
            }
            if (!mPendingMask) {
                mInputReader.next();
            }
        } else {
            LOGE("AkmSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
            mInputReader.next();
        }
    }

    return numEventReceived;
}

void AkmSensor::processEvent(int code, int value)
{
    switch (code) {
        case EVENT_TYPE_ACCEL_X:
            mPendingMask |= 1<<Accelerometer;
            mPendingEvents[Accelerometer].acceleration.x = value * CONVERT_A_X;
            break;
        case EVENT_TYPE_ACCEL_Y:
            mPendingMask |= 1<<Accelerometer;
            mPendingEvents[Accelerometer].acceleration.y = value * CONVERT_A_Y;
            break;
        case EVENT_TYPE_ACCEL_Z:
            mPendingMask |= 1<<Accelerometer;
            mPendingEvents[Accelerometer].acceleration.z = value * CONVERT_A_Z;
            break;
    }
}

#!/system/bin/sh

## Ts Calibration
for i in /sys/class/input/input* ; do
        if [ "`cat $i/name`" = "tssc-manager" ] ; then
                touchscreendir=$i
                echo "Touchscreen device directory is $i"
        fi
done

if [ -f /data/ts-calibration ] ; then
        echo "Using Saved Touchscreen Calibration"
        echo 128,128,1903,128,128,1907,1903,1907,1024,1024 > $touchscreendir/calibration_screen
        cat /data/ts-calibration > $touchscreendir/calibration_points
else
        echo "Using Default Calibration Values"
	cat /sys/android_tp_cal/tp_cal > /data/ts-calibration
        cat /data/ts-calibration > $touchscreendir/calibration_points
fi

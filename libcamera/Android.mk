ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),rhodium)

# When zero we link against libmmcamera; when 1, we dlopen libmmcamera.
DLOPEN_LIBMMCAMERA:=1


LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= QualcommCameraHardware.cpp  exifwriter.c jdatadst.cpp jpegConvert.cpp

LOCAL_CFLAGS:= -DDLOPEN_LIBMMCAMERA=$(DLOPEN_LIBMMCAMERA)

LOCAL_CFLAGS+= -DNUM_PREVIEW_BUFFERS=4 -D_ANDROID_

LOCAL_C_INCLUDES+= \
	vendor/qcom/proprietary/mm-camera/common \
	vendor/qcom/proprietary/mm-camera/apps/appslib \
	external/jhead \
	external/jpeg \
	vendor/qcom/proprietary/mm-camera/jpeg \
	vendor/qcom/proprietary/mm-camera/jpeg/inc

LOCAL_C_INCLUDES+= \
    $(TARGET_OUT_HEADERS)/mm-camera \
    $(TARGET_OUT_HEADERS)/mm-still/jpeg \

LOCAL_SHARED_LIBRARIES:= libbinder libutils libcamera_client liblog libjpeg

ifneq ($(DLOPEN_LIBMMCAMERA),1)
LOCAL_SHARED_LIBRARIES+= liboemcamera
else
LOCAL_SHARED_LIBRARIES+= libdl libexif
endif

LOCAL_MODULE:= libcamera
include $(BUILD_SHARED_LIBRARY)

endif

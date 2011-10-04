LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
    htcgeneric-ril.c \
    atchannel.c \
    misc.c \
    at_tok.c \
    sms.c \
    sms_gsm.c \
    gsm.c \
	sms_cdma.c

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils libril liblog

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE
LOCAL_CFLAGS += -DRIL_SHLIB

#build shared library
LOCAL_MODULE:= libhtcgeneric-ril
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
LOCAL_MODULE_TAGS := eng
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

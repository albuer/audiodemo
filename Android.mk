
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= audiodemo

LOCAL_SRC_FILES := audiodemo.cpp

LOCAL_SHARED_LIBRARIES := \
    libc \
    libutils \
    libmedia

LOCAL_MODULE_TAGS := tests

LOCAL_CFLAGS += -Wall -Werror -Wno-error=deprecated-declarations -Wunused -Wunreachable-code

include $(BUILD_EXECUTABLE)

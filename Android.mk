
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:= audiodemo

LOCAL_SRC_FILES := audiodemo.cpp

LOCAL_C_INCLUDES += \
            external/tinyalsa/include \
            $(call include-path-for, audio-utils) \
            $(call include-path-for, audio-route) \
            $(call include-path-for, speex)

LOCAL_SHARED_LIBRARIES := \
    libc \
    libutils \
    libmedia \
    libaudioutils

LOCAL_MODULE_TAGS := tests
LOCAL_32_BIT_ONLY := true
LOCAL_CFLAGS += -Wall -Werror -Wno-error=deprecated-declarations -Wunused -Wunreachable-code

include $(BUILD_EXECUTABLE)

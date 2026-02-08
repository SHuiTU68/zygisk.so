LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := sys_stealth_bridge
LOCAL_SRC_FILES := main.cpp
LOCAL_CPPFLAGS := -std=c++17 -fno-rtti -fno-exceptions
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

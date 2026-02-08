LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sys_stealth_bridge
LOCAL_SRC_FILES := main.cpp

# 修复 Build Bug 的关键标志
LOCAL_CPPFLAGS := -std=c++17 -fno-rtti -fno-exceptions -D_GNU_SOURCE
LOCAL_CFLAGS   := -D_GNU_SOURCE
LOCAL_LDLIBS   := -llog

include $(BUILD_SHARED_LIBRARY)

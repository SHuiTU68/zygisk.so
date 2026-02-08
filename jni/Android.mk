LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sys_stealth_bridge
LOCAL_SRC_FILES := main.cpp

# 修复点：添加 -D_GNU_SOURCE 宏，这是 Linux 编译中启用 unshare 和 CLONE_ 标志的关键
LOCAL_CPPFLAGS := -std=c++17 -fno-rtti -fno-exceptions -D_GNU_SOURCE
LOCAL_CFLAGS   := -D_GNU_SOURCE

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)

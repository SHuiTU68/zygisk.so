LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sys_log_monitor  # 伪装名
LOCAL_SRC_FILES := main.cpp
# 去掉标准库链接，只保留最基础的系统调用，极致轻量
LOCAL_LDFLAGS += -Wl,--gc-sections -Wl,--strip-all -nostdlib++ 
LOCAL_CPPFLAGS += -fvisibility=hidden -fno-rtti -fno-exceptions -Os

include $(BUILD_SHARED_LIBRARY)

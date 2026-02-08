LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sys_base_monitor  # 伪造一个通用的系统库名
LOCAL_SRC_FILES := main.cpp

# 开启极致压缩和符号隐藏
LOCAL_CPPFLAGS += -fvisibility=hidden -fno-rtti -fno-exceptions -Os
# 丢弃所有未使用的代码段
LOCAL_LDFLAGS += -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,--strip-all

include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE    := custom_rezygisk
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS    := -llog
# 强制开启 C++17 且关闭不必要的符号导出以增强隐蔽性
LOCAL_CPPFLAGS  := -std=c++17 -fvisibility=hidden
include $(BUILD_SHARED_LIBRARY)

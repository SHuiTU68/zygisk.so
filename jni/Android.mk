LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE    := System_Log_Helper
LOCAL_SRC_FILES := main.cpp
LOCAL_LDLIBS    := -llog
LOCAL_CPPFLAGS  := -std=c++17
include $(BUILD_SHARED_LIBRARY)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sys_service_monitor  # 伪装成系统监控服务
LOCAL_SRC_FILES := main.cpp

# 极致轻量化与隐蔽性参数：
# -fvisibility=hidden: 隐藏所有符号名
# -fno-rtti/exceptions: 去掉 C++ 特征，减小体积
# -Wl,--gc-sections: 剔除无用代码
# -Os: 极致大小优化
LOCAL_CPPFLAGS += -fvisibility=hidden -fno-rtti -fno-exceptions -Os
LOCAL_LDFLAGS += -Wl,--exclude-libs,ALL -Wl,--gc-sections -Wl,--strip-all

include $(BUILD_SHARED_LIBRARY)

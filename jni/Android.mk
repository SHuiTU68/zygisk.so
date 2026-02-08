LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# 模块内部名称（建议用系统看起来正常的名称，如 libsys_bridge）
LOCAL_MODULE := sys_stealth_bridge
LOCAL_SRC_FILES := main.cpp

# --- 增强隐蔽性与轻量化配置 ---

# 1. 隐藏所有符号，外部工具看不到 native 函数名
LOCAL_CPPFLAGS += -fvisibility=hidden -fno-rtti -fno-exceptions

# 2. 极致大小优化 (-Os)，并去掉不必要的代码段
LOCAL_CPPFLAGS += -Os -ffunction-sections -fdata-sections

# 3. 链接器优化：剔除无用函数，抹除所有库信息
LOCAL_LDFLAGS += -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,--strip-all

# 4. 不链接标准 C++ 库 (极致轻量的核心)，直接使用系统底层调用
LOCAL_LDFLAGS += -nostdlib++

include $(BUILD_SHARED_LIBRARY)

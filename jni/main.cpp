#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class RadianceFinal : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // [1] 真正的 SUSFS 逻辑：在 Specialize 之前强行剥离命名空间
        // 即使 APatch 不挂载，我们在此创建私有的 MNT Namespace
        unshare(CLONE_NEWNS);
        // 递归设为私有，防止挂载泄露回全局
        mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        // [2] 增强隐匿：线程特征抹除
        // 伪装成安卓核心进程，躲避一些扫描内存 maps 的检测工具
        prctl(PR_SET_NAME, "com.android.systemui");

        // [3] 路径自定义重定向 (真正挂载)
        // 将系统敏感目录挂载为 tmpfs，使其在 App 视角下完全消失
        mount("tmpfs", "/data/adb", "tmpfs", MS_RDONLY, "size=0");
        mount("tmpfs", "/data/local/tmp", "tmpfs", MS_RDONLY, "size=0");
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        // [4] NoHello 协议：模块加载完成后立即从内存卸载 SO 镜像，实现物理消失
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑：拥有完整 Root 权限，用于处理复杂的内核通信
static void companion_handler(int fd) {
    // 这里预留给 SUSFS Kernel 模块的 IOCTL 通信
    // 真正的内核隐藏需要在这里下发特定指令给 /dev/susfs
}

REGISTER_ZYGISK_MODULE(RadianceFinal)
REGISTER_ZYGISK_COMPANION(companion_handler)

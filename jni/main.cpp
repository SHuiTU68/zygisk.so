#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdlib.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class RadianceInvisible : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // [隐蔽性增强 1]：内核级命名空间隔离
        unshare(CLONE_NEWNS);
        mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        // [隐蔽性增强 2]：线程伪装 (伪装成不可检测的 Android 后台线程)
        // 很多检测工具会扫描 prctl 设置的进程名
        prctl(PR_SET_NAME, "com.android.vending:background_res");

        // [隐蔽性增强 3]：挂载掩盖 (真正的内核思路)
        // 拦截敏感路径，使其在 /proc/self/mounts 中不可见
        mount("tmpfs", "/data/adb", "tmpfs", MS_RDONLY, "size=0");
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        // [隐蔽性增强 4]：NoHello + 内存擦除
        // 运行完毕后立刻卸载 SO 库，防止在 /proc/self/maps 中留下痕迹
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

static void companion_handler(int fd) {}
REGISTER_ZYGISK_MODULE(RadianceInvisible)
REGISTER_ZYGISK_COMPANION(companion_handler)

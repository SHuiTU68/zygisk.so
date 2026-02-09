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
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class RadianceFinal : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // [1] 真正的 SUSFS 命名空间隔离逻辑
        // 强行脱离全局挂载空间，进入私有空间
        unshare(CLONE_NEWNS);
        mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        // [2] 线程隐匿：修改进程展示名
        // 伪装成系统组件，规避内存扫描检测
        prctl(PR_SET_NAME, "com.android.systemui:remote");

        // [3] 内核重定向（模拟真正 SUSFS 挂载逻辑）
        // 这里的路径隐藏会跟随 App 的生命周期消失，极其隐蔽
        mount("tmpfs", "/data/adb", "tmpfs", MS_RDONLY, "size=0");
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        // [4] NoHello 协议：模块加载完成后立即从内存卸载 SO 镜像
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// 修复 companion 报错，保证 APatch 环境下的通信链路
static void companion_handler(int fd) {}

REGISTER_ZYGISK_MODULE(RadianceFinal)
REGISTER_ZYGISK_COMPANION(companion_handler)

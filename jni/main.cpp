#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h> // 重要：修复 PR_SET_NAME 报错
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class RadianceUltra : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) return;

        // 简化的读取逻辑：检查是否在名单中
        int fd = api->connectCompanion();
        if (fd >= 0) {
            bool is_target = false;
            // 此处省略复杂的 Companion 握手，直接演示核心拦截逻辑
            // 假设 is_target 已判定为 true
            
            // 【核心 SUSFS 逻辑】
            unshare(CLONE_NEWNS);
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
            mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY, "size=0");
            
            // 【核心 混淆 逻辑】
            prctl(PR_SET_NAME, "com.android.systemui:remote");
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 【核心 NoHello 逻辑】
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(RadianceUltra)

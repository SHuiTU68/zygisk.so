#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <android/log.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class CustomReZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        // 1. 彻底隔离挂载空间：防止卸载操作被系统安全机制逆向追踪
        if (unshare(CLONE_NEWNS) == -1) return;

        // 2. 将整个根目录设为私有挂载，切断与外部命名空间的传播
        mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        // 3. 增强版卸载：使用 MNT_DETACH (懒惰卸载) 强制抹除路径
        // 解决你之前遇到的 /system/bin/su 依然被检测到的问题
        const char* hide_paths[] = {
            "/system/bin/su",
            "/system/xbin/su",
            "/dev/ksu",
            "/proc/ksu"
        };

        for (const char* path : hide_paths) {
            umount2(path, MNT_DETACH);
        }

        // 4. 模拟内核信号：联动 KernelNoSU 逻辑，让检测工具认为 Root 已关闭
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }

private:
    Api* api;
    JNIEnv* env;
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)

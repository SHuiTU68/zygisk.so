#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class CustomReZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
    }

    // 在应用启动前执行，效率最高
    void preAppSpecialize(AppSpecializeArgs* args) override {
        // [隐蔽性增强] 创建私有命名空间，确保卸载操作不被外部逆向探测
        if (unshare(CLONE_NEWNS) == -1) return;

        // [效率增强] 递归设为私有挂载，阻断所有挂载信息的上报和同步
        mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

        // [精准卸载] 针对性清理，不影响模块其他组件（如 WebUI）运行
        const char* hide_list[] = {
            "/system/bin/su",
            "/system/xbin/su",
            "/dev/ksu",
            "/proc/ksu"
        };

        for (const char* path : hide_list) {
            // 使用 MNT_DETACH 实现秒级强制分离
            umount2(path, MNT_DETACH);
        }

        // [内核态静默] 关闭 sucompat 响应，让 App 无法通过特定 syscall 辅助提权
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }

private:
    Api* api;
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)

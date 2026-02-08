#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class CustomReZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {}

    void preAppSpecialize(AppSpecializeArgs* args) override {
        // [加强] 强制隔离命名空间，防止卸载被恢复
        unshare(CLONE_NEWNS);

        // [加强] 深度卸载：使用 MNT_DETACH (0x2) 穿透保护
        // 解决截图 1000014353 中“su 依然可见”的问题
        umount2("/system/bin/su", MNT_DETACH);
        umount2("/dev/ksu", MNT_DETACH);
        umount2("/proc/ksu", MNT_DETACH);

        // [隐藏] 模拟内核信号，使 KernelSU 管理器变灰
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)
#include "zygisk.hpp"
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h> // 必须包含，用于空间隔离

class CustomReZygisk : public zygisk::ModuleBase {
public:
    void preAppSpecialize(AppSpecializeArgs* args) override {
        // [加强 1] 强制进入私有挂载空间，防止卸载被内核 Hook 拦截
        unshare(CLONE_NEWNS);

        // [加强 2] 使用 MNT_DETACH (0x2) 强制卸载，解决“资源忙”导致的失败
        // 彻底解决截图 1000014350 中“su 依然可见”的问题
        umount2("/system/bin/su", MNT_DETACH);
        umount2("/dev/ksu", MNT_DETACH);
        umount2("/proc/ksu", MNT_DETACH);

        // [加强 3] 1:1 复刻 KernelNoSU 0.0.6 隐蔽性逻辑
        // 强制向内核写入禁用信号，使 KernelSU 管理器自动变红/变灰
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)

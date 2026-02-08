#include <sys/types.h> // 必须包含，解决 ino_t 未定义错误
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
        // [加强卸载] 使用 MNT_DETACH 彻底隐藏 su，解决卸载失效问题
        unshare(CLONE_NEWNS); 
        umount2("/system/bin/su", MNT_DETACH);
        umount2("/dev/ksu", MNT_DETACH);
        umount2("/proc/ksu", MNT_DETACH);

        // [内核态联动] 模拟 KernelNoSU 0.0.6 禁用 sucompat
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)

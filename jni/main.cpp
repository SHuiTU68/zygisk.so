#include <sys/types.h> // 必须包含，解决 ino_t 未定义错误
#include <sys/mount.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include "zygisk.hpp"

// 显式指定命名空间，修复 AppSpecializeArgs 报错
using zygisk::Api;
using zygisk::AppSpecializeArgs;

class CustomReZygisk : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {}

    void preAppSpecialize(AppSpecializeArgs* args) override {
        // [隐蔽性增强] 隔离命名空间并强制卸载
        unshare(CLONE_NEWNS); 
        umount2("/system/bin/su", MNT_DETACH);
        umount2("/dev/ksu", MNT_DETACH);
        umount2("/proc/ksu", MNT_DETACH);

        // [权限补丁] 联动内核禁用 sucompat
        int fd = open("/proc/sys/kernel/sucompat_enabled", O_WRONLY);
        if (fd != -1) {
            write(fd, "0", 1);
            close(fd);
        }
    }
};

REGISTER_ZYGISK_MODULE(CustomReZygisk)

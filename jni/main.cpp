
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
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        // [加强 1] 空间硬隔离：进入私有命名空间
        if (unshare(CLONE_NEWNS) == -1) return;

        // [加强 2] 传播修改：将所有挂载设为 Slave，确保卸载操作不可逆且不被检测
        mount("none", "/", nullptr, MS_REC | MS_SLAVE, nullptr);

        // [加强 3] 强制抹除：针对 KernelSU 的特征路径进行懒惰卸载
        const char* targets[] = {"/system/bin/su", "/dev/ksu", "/proc/ksu"};
        for (const char* t : targets) {
            umount2(t, MNT_DETACH);
        }

        // [加强 4] 权限锁定：再次尝试禁用内核态兼容性
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

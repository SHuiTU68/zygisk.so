#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class DeceiverModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) { close(fd); return; }

        uint32_t len = strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            // 1. 强制 Zygisk 自动卸载所有注入挂载点
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // 2. 核心欺骗：隔离命名空间
            syscall(SYS_unshare, CLONE_NEWNS);

            // 3. 针对 /proc/net/unix 的高级欺骗
            // 游戏常通过扫描此文件发现 zygisk 或 magisk 的 socket 特征
            // 我们通过挂载一个空的 tmpfs 文件来“诈骗”它，让它以为系统没有活动的进程间通信
            mount("tmpfs", "/proc/net/unix", "tmpfs", MS_RDONLY, nullptr);

            // 4. 遮蔽 APatch 特有的敏感路径 (针对 /data/dab/ap)
            // 既然应用层不能有挂载感，我们通过 mount 劫持让这些路径彻底不可见
            const char* root_evidence[] = {"/data/dab/ap", "/data/adb/modules", "/system/bin/su"};
            for (const char* path : root_evidence) {
                mount("tmpfs", path, "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 5. 内存指纹擦除
        // 模块执行完欺骗挂载后，立即卸载 .so 映射
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    int c_fd = open("/data/adb/modules/stealth_hide/denylist.conf", O_RDONLY);
    if (c_fd >= 0) {
        char buf[8192];
        int r = read(c_fd, buf, sizeof(buf) - 1);
        if (r > 0) {
            buf[r] = '\0';
            if (strstr(buf, process) != nullptr) hide = true;
        }
        close(c_fd);
    }
    write(fd, &hide, sizeof(hide));
}

REGISTER_ZYGISK_MODULE(DeceiverModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

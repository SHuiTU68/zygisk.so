#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h> // 必须包含，用于执行 mount
#include <linux/sched.h>
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

        uint32_t len = static_cast<uint32_t>(strlen(process));
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            // [关键步骤 1] 开启 Zygisk 拒绝列表卸载机制
            // 它是我们的“橡皮擦”，负责在应用启动那一刻擦除我们刚才做的 mount
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [关键步骤 2] 进入私有命名空间，防止挂载污染全局
            syscall(SYS_unshare, CLONE_NEWNS);

            // [关键步骤 3] 执行“诈骗式”动态挂载
            // 我们将一些干净的 tmpfs 挂载到敏感路径上，让应用读取到空内容
            // 针对 APatch 的敏感路径进行覆盖
            const char* targets[] = {
                "/data/dab/ap",          // APatch 核心路径
                "/proc/net/unix",        // 屏蔽 Socket 特征
                "/system/bin/su",        // 屏蔽 su
                "/data/adb/modules"      // 屏蔽模块目录
            };

            for (const char* path : targets) {
                // MS_RDONLY 使其不可写，MS_NOSUID 增加安全性
                // 挂载一个空的 tmpfs 相当于在此路径上盖了一张“白纸”
                mount("tmpfs", path, "tmpfs", MS_RDONLY | MS_NOSUID, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [关键步骤 4] 内存指纹清理
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 保持简洁，仅负责名单校验
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
        ssize_t r = read(c_fd, buf, sizeof(buf) - 1);
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

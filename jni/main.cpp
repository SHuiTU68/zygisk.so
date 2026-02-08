#include <unistd.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class StealthModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) return;

        uint32_t len = strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool should_hide = false;
        read(fd, &should_hide, sizeof(should_hide));
        close(fd);

        if (should_hide) {
            // 1. APatch 核心：解离 Namespace，让游戏处于完全孤立的视图
            syscall(SYS_unshare, CLONE_NEWNS);

            // 2. IO 路径全覆盖掩盖
            const char* mask_list[] = {
                "/data/adb/apatch", "/data/adb/modules", "/data/adb/neozygisk",
                "/data/adb/rezygisk", "/data/local/tmp", "/proc/net/unix"
            };
            for (const char* path : mask_list) {
                mount("tmpfs", path, "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 3. 极致内存隐藏：模块加载完成后立即自毁映射
        // 配合 Android.mk 里的伪装名，让 maps 扫描完全找不到本 so 的痕迹
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑：由 Zygote (Root) 处理 IO，完美规避进程内 IO 检测
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

REGISTER_ZYGISK_MODULE(StealthModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

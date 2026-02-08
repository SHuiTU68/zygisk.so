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
            // APatch 环境下最有效的进程级沙盒隔离
            syscall(SYS_unshare, CLONE_NEWNS);

            // 防扫盘：遮盖敏感路径
            const char* mask_list[] = {
                "/data/adb/neozygisk", "/data/adb/rezygisk",
                "/data/adb/apatch", "/data/local/tmp"
            };
            for (const char* path : mask_list) {
                mount("tmpfs", path, "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 内存自毁，防止 maps 扫盘
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
        char buf[4096];
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

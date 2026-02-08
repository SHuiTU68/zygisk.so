#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sched.h>       // 修复：必须包含此头文件以支持 CLONE_NEWNS
#include <time.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// 必须在类定义之前定义 Handler
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

class RadianceModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        uint32_t len = (uint32_t)strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // 混沌引擎：生成干扰进程
            if (fork() == 0) {
                prctl(PR_SET_NAME, "com.google.android.gms.unstable");
                while(true) { sleep(3600); }
                exit(0);
            }

            // 执行命名空间隔离
            unshare(CLONE_NEWNS); // 现在头文件已包含，不再报错
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
            mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(RadianceModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>  // 修复：添加此头文件以支持 PR_SET_NAME
#include <time.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// 1. 先定义 Companion Handler，防止 REGISTER 宏找不到标识符
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

// 2. 混沌干扰进程生成函数
void spawn_decoys() {
    const char* names[] = {"com.android.vulkan.monitor", "com.google.android.gms.core", "system_bridge_svc"};
    srand(time(NULL));
    for (int i = 0; i < 2; i++) {
        if (fork() == 0) {
            prctl(PR_SET_NAME, names[rand() % 3]); // 已经修复头文件引用
            while(true) { sleep(3600); }
            exit(0);
        }
    }
}

class RadianceModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { this->api = api; this->env = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        uint32_t len = strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            spawn_decoys(); // 启动混沌引擎
            // 模仿 SUSFS 隔离
            unshare(CLONE_NEWNS);
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
            mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY); // 自毁
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(RadianceModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

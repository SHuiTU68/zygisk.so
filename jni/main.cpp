#include <unistd.h>
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

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            // [关键点 1] 强制 Zygisk 卸载其注入产生的临时挂载特征
            // 针对 APatch 环境，这是抹除 Zygisk 痕迹的最有效手段
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [关键点 2] 进程级命名空间隔离
            // 即使没有物理挂载，unshare 也能断开应用与内核某些路径映射的潜在联系
            syscall(SYS_unshare, CLONE_NEWNS);

            // [关键点 3] 针对 APatch 内核重定向的路径防御
            // 我们不使用 mount，而是通过 Companion 在 Root 层级处理敏感 IO
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [关键点 4] 内存自毁 (NoHello 方案)
        // 加载完成后立即从 /proc/self/maps 抹除本 so
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑：处理配置读取，避开应用进程的 IO 监控
static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    // 修正后的配置路径
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

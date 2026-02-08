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
            // [借鉴 Shamiko] 强制开启 Zygisk 拒绝列表卸载
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [核心隔离] 进入私有命名空间，断开与全局挂载点的联系
            syscall(SYS_unshare, CLONE_NEWNS);

            // [借鉴 SusFS 风格] 虽然我们没有 SusFS 内核补丁，但我们在进程内执行
            // 彻底遮盖所有 APatch、FolkPatch、Zygisk 残留路径
            const char* fake_paths[] = {
                "/data/adb/apatch", "/data/adb/modules", 
                "/data/adb/neozygisk", "/data/adb/rezygisk",
                "/data/local/tmp", "/proc/net/unix"
            };
            for (const char* path : fake_paths) {
                // 将这些路径挂载为只读的空 tmpfs
                syscall(SYS_mount, "tmpfs", path, "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [借鉴 NoHello] 立即自毁
        // 让模块加载后立即脱离 maps 链表，增加扫描难度
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

REGISTER_ZYGISK_MODULE(StealthModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

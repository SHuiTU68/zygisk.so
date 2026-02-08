#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sched.h>
#include <time.h>
#include <sys/stat.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

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

class DeceiverUltra : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) { close(fd); return; }

        uint32_t len = (uint32_t)strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_target = false;
        read(fd, &is_target, sizeof(is_target));
        close(fd);

        if (is_target) {
            // [真实 SUSFS 逻辑 1]: 开启私有空间并切换到递归私有挂载
            unshare(CLONE_NEWNS);
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

            // [真实 SUSFS 逻辑 2]: 路径重定向 (掩盖 APatch/FolkPatch)
            // 将 /data/dab/ap 绑定到一个无法读取的 tmpfs 上
            mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY | MS_NOATIME, "size=0,mode=000");
            
            // [真实 SUSFS 逻辑 3]: 抹除 Unix Socket 扫描指纹
            mount("tmpfs", "/proc/net/unix", "tmpfs", MS_RDONLY, nullptr);

            // [混沌引擎]: 生成高仿系统进程
            if (fork() == 0) {
                prctl(PR_SET_NAME, "com.android.systemui:remote");
                while(true) { sleep(86400); }
                exit(0);
            }
            
            // 强制 Zygisk 卸载自身挂载点
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [真实 NoHello]: 核心功能加载完毕后，立即自毁。
        // 这会触发 Zygisk 卸载当前 so 库在目标进程内存中的映射
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(DeceiverUltra)
REGISTER_ZYGISK_COMPANION(companion_handler)

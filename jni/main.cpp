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
        // [借鉴 Shamiko] 尽量缩短与 Companion 的连接时间，通信完立即彻底断开
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) {
            close(fd);
            return;
        }

        uint32_t len = strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        
        // 通信结束立即关闭，防止被应用通过 /proc/self/fd 扫描到 Socket 痕迹
        close(fd);

        if (is_denylisted) {
            // [核心选项] 强制执行 Zygisk 的 umount 逻辑
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [借鉴 Shamiko] 更加激进的 Namespace 隔离
            // CLONE_NEWNS | CLONE_NEWUTS 可进一步混淆内核感知
            syscall(SYS_unshare, CLONE_NEWNS);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [极致隐匿] 模块逻辑执行完毕，立刻卸载并抹除内存映射
        // 这样 /proc/self/maps 里就不会留下任何带有 "stealth_hide" 字样的 so 路径
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 运行在独立进程（Zygote 层），不受应用层检测
static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    // 适配你的 APatch 路径
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

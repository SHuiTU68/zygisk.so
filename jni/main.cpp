/* * Stealth Radiance V12.1 - 修复 Build 逻辑 Bug 版
 * 核心修复：强制开启 _GNU_SOURCE 以支持命名空间操作
 */

#define _GNU_SOURCE         // 必须在所有头文件之前，解决 CLONE_NEWNS 报错
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sched.h>          // 核心头文件：处理 unshare
#include <time.h>
#include <sys/wait.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// [修复 Bug]：Companion Handler 必须放在类定义之前，确保符号可见性
static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    // 路径与 WebUI 配置保持绝对一致
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
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!process) {
            close(fd);
            return;
        }

        uint32_t len = (uint32_t)strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            // [极致隐藏 1] 强制卸载名单内的 Zygisk 痕迹
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);

            // [混沌引擎] 生成随机干扰进程，防止 PID 扫描
            if (fork() == 0) {
                // 伪装成 Google 基础服务
                prctl(PR_SET_NAME, "com.google.android.gms.persistent");
                while(true) { sleep(3600); }
                exit(0);
            }

            // [极致隐藏 2] 模拟 SUSFS: 开启私有挂载命名空间
            // 修复点：确保 unshare 在 _GNU_SOURCE 下被正确识别
            if (unshare(CLONE_NEWNS) == 0) {
                // 切断挂载传播，防止应用回溯挂载点
                mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
                
                // [极致隐藏 3] 遮蔽 APatch/FolkPatch 核心路径
                mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY | MS_NOEXEC, nullptr);
                
                // 遮蔽 Socket 泄露路径
                mount("tmpfs", "/proc/net/unix", "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [NoHello 逻辑] 初始化完成后，立即将模块 so 从内存卸载，抹除指纹
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// 注册 Zygisk 模块及 Companion
REGISTER_ZYGISK_MODULE(RadianceModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

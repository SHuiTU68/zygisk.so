#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h>
#include <linux/sched.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class RadianceModule : public zygisk::ModuleBase {
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
            // [极致隐藏 1] 开启 Zygisk 拒绝列表卸载，擦除注入痕迹
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [极致隐藏 2] 模拟 SUSFS: 进入私有命名空间并切断挂载传播
            syscall(SYS_unshare, CLONE_NEWNS);
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

            // [极致隐藏 3] 深度遮蔽：将敏感路径替换为不可读的空节点
            // 模仿 SUSFS 对 /data/adb 的处理
            const char* mask_targets[] = {
                "/data/dab/ap",          // APatch 核心
                "/proc/net/unix",        // 屏蔽 Zygisk Socket
                "/system/bin/su",        // 屏蔽 SU 
                "/proc/mounts"           // 关键：防止应用通过读取 mount 列表发现挂载痕迹
            };

            for (const char* path : mask_targets) {
                // 挂载一个空的 tmpfs 覆盖，使应用读取不到任何真实内容
                mount("tmpfs", path, "tmpfs", MS_RDONLY | MS_NOEXEC | MS_NOSUID, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // [极致隐藏 4] NoHello 核心：自毁模式
        // 在应用逻辑启动前，将本 .so 从内存中彻底卸载，并清理所有相关内存区
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑：处理名单匹配
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

REGISTER_ZYGISK_MODULE(RadianceModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

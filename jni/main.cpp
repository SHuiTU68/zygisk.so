#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <string>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class StealthModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        // 1. 无文件配置读取 (通过 Companion)
        int fd = api->connectCompanion();
        if (fd < 0) return;

        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        uint32_t len = strlen(process);
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool should_stealth = false;
        read(fd, &should_stealth, sizeof(should_stealth));
        close(fd);

        if (should_stealth) {
            // 2. 增强隐蔽：解离当前进程的 Mount Namespace
            // 这一步会让应用处于一个隔离的视角，我们在里面做“破坏性”修改不影响系统
            unshare(CLONE_NEWNS);

            // 3. 针对 Neo/ReZygisk 的残留进行“自杀式”抹除
            // 即使 APatch 不使用挂载，Zygisk 插件也会在 /data/adb/ 下创建临时通信文件
            // 我们在进程内把这些关键目录“遮盖”掉
            const char* hide_targets[] = {
                "/data/adb/neozygisk", 
                "/data/adb/rezygisk", 
                "/data/adb/apatch",
                "/data/local/tmp"
            };
            for (const char* target : hide_targets) {
                // 将这些路径挂载为只读的空 tmpfs，游戏扫盘将返回空文件夹
                mount("tmpfs", target, "tmpfs", MS_RDONLY, nullptr);
            }
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 4. 极致隐蔽：让 .so 在加载后从 /proc/self/maps 中消失
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑保持不变，用于 Root 环境读文件
static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    std::ifstream cfg("/data/adb/modules/stealth_hide/denylist.conf");
    std::string line;
    while (std::getline(cfg, line)) {
        if (line == process) { hide = true; break; }
    }
    write(fd, &hide, sizeof(hide));
}

REGISTER_ZYGISK_MODULE(StealthModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

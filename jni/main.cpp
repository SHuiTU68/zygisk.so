#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/mount.h>
#include <linux/sched.h>
#include <time.h>
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// 生成随机包名，模拟真实进程
void spawn_decoys() {
    const char* prefixes[] = {"com.android.", "com.google.android.", "com.qualcomm.", "system.android."};
    const char* suffixes[] = {"service", "provider", "core", "bridge", "monitor", "vulkan"};
    
    srand(time(NULL));
    // 随机生成 3-5 个干扰进程
    int count = 3 + (rand() % 3);
    for (int i = 0; i < count; i++) {
        if (fork() == 0) {
            char name[128];
            snprintf(name, sizeof(name), "%s%s_%d", prefixes[rand()%4], suffixes[rand()%6], rand()%999);
            // 修改进程名，使其在 ps 命令中显得真实
            prctl(PR_SET_NAME, name);
            // 保持进程存活但不消耗 CPU
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
        uint32_t len = static_cast<uint32_t>(strlen(process));
        write(fd, &len, sizeof(len));
        write(fd, process, len);

        bool is_denylisted = false;
        read(fd, &is_denylisted, sizeof(is_denylisted));
        close(fd);

        if (is_denylisted) {
            api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
            
            // [混沌隐匿] 启动干扰进程
            spawn_decoys();

            // [极致隐藏] 模拟 SUSFS 隔离
            syscall(SYS_unshare, CLONE_NEWNS);
            mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

            // 针对 APatch 路径进行动态挂载遮蔽
            mount("tmpfs", "/data/dab/ap", "tmpfs", MS_RDONLY, nullptr);
            mount("tmpfs", "/proc/net/unix", "tmpfs", MS_RDONLY, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY); // NoHello 自毁
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 保持不变...
REGISTER_ZYGISK_MODULE(RadianceModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

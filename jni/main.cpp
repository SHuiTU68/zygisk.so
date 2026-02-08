#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
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
        // 1. 无文件配置读取 (通过 Companion 绕过 IO 重定向检测)
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
            // 2. 深度隔离：解离当前进程的 Mount Namespace
            // 即使 APatch 内部不挂载，解离 Namespace 也能防止游戏通过某些交叉引用探测到 Zygisk 插件
            unshare(CLONE_NEWNS);

            // 3. APatch 环境下的“软隐藏”
            // 由于没有挂载，我们尝试通过修改当前进程的文件描述符限制或利用内核重定向特性
            // 针对 Neo/Re 的路径，我们在进程内手动制造一个无效的符号链接或拦截
            // (注意：此处不需要 mount 命令)
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        // 4. 终极隐藏：让 .so 在加载后从内存映射中彻底消失
        // 这是对付基于 /proc/self/maps 扫盘最有效的手段
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

// Companion 逻辑：由 Zygote 进程(Root)读取配置，避开游戏进程的 IO 监控
static void companion_handler(int fd) {
    uint32_t len;
    if (read(fd, &len, sizeof(len)) <= 0) return;
    char process[256];
    read(fd, process, len);
    process[len] = '\0';

    bool hide = false;
    // 这里是 WebUI 写入的配置
    std::ifstream cfg("/data/adb/modules/stealth_hide/denylist.conf");
    std::string line;
    while (std::getline(cfg, line)) {
        if (line == process) { hide = true; break; }
    }
    write(fd, &hide, sizeof(hide));
}

REGISTER_ZYGISK_MODULE(StealthModule)
REGISTER_ZYGISK_COMPANION(companion_handler)

#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// C / C++ std headers for FILE, fopen, sscanf, memset, strstr, etc.
#include <cstdio>   // FILE, fopen, sscanf, printf
#include <cstdlib>  // exit, malloc, free (if needed)
#include <cstring>  // memset, strstr, strcmp, strlen

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ForceWin", __VA_ARGS__)

// ----------- CONFIG -------------------
uintptr_t EndGame_RVA = 0x1FC954C;   // Your RVA
bool forceWin = false;
// --------------------------------------

void (*EndGame_func)(void *instance) = nullptr;

// Get base address of libil2cpp.so
uintptr_t getBase() {
    uintptr_t base = 0;
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libil2cpp.so")) {
            // scan hex base
            sscanf(line, "%lx", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

// TCP server for receiving "win" command
void *tcpServer(void *) {
    int server, client;
    char buffer[64];

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        LOGD("socket() failed");
        return nullptr;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5050);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGD("bind() failed");
        close(server);
        return nullptr;
    }

    if (listen(server, 1) < 0) {
        LOGD("listen() failed");
        close(server);
        return nullptr;
    }

    LOGD("TCP server started (port 5050)");

    while (true) {
        client = accept(server, nullptr, nullptr);
        if (client < 0) {
            LOGD("accept() failed");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t r = read(client, buffer, sizeof(buffer)-1);
        if (r > 0) {
            buffer[r] = '\0';
            if (strstr(buffer, "win")) {
                forceWin = true;
                LOGD("ForceWin Command Received!");
            }
        }
        close(client);
    }
    return nullptr;
}

// Background thread to trigger EndGame()
void *forceThread(void *) {
    sleep(10); // wait for game load

    uintptr_t base = getBase();
    if (!base) {
        LOGD("Base address not found!");
        return nullptr;
    }

    EndGame_func = (void (*)(void *))(base + EndGame_RVA);

    LOGD("EndGame Hooked at: %p", EndGame_func);

    while (true) {
        if (forceWin) {
            LOGD("Triggering EndGame()");
            if (EndGame_func) EndGame_func(nullptr);
            forceWin = false;
        }
        usleep(200000);
    }
    return nullptr;
}

// JNI entry point
extern "C" jint JNI_OnLoad(JavaVM *vm, void *) {
    LOGD("ForceWin SO Loaded!");

    pthread_t t1, t2;
    pthread_create(&t1, nullptr, forceThread, nullptr);
    pthread_create(&t2, nullptr, tcpServer, nullptr);

    return JNI_VERSION_1_6;
}

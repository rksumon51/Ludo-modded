// mod.cpp — ForceWin template for IL2CPP games (uses il2cpp runtime invoke if available)
// Paste this file into your repo (replace placeholders later if needed).
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <android/log.h>
#include <vector>

#define LOG_TAG "MOD_FORCEWIN"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- il2cpp function typedefs (common signatures) ---
typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size); // some versions
typedef void* (*il2cpp_domain_assemblies_t)(void* domain, void*** assemblies, size_t* size); // fallback
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namesp, const char* name);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int args);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);

// Global pointers
static void* il2cpp_handle = nullptr;
static il2cpp_domain_get_t il2cpp_domain_get = nullptr;
static il2cpp_domain_assemblies_t il2cpp_domain_assemblies = nullptr;
static il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
static il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
static il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name = nullptr;
static il2cpp_runtime_invoke_t il2cpp_runtime_invoke = nullptr;

static bool resolved = false;

bool resolve_il2cpp_symbols() {
    if (resolved) return true;

    // try open libil2cpp
    il2cpp_handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_LOCAL);
    if (!il2cpp_handle) {
        LOGE("dlopen libil2cpp.so failed, trying RTLD_DEFAULT");
        il2cpp_handle = RTLD_DEFAULT;
    } else {
        LOGI("dlopen libil2cpp.so succeeded");
    }

    // Try many known symbol names (there are variations between Unity versions)
    il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(il2cpp_handle, "il2cpp_domain_get");
    if (!il2cpp_domain_get) il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(il2cpp_handle, "domain_get");

    // assembly iteration: different unity versions provide different functions; we try a few common ones
    il2cpp_domain_assemblies = (il2cpp_domain_assemblies_t)dlsym(il2cpp_handle, "il2cpp_domain_assemblies");
    if (!il2cpp_domain_assemblies) {
        // some versions use il2cpp_domain_get_assemblies or domain_get_assemblies etc.
        il2cpp_domain_assemblies = (il2cpp_domain_assemblies_t)dlsym(il2cpp_handle, "il2cpp_domain_get_assemblies");
    }

    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cpp_handle, "il2cpp_assembly_get_image");
    if (!il2cpp_assembly_get_image) il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cpp_handle, "assembly_get_image");

    il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(il2cpp_handle, "il2cpp_class_from_name");
    il2cpp_class_get_method_from_name = (il2cpp_class_get_method_from_name_t)dlsym(il2cpp_handle, "il2cpp_class_get_method_from_name");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_t)dlsym(il2cpp_handle, "il2cpp_runtime_invoke");

    // log findings
    LOGI("resolved il2cpp_domain_get: %p", (void*)il2cpp_domain_get);
    LOGI("resolved il2cpp_domain_assemblies: %p", (void*)il2cpp_domain_assemblies);
    LOGI("resolved il2cpp_assembly_get_image: %p", (void*)il2cpp_assembly_get_image);
    LOGI("resolved il2cpp_class_from_name: %p", (void*)il2cpp_class_from_name);
    LOGI("resolved il2cpp_class_get_method_from_name: %p", (void*)il2cpp_class_get_method_from_name);
    LOGI("resolved il2cpp_runtime_invoke: %p", (void*)il2cpp_runtime_invoke);

    // success if the core functions we need are present
    resolved = (il2cpp_domain_get && il2cpp_assembly_get_image && il2cpp_class_from_name && il2cpp_class_get_method_from_name && il2cpp_runtime_invoke);
    return resolved;
}

// Helper: try to find image by name by iterating assemblies (best effort)
void* find_image_by_name(const char* wanted_name) {
    if (!il2cpp_domain_get) return nullptr;
    void* domain = il2cpp_domain_get();
    if (!domain) return nullptr;

    // different signatures exist; try to call il2cpp_domain_assemblies if it matches our typedef
    if (il2cpp_domain_assemblies) {
        size_t size = 0;
        void** assemblies = nullptr;
        // try to call - the typedef might not match exactly; use LDTRICK: treat it as function that returns pointer to array?
        // Many Il2Cpp versions: void** il2cpp_domain_get_assemblies(void* domain, size_t* size)
        typedef void** (*domain_assemblies_fn)(void*, size_t*);
        domain_assemblies_fn da = (domain_assemblies_fn)il2cpp_domain_assemblies;
        assemblies = da(domain, &size);
        if (assemblies && size > 0) {
            for (size_t i = 0; i < size; ++i) {
                void* asm_ptr = assemblies[i];
                if (!asm_ptr) continue;
                void* image = nullptr;
                // try assembly_get_image
                if (il2cpp_assembly_get_image) {
                    typedef void* (*asm_get_image_fn)(void*);
                    asm_get_image_fn agi = (asm_get_image_fn)il2cpp_assembly_get_image;
                    image = agi(asm_ptr);
                }
                // We can't read image name via function easily; but class_from_name can accept image pointer
                // We'll return the image pointer if not null and let caller try class_from_name with it
                if (image) {
                    // A heuristic: try class_from_name on this image for a known class like "ResultHandler"
                    // but here we simply return it to try class lookup.
                    return image;
                }
            }
        }
    }

    // fallback: if we cannot enumerate, return nullptr
    return nullptr;
}

// Try to find the ResultHandler class and call EndGame()
bool call_ResultHandler_EndGame() {
    if (!resolve_il2cpp_symbols()) {
        LOGE("il2cpp symbols not resolved; cannot call managed methods");
        return false;
    }

    const char* assembly_name = "Assembly-CSharp.dll"; // most user scripts are in Assembly-CSharp
    const char* namespace_name = ""; // your dump showed namespace empty
    const char* class_name = "ResultHandler";
    const char* method_name = "EndGame";

    // Try to find an image pointer (best-effort)
    void* image = find_image_by_name(assembly_name);
    void* klass = nullptr;
    if (image && il2cpp_class_from_name) {
        klass = il2cpp_class_from_name(image, namespace_name, class_name);
        LOGI("class_from_name(image,'%s','%s') -> %p", namespace_name, class_name, klass);
    }

    // If class not found using image pointer, try passing nullptr to class_from_name (some builds allow that)
    if (!klass && il2cpp_class_from_name) {
        klass = il2cpp_class_from_name(nullptr, namespace_name, class_name);
        LOGI("class_from_name(nullptr,'%s','%s') -> %p", namespace_name, class_name, klass);
    }

    if (!klass) {
        LOGE("Could not find class %s.%s", namespace_name, class_name);
        return false;
    }

    if (!il2cpp_class_get_method_from_name) {
        LOGE("il2cpp_class_get_method_from_name not available");
        return false;
    }

    void* method = il2cpp_class_get_method_from_name(klass, method_name, 0);
    LOGI("method '%s' -> %p", method_name, method);
    if (!method) {
        LOGE("Method %s not found in class", method_name);
        return false;
    }

    // invoke
    if (!il2cpp_runtime_invoke) {
        LOGE("il2cpp_runtime_invoke not found");
        return false;
    }

    il2cpp_runtime_invoke(method, nullptr, nullptr, nullptr);
    LOGI("Called %s successfully (requested force)", method_name);
    return true;
}

// TryForceWin wrapper - you can expand to send player index as parameter if needed.
void TryForceWin(int playerIndex) {
    LOGI("TryForceWin called for player %d", playerIndex);
    bool ok = call_ResultHandler_EndGame();
    if (!ok) {
        LOGE("call_ResultHandler_EndGame failed - you may need to provide addresses from dumper");
    }
}

// simple TCP server that listens for "force <n>"
void* server_thread(void*) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("socket() failed");
        return nullptr;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(6006);
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        LOGE("bind() failed");
        close(sock);
        return nullptr;
    }
    if (listen(sock, 1) != 0) {
        LOGE("listen() failed");
        close(sock);
        return nullptr;
    }
    LOGI("Force server listening on 127.0.0.1:6006");
    while (true) {
        int client = accept(sock, nullptr, nullptr);
        if (client < 0) break;
        char buf[256];
        int r = recv(client, buf, sizeof(buf)-1, 0);
        if (r > 0) {
            buf[r] = 0;
            LOGI("Received: %s", buf);
            if (strncmp(buf, "force", 5) == 0) {
                int idx = atoi(buf + 5);
                TryForceWin(idx);
            }
        }
        close(client);
    }
    close(sock);
    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    // start server in background
    pthread_t t;
    if (pthread_create(&t, nullptr, server_thread, nullptr) == 0) {
        LOGI("Started server thread");
    } else {
        LOGE("Failed to start server thread");
    }
}

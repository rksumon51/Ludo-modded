// mod_arm32.cpp  (for armeabi-v7a)
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#define LOG_TAG "libmod_patch32"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ----------------- CONFIG (replace with your values) -----------------
static const char* TARGET_MODULE = "libil2cpp.so";    // change if needed
static const uint32_t RVA_WINNERS_CHECK = 0x1FC954C; // <-- replace with your dump value (example)
static const uint32_t RVA_ENDGAME = 0x1FC9748;       // <-- replace with your dump value (example)

// If true, will call apply_force_win() on load. Set false during testing.
static bool AUTO_APPLY_ON_LOAD = false;
// ----------------------------------------------------------------------

static inline uintptr_t page_start(uintptr_t addr) {
    size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
    return addr & ~(pagesz - 1);
}

static bool make_writable(void* addr, size_t len) {
    uintptr_t start = page_start((uintptr_t)addr);
    size_t pagesz = (size_t)sysconf(_SC_PAGESIZE);
    uintptr_t end = ((uintptr_t)addr + len + pagesz - 1) & ~(pagesz - 1);
    size_t full_len = end - start;
    if (mprotect((void*)start, full_len, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed errno=%d", errno);
        return false;
    }
    return true;
}

static void write_bytes(void* dest, const void* src, size_t size) {
    if (!make_writable(dest, size)) {
        LOGE("make_writable failed for %p", dest);
        return;
    }
    memcpy(dest, src, size);
    __builtin___clear_cache((char*)dest, (char*)dest + size);
}

static void write_int32(uint32_t addr, int32_t val) {
    void* p = (void*)(uintptr_t)addr;
    if (!make_writable(p, sizeof(int32_t))) {
        LOGE("make_writable int failed");
        return;
    }
    *((int32_t*)p) = val;
    __builtin___clear_cache((char*)p, (char*)p + sizeof(int32_t));
}

static uint32_t find_module_base(const char* module_name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) {
        LOGE("open /proc/self/maps failed");
        return 0;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, module_name)) {
            uint32_t start = 0;
            sscanf(line, "%x-%*x %*s %*s %*s %*s", &start);
            fclose(f);
            LOGI("found module %s at 0x%08x", module_name, start);
            return start;
        }
    }
    fclose(f);
    LOGE("module %s not found", module_name);
    return 0;
}

/*
 * IMPORTANT:
 * - Most likely the RVA values point to code sections (functions). You MUST NOT write
 *   directly into code addresses unless you know what you are doing.
 * - Workflow: Use Frida/Jshook to identify the *data* address that holds winnersCount or winners[].
 * - Once you know a stable data address (module_base + offset), write that address below.
 */
static void apply_force_win(uint32_t module_base) {
    LOGI("apply_force_win: module_base=0x%08x", module_base);

    // ---------- REPLACE THIS with real data address (module_base + offset) ----------
    // Example placeholder: suppose winners_count is at module_base + 0x00ABCDEF
    // uint32_t winners_count_addr = module_base + 0x00ABCDEF;
    // write_int32(winners_count_addr, 4); // set winners count = 4
    // LOGI("Wrote 4 to winners_count at 0x%08x", winners_count_addr);
    // ------------------------------------------------------------------------------

    uint32_t demo_addr = module_base + RVA_WINNERS_CHECK;
    LOGI("Demo address (likely code): 0x%08x — DO NOT WRITE HERE unless it's data", demo_addr);
    LOGI("apply_force_win finished (demo). Replace with actual data write for real effect.");
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("libmod_patch32 JNI_OnLoad");

    uint32_t base = find_module_base(TARGET_MODULE);
    if (!base) {
        LOGE("module base not found - cannot proceed");
        return JNI_VERSION_1_6;
    }

    if (AUTO_APPLY_ON_LOAD) {
        LOGI("AUTO_APPLY_ON_LOAD enabled -> applying");
        apply_force_win(base);
    } else {
        LOGI("AUTO_APPLY_ON_LOAD disabled (safe mode)");
    }
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_mod_NativeBridge_triggerForceWin(JNIEnv* env, jclass clazz) {
    (void)env; (void)clazz;
    LOGI("triggerForceWin called");
    uint32_t base = find_module_base(TARGET_MODULE);
    if (!base) {
        LOGE("module base not found in trigger");
        return;
    }
    apply_force_win(base);
}

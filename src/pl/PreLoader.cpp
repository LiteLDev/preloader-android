//
// Created by mrjar on 10/5/2025.
//

#include <android/native_activity.h>
#include <dlfcn.h>
#include <jni.h>

#include <mutex>
#include <string>
#include <vector>

#include "pl/Logger.h"
#include "pl/Mod.h"
#include "pl/PreloaderInput.h"
#include "pl/internal/AndroidUtils.h"
#include "pl/internal/ModManager.h"

namespace {

auto &logger = preloader_logger;
constexpr const char *kEmptyPath = "<empty>";

JavaVM *g_vm = nullptr;

std::string g_modsDir;
std::string g_cacheDir;
std::vector<PreloaderInput_OnTouch_Fn> g_touchCallbacks;
std::mutex g_callbackMutex;
bool g_modsInitialized = false;

void (*g_onCreate)(ANativeActivity *, void *, size_t) = nullptr;
void (*g_onFinish)(ANativeActivity *) = nullptr;
void (*g_androidMain)(struct android_app *) = nullptr;

void RegisterTouchCallback(PreloaderInput_OnTouch_Fn callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_touchCallbacks.push_back(callback);
    logger.debug("Registered touch callback");
}

PreloaderInput_Interface g_inputInterface = {
        .RegisterTouchCallback = RegisterTouchCallback
};

bool ResolveMinecraftEntryPoints(void *handle) {
    g_onCreate = reinterpret_cast<decltype(g_onCreate)>(
        dlsym(handle, "ANativeActivity_onCreate"));
    g_onFinish = reinterpret_cast<decltype(g_onFinish)>(
        dlsym(handle, "ANativeActivity_finish"));
    g_androidMain = reinterpret_cast<decltype(g_androidMain)>(
        dlsym(handle, "android_main"));

    if (!g_onCreate || !g_androidMain) {
        logger.error("Failed to resolve required symbols");
        return false;
    }

    logger.debug("Successfully loaded Minecraft native functions");
    return true;
}

void InitializeEnabledMods() {
    if (g_modsInitialized || g_modsDir.empty()) {
        if (g_modsDir.empty()) {
            logger.warn("Skipping mod initialization because modsDir is empty");
        }
        return;
    }

    logger.info("Initializing enabled mods from {} using cache {}",
                g_modsDir.empty() ? kEmptyPath : g_modsDir,
                g_cacheDir.empty() ? kEmptyPath : g_cacheDir);
    ModManager::LoadAndInitializeEnabledMods(g_modsDir, g_cacheDir, g_vm);
    g_modsInitialized = true;
    logger.debug("Mods initialized successfully");
}

bool BootstrapMinecraftLibrary(JNIEnv *env, jstring libPath) {
    const char *path = env->GetStringUTFChars(libPath, nullptr);
    if (!path) {
        logger.error("Failed to access library path");
        return false;
    }

    logger.info("Bootstrapping Minecraft library from {}", path);
    void *handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        logger.error("Failed to load library {}: {}", path, dlerror());
        env->ReleaseStringUTFChars(libPath, path);
        return false;
    }

    const bool resolved = ResolveMinecraftEntryPoints(handle);
    env->ReleaseStringUTFChars(libPath, path);

    if (resolved) {
        InitializeEnabledMods();
    }

    return resolved;
}

} // namespace

extern "C" {

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity,
                                        void *savedState,
                                        size_t savedStateSize) {
    if (g_onCreate) {
        g_onCreate(activity, savedState, savedStateSize);
    } else {
        logger.error("ANativeActivity_onCreate function not loaded");
    }
}

JNIEXPORT void ANativeActivity_finish(ANativeActivity *activity) {
    if (g_onFinish) {
        g_onFinish(activity);
    }
}

JNIEXPORT void android_main(struct android_app *state) {
    if (g_androidMain) {
        g_androidMain(state);
    } else {
        logger.error("android_main function not loaded");
    }
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void) reserved;

    g_vm = vm;
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) != JNI_OK) {
        return JNI_VERSION_1_4;
    }

    auto paths = AndroidUtils::FetchContextPaths(env);
    g_modsDir = std::move(paths.modsDir);
    g_cacheDir = std::move(paths.cacheDir);
    logger.info("JNI_OnLoad paths modsDir={} cacheDir={}",
                g_modsDir.empty() ? kEmptyPath : g_modsDir,
                g_cacheDir.empty() ? kEmptyPath : g_cacheDir);

    return JNI_VERSION_1_4;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_minecraft_MinecraftActivity_nativeOnLauncherLoaded(
        JNIEnv *env,
        jobject thiz,
        jstring libPath) {
    (void) thiz;
    return BootstrapMinecraftLibrary(env, libPath) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_minecraft_MinecraftLauncher_nativeOnLauncherLoaded(
        JNIEnv *env,
        jobject thiz,
        jstring libPath) {
    (void) thiz;
    return BootstrapMinecraftLibrary(env, libPath) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeOnLaunched(
        JNIEnv *env,
        jclass clazz,
        jstring libPath) {
    (void) clazz;
    return BootstrapMinecraftLibrary(env, libPath) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod(
        JNIEnv *env,
        jclass clazz,
        jstring libPath,
        jobject modObj) {
    (void) clazz;
    (void) modObj;

    if (!g_vm) {
        logger.error("JavaVM is not initialized");
        return JNI_FALSE;
    }

    const char *path = env->GetStringUTFChars(libPath, nullptr);
    if (!path) {
        logger.error("Failed to access mod library path");
        return JNI_FALSE;
    }

    const bool loaded = ModManager::LoadModLibrary(path, g_vm);
    env->ReleaseStringUTFChars(libPath, path);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
        JNIEnv *env,
        jclass clazz,
        jint action,
        jint pointerId,
        jfloat x,
        jfloat y) {
    (void) env;
    (void) clazz;

    std::lock_guard<std::mutex> lock(g_callbackMutex);
    bool consumed = false;
    for (auto callback : g_touchCallbacks) {
        if (callback) {
            consumed |= callback(action, pointerId, x, y);
        }
    }
    return consumed ? JNI_TRUE : JNI_FALSE;
}

PLAPI PreloaderInput_Interface *GetPreloaderInput() {
    return &g_inputInterface;
}

} // extern "C"

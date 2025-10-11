//
// Created by mrjar on 10/5/2025.
//

#include <jni.h>
#include <android/native_activity.h>
#include <dlfcn.h>
#include <android/log.h>
#include "internal/AndroidUtils.h"
#include "internal/ModManager.h"
#include "Logger.h"

pl::log::Logger logger("NativeLoader");

JavaVM *g_vm = nullptr;

static std::string g_modsDir;
static std::string g_cacheDir;
static bool g_modsInitialized = false;

static void (*onCreate)(ANativeActivity*, void*, size_t) = nullptr;
static void (*onFinish)(ANativeActivity*) = nullptr;
static void (*androidMain)(struct android_app*) = nullptr;

extern "C" {


JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    if (onCreate) {
        onCreate(activity, savedState, savedStateSize);
    } else {
        logger.error("ANativeActivity_onCreate function not loaded");
    }
}

JNIEXPORT void ANativeActivity_finish(ANativeActivity* activity) {
    if (onFinish) {
        onFinish(activity);
    }
}

JNIEXPORT void android_main(struct android_app* state) {
    if (androidMain) {
        androidMain(state);
    } else {
        logger.error("android_main function not loaded");
    }
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_vm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4) != JNI_OK)
        return JNI_VERSION_1_4;

    auto paths = AndroidUtils::FetchContextPaths(env);
    g_modsDir = paths.modsDir;
    g_cacheDir = paths.cacheDir;

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_minecraft_MinecraftActivity_nativeOnLauncherLoaded(
        JNIEnv* env,
        jobject thiz,
        jstring libPath
) {
    const char* path = env->GetStringUTFChars(libPath, nullptr);

    void* handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        logger.error("Failed to load library: %s", dlerror());
        env->ReleaseStringUTFChars(libPath, path);
        return;
    }

    onCreate = reinterpret_cast<decltype(onCreate)>(dlsym(handle, "ANativeActivity_onCreate"));
    onFinish = reinterpret_cast<decltype(onFinish)>(dlsym(handle, "ANativeActivity_finish"));
    androidMain = reinterpret_cast<decltype(androidMain)>(dlsym(handle, "android_main"));

    if (!onCreate || !androidMain) {
        logger.error("Failed to resolve required symbols");
    } else {
        logger.debug("Successfully loaded Minecraft native functions");
    }
    env->ReleaseStringUTFChars(libPath, path);
    if (!g_modsInitialized && !g_modsDir.empty()) {
        ModManager::LoadAndInitializeEnabledMods(g_modsDir, g_cacheDir, g_vm);
        g_modsInitialized = true;
        logger.debug("Mods initialized successfully");
    }
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_minecraft_MinecraftLauncher_nativeOnLauncherLoaded(
        JNIEnv* env,
        jobject thiz,
        jstring libPath
) {


    Java_org_levimc_launcher_core_minecraft_MinecraftActivity_nativeOnLauncherLoaded(env, thiz, libPath);
}

}
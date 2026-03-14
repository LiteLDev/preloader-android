//
// Created by mrjar on 10/5/2025.
//

#include "PreLoader.h"

JavaVM *g_vm = nullptr;

static std::string g_modsDir;
static std::string g_cacheDir;
static std::vector<PreloaderInput_OnTouch_Fn> g_touchCallbacks;
static std::mutex g_callbackMutex;
static bool isLoaded = false;

static void (*onCreate)(ANativeActivity*, void*, size_t) = nullptr;
static void (*onFinish)(ANativeActivity*) = nullptr;
static void (*androidMain)(struct android_app*) = nullptr;

std::string getModsDir() {
    if(isLoaded) return g_modsDir;
    return "";
}

std::string getCacheDir() {
    if(isLoaded) return g_cacheDir;
    return "";
}

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
    
    isLoaded = true;

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeOnLaunched(
        JNIEnv* env,
        jclass clazz,
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
}

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod(JNIEnv* env, jclass clazz, jstring libPath, jobject modObj) {
    Mod mod(env, modObj);
    const char* path = env->GetStringUTFChars(libPath, nullptr);
    if (void *handle = dlopen(path, RTLD_NOW)) {
      LoadFunc func = (LoadFunc)dlsym(handle, "LeviMod_Load");
      if (func) {
        func(g_vm, mod);
      }
    } else {
        logger.error("failed to load mod: %s", path);
        return JNI_FALSE;
    }
    env->ReleaseStringUTFChars(libPath, path);
    logger.info("successful loading mod: %s", path);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(JNIEnv* env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y) {

    std::lock_guard<std::mutex> lock(g_callbackMutex);
    bool consumed = false;
    for (auto callback : g_touchCallbacks) {
        if (callback) {
            consumed |= callback(action, pointerId, x, y);
        }
    }
    return consumed ? JNI_TRUE : JNI_FALSE;
}

static void RegisterTouchCallback(PreloaderInput_OnTouch_Fn callback) {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_touchCallbacks.push_back(callback);
    logger.debug("Registered touch callback");
}

static PreloaderInput_Interface g_inputInterface = {
        .RegisterTouchCallback = RegisterTouchCallback
};

PLAPI PreloaderInput_Interface* GetPreloaderInput() {
    return &g_inputInterface;
}

}
#pragma once

#include <jni.h>
#include <vector>
#include <mutex>

#include <android/native_activity.h>
#include <dlfcn.h>

#include "pl/api/Macro.h"
#include "pl/Mod.h"
#include "pl/internal/AndroidUtils.h"
#include "pl/Logger.h"
#include "pl/api/memory/Hook.h"

using LoadFunc = void (*)(JavaVM*, Mod);

typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId, float x, float y);

struct PreloaderInput_Interface {
    void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
};

extern "C" {
    JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
        JNIEnv* env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y);

    PLAPI PreloaderInput_Interface* GetPreloaderInput();
}

std::string getCacheDir();
std::string getModsDir();
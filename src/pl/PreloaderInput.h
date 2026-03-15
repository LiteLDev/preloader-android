#pragma once

#include <jni.h>
#include <mutex>
#include <vector>

#include "pl/api/Macro.h"

typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId, float x, float y);

struct PreloaderInput_Interface {
    void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
};

extern "C" {
    JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
        JNIEnv* env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y);

    PLAPI PreloaderInput_Interface* GetPreloaderInput();
}

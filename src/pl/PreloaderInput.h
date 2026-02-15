#pragma once

#include <jni.h>
#include <vector>
#include <mutex>

typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId, float x, float y);

struct PreloaderInput_Interface {
    void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
};

extern "C" {
    JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
        JNIEnv* env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y);

    __attribute__((visibility("default"))) PreloaderInput_Interface* GetPreloaderInput();
}

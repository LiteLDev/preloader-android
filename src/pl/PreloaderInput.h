#pragma once

#include <jni.h>
#include <mutex>
#include <vector>

#include "pl/api/Macro.h"

typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId, float x, float y);
typedef bool (*PreloaderInput_OnKeyChar_Fn)(unsigned int unicodeChar);
typedef bool (*PreloaderInput_OnKeyDown_Fn)(int keyCode);

struct PreloaderInput_Interface {
    void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
    void (*RegisterKeyCharCallback)(PreloaderInput_OnKeyChar_Fn callback);
    void (*RegisterKeyDownCallback)(PreloaderInput_OnKeyDown_Fn callback);
    void (*ShowKeyboard)();
    void (*HideKeyboard)();
};

extern "C" {
JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
        JNIEnv* env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y);

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnKeyChar(
        JNIEnv* env, jclass clazz, jint unicodeChar);

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnKeyDown(
        JNIEnv* env, jclass clazz, jint keyCode);

JNIEXPORT void JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeSetActivity(
        JNIEnv* env, jclass clazz, jobject activity);

JNIEXPORT void JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeClearActivity(
        JNIEnv* env, jclass clazz);

PLAPI PreloaderInput_Interface* GetPreloaderInput();
}

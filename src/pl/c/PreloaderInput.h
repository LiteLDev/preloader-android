#pragma once

#include <jni.h>
#include <stdbool.h>

#include "pl/c/Macro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*PreloaderInput_OnTouch_Fn)(int action, int pointerId, float x,
                                          float y);
typedef bool (*PreloaderInput_OnKeyEvent_Fn)(int keyCode,
                                             unsigned int unicodeChar,
                                             bool isKeyDown);

typedef bool (*PreloaderInput_OnMouse_Fn)(int button, bool isDown);

typedef struct PreloaderInput_Interface {
  void (*RegisterTouchCallback)(PreloaderInput_OnTouch_Fn callback);
  void (*RegisterKeyEventCallback)(PreloaderInput_OnKeyEvent_Fn callback);
  void (*ShowKeyboard)(void);
  void (*HideKeyboard)(void);
  void (*RegisterMouseCallback)(PreloaderInput_OnMouse_Fn callback);
} PreloaderInput_Interface;

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
    JNIEnv *env, jclass clazz, jint action, jint pointerId, jfloat x, jfloat y);

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnKeyEvent(
    JNIEnv *env, jclass clazz, jint keyCode, jint unicodeChar,
    jboolean isKeyDown);

JNIEXPORT jboolean JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnMouse(
    JNIEnv *env, jclass clazz, jint button, jboolean isDown);

JNIEXPORT void JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeSetActivity(
    JNIEnv *env, jclass clazz, jobject activity);

JNIEXPORT void JNICALL Java_org_levimc_launcher_preloader_PreloaderInput_nativeClearActivity(
    JNIEnv *env, jclass clazz);

PLAPI PreloaderInput_Interface *GetPreloaderInput(void);

#ifdef __cplusplus
} // extern "C"
#endif


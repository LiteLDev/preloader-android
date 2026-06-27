#include <jni.h>

#include "pl/c/PreloaderInput.h"
#include "pl/internal/ModManager.h"
#include "pl/Logger.h"
#include "pl/runtime/InputBridge.h"
#include "pl/runtime/JavaRuntime.h"

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;

  pl::runtime::SetJavaVm(vm);
  return JNI_VERSION_1_4;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod(JNIEnv *env,
                                                            jclass clazz,
                                                            jstring libPath,
                                                            jobject modObj) {
  (void)clazz;
  (void)modObj;

  JavaVM *vm = pl::runtime::GetJavaVm();
  if (!vm) {
    preloader_logger.error("JavaVM is not initialized");
    return JNI_FALSE;
  }

  const char *path = env->GetStringUTFChars(libPath, nullptr);
  if (!path) {
    preloader_logger.error("Failed to access mod library path");
    return JNI_FALSE;
  }

  const bool loaded = ModManager::LoadModLibrary(path, vm);
  env->ReleaseStringUTFChars(libPath, path);
  return loaded ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
    JNIEnv *env, jclass clazz, jint action, jint pointerId, jfloat x,
    jfloat y) {
  (void)env;
  (void)clazz;

  const bool consumed =
      pl::runtime::DispatchTouch(action, pointerId, x, y);
  return consumed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnKeyEvent(
    JNIEnv *env, jclass clazz, jint keyCode, jint unicodeChar,
    jboolean isKeyDown) {
  (void)env;
  (void)clazz;

  const bool consumed = pl::runtime::DispatchKeyEvent(
      static_cast<int>(keyCode), static_cast<unsigned int>(unicodeChar),
      isKeyDown == JNI_TRUE);
  return consumed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnMouse(
    JNIEnv *env, jclass clazz, jint button, jboolean isDown) {
  (void)env;
  (void)clazz;

  const bool consumed = pl::runtime::DispatchMouse(
      static_cast<int>(button), isDown == JNI_TRUE);
  return consumed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeSetActivity(
    JNIEnv *env, jclass clazz, jobject activity) {
  (void)clazz;

  pl::runtime::SetActivity(env, activity);
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeClearActivity(
    JNIEnv *env, jclass clazz) {
  (void)clazz;

  pl::runtime::ClearActivity(env);
}

PLAPI PreloaderInput_Interface *GetPreloaderInput() {
  return pl::runtime::GetInputInterface();
}

} // extern "C"

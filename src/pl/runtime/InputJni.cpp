#include <jni.h>

#include <string>

#include "pl/c/PreloaderInput.h"
#include "pl/runtime/GameHooks.h"
#include "pl/runtime/InputBridge.h"
#include "pl/runtime/JavaRuntime.h"

namespace {

std::string ToStdString(JNIEnv *env, jstring value) {
  if (!value) {
    return {};
  }

  const char *chars = env->GetStringUTFChars(value, nullptr);
  if (!chars) {
    return {};
  }

  std::string result(chars);
  env->ReleaseStringUTFChars(value, chars);
  return result;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
    JNIEnv *env, jclass clazz, jint action, jint pointerId, jfloat x,
    jfloat y) {
  (void)env;
  (void)clazz;

  const bool consumed = pl::runtime::DispatchTouch(action, pointerId, x, y);
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

  const bool consumed =
      pl::runtime::DispatchMouse(static_cast<int>(button), isDown == JNI_TRUE);
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

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsPauseMenuOpen(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsPauseMenuOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsHudScreenOpen(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsHudScreenOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsShowingMenu(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsShowingMenu() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeShouldForceGlobalModMenu(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::ShouldForceGlobalModMenu() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeConfigureSignatureRules(
    JNIEnv *env, jclass clazz, jstring rulesPath, jstring minecraftVersion) {
  (void)clazz;

  pl::runtime::ConfigureGameHooks(ToStdString(env, rulesPath),
                                  ToStdString(env, minecraftVersion));
}

PLAPI PreloaderInput_Interface *GetPreloaderInput() {
  return pl::runtime::GetInputInterface();
}

} // extern "C"

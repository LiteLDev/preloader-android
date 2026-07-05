#include <jni.h>

#include <filesystem>
#include <optional>

#include "pl/Logger.hpp"
#include "pl/internal/ModManager.h"
#include "pl/runtime/GameHooks.h"
#include "pl/runtime/JavaRuntime.h"
#include "pl/runtime/ModMenuBridge.h"

namespace {

jboolean LoadModFromJava(JNIEnv *env, jstring libPath, jstring modRootPath) {
  JavaVM *vm = pl::runtime::GetJavaVm();
  if (!vm) {
    preloaderLogger.error("JavaVM is not initialized");
    return JNI_FALSE;
  }

  const char *path = env->GetStringUTFChars(libPath, nullptr);
  if (!path) {
    preloaderLogger.error("Failed to access mod library path");
    return JNI_FALSE;
  }

  std::optional<std::filesystem::path> sourceModDirectory;
  const char *sourcePath = nullptr;
  if (modRootPath) {
    sourcePath = env->GetStringUTFChars(modRootPath, nullptr);
    if (!sourcePath) {
      env->ReleaseStringUTFChars(libPath, path);
      preloaderLogger.error("Failed to access original mod root path");
      return JNI_FALSE;
    }
    sourceModDirectory = std::filesystem::path(sourcePath);
  }

  const bool loaded = ModManager::LoadModLibrary(path, sourceModDirectory, vm);

  if (sourcePath) {
    env->ReleaseStringUTFChars(modRootPath, sourcePath);
  }
  env->ReleaseStringUTFChars(libPath, path);
  return loaded ? JNI_TRUE : JNI_FALSE;
}

} // namespace

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;
  pl::runtime::SetJavaVm(vm);
  return JNI_VERSION_1_4;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod__Ljava_lang_String_2Lorg_levimc_launcher_core_mods_Mod_2(
    JNIEnv *env, jclass clazz, jstring libPath, jobject modObj) {
  (void)clazz;
  (void)modObj;
  return LoadModFromJava(env, libPath, nullptr);
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeLoadMod__Ljava_lang_String_2Ljava_lang_String_2Lorg_levimc_launcher_core_mods_Mod_2(
    JNIEnv *env, jclass clazz, jstring libPath, jstring modRootPath,
    jobject modObj) {
  (void)clazz;
  (void)modObj;
  return LoadModFromJava(env, libPath, modRootPath);
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeEnableLoadedMods(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  ModManager::EnableLoadedMods();
  pl::runtime::InitGameHooks();
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_ModManager_nativeDisableAndUnloadLoadedMods(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  ModManager::DisableAndUnloadLoadedMods();
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_minecraft_MinecraftRuntimePreparer_nativeSetupRuntime(
    JNIEnv *env, jclass clazz, jstring modsPath) {
  (void)clazz;
  if (!modsPath) {
    return;
  }

  const char *path = env->GetStringUTFChars(modsPath, nullptr);
  if (!path) {
    return;
  }

  preloaderLogger.debug("Native runtime mod directory: {}", path);
  env->ReleaseStringUTFChars(modsPath, path);
}

} // extern "C"

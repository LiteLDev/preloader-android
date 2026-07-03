#include <jni.h>

#include <atomic>
#include <filesystem>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.h"
#include "pl/c/Hook.h"
#include "pl/c/PreloaderInput.h"
#include "pl/c/PreloaderModMenu.h"
#include "pl/c/Signature.h"
#include "pl/cpp/Signature.hpp"
#include "pl/internal/ModManager.h"
#include "pl/runtime/InputBridge.h"
#include "pl/runtime/JavaRuntime.h"
#include "pl/runtime/ModMenuBridge.h"

namespace {
jboolean LoadModFromJava(JNIEnv *env, jstring libPath, jstring modRootPath) {
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

  std::optional<std::filesystem::path> sourceModDirectory;
  const char *sourcePath = nullptr;
  if (modRootPath) {
    sourcePath = env->GetStringUTFChars(modRootPath, nullptr);
    if (!sourcePath) {
      env->ReleaseStringUTFChars(libPath, path);
      preloader_logger.error("Failed to access original mod root path");
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

static std::atomic_bool g_isPauseMenuOpen{false};

static void (*orig_PauseMenuDtor)(void *) = nullptr;
static void hook_PauseMenuDtor(void *_this) {
  g_isPauseMenuOpen.store(false, std::memory_order_relaxed);
  if (orig_PauseMenuDtor)
    orig_PauseMenuDtor(_this);
}

static void (*orig_PauseMenuOpen)(void *) = nullptr;
static void hook_PauseMenuOpen(void *_this) {
  g_isPauseMenuOpen.store(true, std::memory_order_relaxed);
  if (orig_PauseMenuOpen)
    orig_PauseMenuOpen(_this);
}

static std::atomic_bool g_isHudScreenOpen{false};

static void (*orig_HudScreenDtor)(void *) = nullptr;
static void hook_HudScreenDtor(void *_this) {
  g_isHudScreenOpen.store(false, std::memory_order_relaxed);
  if (orig_HudScreenDtor)
    orig_HudScreenDtor(_this);
}

static void (*orig_HudScreenOpen)(void *) = nullptr;
static void hook_HudScreenOpen(void *_this) {
  g_isHudScreenOpen.store(true, std::memory_order_relaxed);
  if (orig_HudScreenOpen)
    orig_HudScreenOpen(_this);
}

static std::atomic_bool g_isShowingMenu{false};
static bool (*orig_isShowingMenu)(void *) = nullptr;
static bool hook_isShowingMenu(void *_this) {
  bool res = false;
  if (orig_isShowingMenu) {
    res = orig_isShowingMenu(_this);
  }
  g_isShowingMenu.store(res, std::memory_order_relaxed);
  return res;
}

static std::once_flag g_gameHooksOnce;

static void InitGameHooks() {
  std::call_once(g_gameHooksOnce, [] {
    const char *pauseDtorSig =
        "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D5 F3 03 00 AA ? ? "
        "? F9 ? ? ? F8 ? ? ? ? ? ? ? 91 ? ? ? F9 ? ? ? F9 ? ? ? 91 ? ? ? F9 ? "
        "? ? B4 ? ? ? 94 ? ? ? F9 ? ? ? B4 ? ? ? F9 F4 03 00 AA ? ? ? F9 ? ? ? "
        "B4 ? ? ? F9 ? ? ? F9 ? ? ? F9 00 01 3F D6 ? ? ? 91 ? ? ? 92 ? ? ? 97 "
        "? ? ? B5 ? ? ? F9 E0 03 14 AA ? ? ? F9 00 01 3F D6 E0 03 14 AA ? ? ? "
        "94 ? ? ? F9 ? ? ? B4 ? ? ? 94 ? ? ? F9";
    const char *pauseOpenSig =
        "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? "
        "91 ? ? ? D5 F4 03 00 AA ? ? ? 52 ? ? ? F9 ? ? ? F8 ? ? ? F9";
    const char *hudDtorSig =
        "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D5 F3 03 "
        "00 AA ? ? ? F9 ? ? ? F8 ? ? ? ? ? ? ? 91 ? ? ? F9 ? ? ? 91 ? ? ? F9";
    const char *hudOpenSig =
        "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? "
        "91 ? ? ? D5 ? ? ? 52 F3 03 00 AA ? ? ? F9 ? ? ? 72 ? ? ? F8 ? ? ? 97 "
        "? ? ? F9";
    const char *isShowingMenuSig =
        "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? 91 ? ? ? D5 F3 03 00 AA ? ? "
        "? F9 ? ? ? F8 ? ? ? F9 ? ? ? 38 ? ? ? F9 E8 03 00 91 20 01 3F D6 ? ? "
        "? F9 ? ? ? B5 ? ? ? ? ? ? ? 91 ? ? ? ? ? ? ? 91 ? ? ? ? ? ? ? 91 ? ? "
        "? ? ? ? ? 91 ? ? ? 52 ? ? ? 95";

    auto results = pl::signature::resolveSignatures(
        {pauseDtorSig, pauseOpenSig, hudDtorSig, hudOpenSig, isShowingMenuSig},
        "libminecraftpe.so");

    uintptr_t pauseDtor = results[pauseDtorSig];
    if (pauseDtor) {
      pl_hook((PLFuncPtr)pauseDtor, (PLFuncPtr)hook_PauseMenuDtor,
              (PLFuncPtr *)&orig_PauseMenuDtor, PL_HOOK_PRIORITY_NORMAL);
    }

    uintptr_t pauseOpen = results[pauseOpenSig];
    if (pauseOpen) {
      pl_hook((PLFuncPtr)pauseOpen, (PLFuncPtr)hook_PauseMenuOpen,
              (PLFuncPtr *)&orig_PauseMenuOpen, PL_HOOK_PRIORITY_NORMAL);
    }

    uintptr_t hudDtor = results[hudDtorSig];
    if (hudDtor) {
      pl_hook((PLFuncPtr)hudDtor, (PLFuncPtr)hook_HudScreenDtor,
              (PLFuncPtr *)&orig_HudScreenDtor, PL_HOOK_PRIORITY_NORMAL);
    }

    uintptr_t hudOpen = results[hudOpenSig];
    if (hudOpen) {
      pl_hook((PLFuncPtr)hudOpen, (PLFuncPtr)hook_HudScreenOpen,
              (PLFuncPtr *)&orig_HudScreenOpen, PL_HOOK_PRIORITY_NORMAL);
    }

    uintptr_t isShowingMenuAddr = results[isShowingMenuSig];
    if (isShowingMenuAddr) {
      pl_hook((PLFuncPtr)isShowingMenuAddr, (PLFuncPtr)hook_isShowingMenu,
              (PLFuncPtr *)&orig_isShowingMenu, PL_HOOK_PRIORITY_NORMAL);
    }
  });
}

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
  InitGameHooks();
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

  if (!modsPath)
    return;

  const char *path = env->GetStringUTFChars(modsPath, nullptr);
  if (!path)
    return;

  preloader_logger.debug("Native runtime mod directory: {}", path);
  env->ReleaseStringUTFChars(modsPath, path);
}

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
  return g_isPauseMenuOpen.load(std::memory_order_relaxed) ? JNI_TRUE
                                                           : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsHudScreenOpen(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return g_isHudScreenOpen.load(std::memory_order_relaxed) ? JNI_TRUE
                                                           : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsShowingMenu(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return g_isShowingMenu.load(std::memory_order_relaxed) ? JNI_TRUE : JNI_FALSE;
}

PLAPI PreloaderInput_Interface *GetPreloaderInput() {
  return pl::runtime::GetInputInterface();
}

PLAPI PLModMenu_Interface *GetPreloaderModMenu() {
  return pl::runtime::GetModMenuInterface();
}

JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalModCount(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::GetRegisteredModuleCount();
}

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalModInfo(
    JNIEnv *env, jclass clazz, jint index) {
  (void)clazz;
  pl::runtime::RegisteredModule mod;
  if (!pl::runtime::GetRegisteredModuleInfo(index, mod))
    return env->NewStringUTF("{}");

  nlohmann::json payload = {
      {"module_id", mod.module_id},
      {"display_name", mod.display_name},
      {"description", mod.description},
      {"mod_id", mod.mod_id},
      {"enabled", mod.enabled},
      {"hide_in_hud_editor", mod.hide_in_hud_editor},
      {"configs", nlohmann::json::array()},
  };
  for (const auto &cfg : mod.configs) {
    payload["configs"].push_back({
        {"key", cfg.key},
        {"display_name", cfg.display_name},
        {"type", static_cast<int>(cfg.type)},
        {"default_value", cfg.default_value},
        {"min_value", cfg.min_value},
        {"max_value", cfg.max_value},
        {"current_value", cfg.current_value},
        {"depends_on", cfg.depends_on},
    });
  }
  const std::string json = payload.dump();
  return env->NewStringUTF(json.c_str());
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeToggleExternalMod(
    JNIEnv *env, jclass clazz, jstring moduleId, jboolean enabled) {
  (void)clazz;
  if (!moduleId)
    return;
  const char *id = env->GetStringUTFChars(moduleId, nullptr);
  if (id) {
    pl::runtime::ToggleRegisteredModule(id, enabled == JNI_TRUE);
    env->ReleaseStringUTFChars(moduleId, id);
  }
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeSetExternalModConfig(
    JNIEnv *env, jclass clazz, jstring moduleId, jstring key, jstring value) {
  (void)clazz;
  if (!moduleId || !key)
    return;
  const char *idStr = env->GetStringUTFChars(moduleId, nullptr);
  const char *keyStr = env->GetStringUTFChars(key, nullptr);
  const char *valStr = value ? env->GetStringUTFChars(value, nullptr) : nullptr;
  if (idStr && keyStr && (!value || valStr)) {
    pl::runtime::SetRegisteredModuleConfig(idStr, keyStr, valStr ? valStr : "");
  }
  if (valStr)
    env->ReleaseStringUTFChars(value, valStr);
  if (keyStr)
    env->ReleaseStringUTFChars(key, keyStr);
  if (idStr)
    env->ReleaseStringUTFChars(moduleId, idStr);
}

JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonCount(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::GetRegisteredButtonCount();
}

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonInfo(
    JNIEnv *env, jclass clazz, jint index) {
  (void)clazz;
  pl::runtime::RegisteredButton button;
  if (!pl::runtime::GetRegisteredButtonInfo(index, button))
    return env->NewStringUTF("{}");

  nlohmann::json payload = {
      {"button_id", button.button_id},
      {"module_id", button.module_id},
      {"display_name", button.display_name},
      {"mod_id", button.mod_id},
      {"label", button.label},
      {"android_key_code", button.android_key_code},
      {"behavior", static_cast<int>(button.behavior)},
      {"default_visible", button.default_visible},
      {"module_enabled", button.module_enabled},
      {"style",
       {{"preset", static_cast<int>(button.style.preset)},
        {"normal_bg_color", button.style.normal_bg_color},
        {"active_bg_color", button.style.active_bg_color},
        {"border_color", button.style.border_color},
        {"text_color", button.style.text_color},
        {"active_text_color", button.style.active_text_color},
        {"width_scale", button.width_scale},
        {"height_scale", button.height_scale}}},
  };
  const std::string json = payload.dump();
  return env->NewStringUTF(json.c_str());
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeDispatchExternalButtonEvent(
    JNIEnv *env, jclass clazz, jstring buttonId, jint event, jfloat value) {
  (void)clazz;
  if (!buttonId)
    return;

  const char *id = env->GetStringUTFChars(buttonId, nullptr);
  if (id) {
    pl::runtime::DispatchRegisteredButtonEvent(
        id, static_cast<PLModMenu_ButtonEvent>(event), value);
    env->ReleaseStringUTFChars(buttonId, id);
  }
}

JNIEXPORT jobjectArray JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetDrawCommands(
    JNIEnv *env, jclass clazz) {
  (void)clazz;
  std::vector<pl::runtime::InternalDrawCommand> cmds;
  pl::runtime::GetDrawCommands(cmds);

  constexpr size_t kRectFieldsPerCommand = 6;
  constexpr jsize kResultFieldCount = 7;
  const size_t commandCount = cmds.size();
  const size_t maxArrayLength =
      static_cast<size_t>(std::numeric_limits<jsize>::max());
  if (commandCount > maxArrayLength ||
      commandCount > maxArrayLength / kRectFieldsPerCommand) {
    preloader_logger.error("Too many draw commands to marshal to Java: {}",
                           commandCount);
    return nullptr;
  }

  const jsize n = static_cast<jsize>(commandCount);
  const jsize rectCount =
      static_cast<jsize>(commandCount * kRectFieldsPerCommand);

  jclass stringClass = env->FindClass("java/lang/String");
  if (!stringClass)
    return nullptr;
  jclass objectClass = env->FindClass("java/lang/Object");
  if (!objectClass) {
    env->DeleteLocalRef(stringClass);
    return nullptr;
  }

  jintArray typesArray = env->NewIntArray(n);
  jfloatArray rectsArray = env->NewFloatArray(rectCount);
  jintArray colorsArray = env->NewIntArray(n);
  jfloatArray sizesArray = env->NewFloatArray(n);
  jobjectArray textsArray = env->NewObjectArray(n, stringClass, nullptr);
  jobjectArray modulesArray = env->NewObjectArray(n, stringClass, nullptr);
  jobjectArray fontsArray = env->NewObjectArray(n, stringClass, nullptr);

  auto cleanupRefs = [&] {
    if (typesArray)
      env->DeleteLocalRef(typesArray);
    if (rectsArray)
      env->DeleteLocalRef(rectsArray);
    if (colorsArray)
      env->DeleteLocalRef(colorsArray);
    if (sizesArray)
      env->DeleteLocalRef(sizesArray);
    if (textsArray)
      env->DeleteLocalRef(textsArray);
    if (modulesArray)
      env->DeleteLocalRef(modulesArray);
    if (fontsArray)
      env->DeleteLocalRef(fontsArray);
    env->DeleteLocalRef(objectClass);
    env->DeleteLocalRef(stringClass);
  };

  if (!typesArray || !rectsArray || !colorsArray || !sizesArray ||
      !textsArray || !modulesArray || !fontsArray) {
    cleanupRefs();
    return nullptr;
  }

  auto setStringElement = [&](jobjectArray array, jsize index,
                              const std::string &value) {
    if (value.empty())
      return true;
    jstring str = env->NewStringUTF(value.c_str());
    if (!str)
      return false;
    env->SetObjectArrayElement(array, index, str);
    env->DeleteLocalRef(str);
    return !env->ExceptionCheck();
  };

  if (commandCount > 0) {
    std::vector<jint> types(commandCount);
    std::vector<jfloat> rects(static_cast<size_t>(rectCount));
    std::vector<jint> colors(commandCount);
    std::vector<jfloat> sizes(commandCount);
    for (size_t i = 0; i < commandCount; ++i) {
      const size_t rectOffset = i * kRectFieldsPerCommand;
      types[i] = static_cast<jint>(cmds[i].type);
      rects[rectOffset + 0] = cmds[i].x;
      rects[rectOffset + 1] = cmds[i].y;
      rects[rectOffset + 2] = cmds[i].w;
      rects[rectOffset + 3] = cmds[i].h;
      rects[rectOffset + 4] = cmds[i].x3;
      rects[rectOffset + 5] = cmds[i].y3;
      colors[i] = static_cast<jint>(cmds[i].color);
      sizes[i] = cmds[i].size;
      const jsize index = static_cast<jsize>(i);
      if (!setStringElement(textsArray, index, cmds[i].text) ||
          !setStringElement(modulesArray, index, cmds[i].module_id) ||
          !setStringElement(fontsArray, index, cmds[i].font_id)) {
        cleanupRefs();
        return nullptr;
      }
    }
    env->SetIntArrayRegion(typesArray, 0, n, types.data());
    env->SetFloatArrayRegion(rectsArray, 0, rectCount, rects.data());
    env->SetIntArrayRegion(colorsArray, 0, n, colors.data());
    env->SetFloatArrayRegion(sizesArray, 0, n, sizes.data());
    if (env->ExceptionCheck()) {
      cleanupRefs();
      return nullptr;
    }
  }

  jobjectArray result =
      env->NewObjectArray(kResultFieldCount, objectClass, nullptr);
  if (!result) {
    cleanupRefs();
    return nullptr;
  }

  env->SetObjectArrayElement(result, 0, typesArray);
  env->SetObjectArrayElement(result, 1, rectsArray);
  env->SetObjectArrayElement(result, 2, colorsArray);
  env->SetObjectArrayElement(result, 3, sizesArray);
  env->SetObjectArrayElement(result, 4, textsArray);
  env->SetObjectArrayElement(result, 5, modulesArray);
  env->SetObjectArrayElement(result, 6, fontsArray);

  if (env->ExceptionCheck()) {
    env->DeleteLocalRef(result);
    cleanupRefs();
    return nullptr;
  }

  cleanupRefs();

  return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetRegisteredFontBytes(
    JNIEnv *env, jclass clazz, jstring fontId) {
  (void)clazz;
  if (!fontId)
    return nullptr;
  const char *idStr = env->GetStringUTFChars(fontId, nullptr);
  if (!idStr)
    return nullptr;

  std::vector<unsigned char> fontData;
  const bool found = pl::runtime::GetRegisteredFontBytes(idStr, fontData);
  env->ReleaseStringUTFChars(fontId, idStr);

  if (!found || fontData.empty())
    return nullptr;

  const size_t maxArrayLength =
      static_cast<size_t>(std::numeric_limits<jsize>::max());
  if (fontData.size() > maxArrayLength) {
    preloader_logger.error(
        "Registered font is too large to marshal to Java: {}", fontData.size());
    return nullptr;
  }

  const jsize byteCount = static_cast<jsize>(fontData.size());
  jbyteArray result = env->NewByteArray(byteCount);
  if (!result)
    return nullptr;
  env->SetByteArrayRegion(result, 0, byteCount,
                          reinterpret_cast<const jbyte *>(fontData.data()));
  if (env->ExceptionCheck())
    return nullptr;
  return result;
}

} // extern "C"

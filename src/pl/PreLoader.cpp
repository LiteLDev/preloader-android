#include <jni.h>

#include "pl/c/PreloaderInput.h"
#include "pl/c/PreloaderModMenu.h"
#include "pl/internal/ModManager.h"
#include "pl/Logger.h"
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

    std::string json = "{";
    auto esc = [](const std::string &s) {
        std::string out;
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else out += c;
        }
        return out;
    };
    json += "\"module_id\":\"" + esc(mod.module_id) + "\"";
    json += ",\"display_name\":\"" + esc(mod.display_name) + "\"";
    json += ",\"description\":\"" + esc(mod.description) + "\"";
    json += ",\"mod_id\":\"" + esc(mod.mod_id) + "\"";
    json += ",\"enabled\":" + std::string(mod.enabled ? "true" : "false");
    json += ",\"configs\":[";
    for (size_t i = 0; i < mod.configs.size(); ++i) {
        const auto &cfg = mod.configs[i];
        if (i > 0) json += ",";
        json += "{\"key\":\"" + esc(cfg.key) + "\"";
        json += ",\"display_name\":\"" + esc(cfg.display_name) + "\"";
        json += ",\"type\":" + std::to_string(static_cast<int>(cfg.type));
        json += ",\"default_value\":\"" + esc(cfg.default_value) + "\"";
        json += ",\"min_value\":\"" + esc(cfg.min_value) + "\"";
        json += ",\"max_value\":\"" + esc(cfg.max_value) + "\"";
        json += ",\"current_value\":\"" + esc(cfg.current_value) + "\"}";
    }
    json += "]}";
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeToggleExternalMod(
        JNIEnv *env, jclass clazz, jstring moduleId, jboolean enabled) {
    (void)clazz;
    const char *id = env->GetStringUTFChars(moduleId, nullptr);
    if (id) {
        pl::runtime::ToggleRegisteredModule(id, enabled == JNI_TRUE);
        env->ReleaseStringUTFChars(moduleId, id);
    }
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeSetExternalModConfig(
        JNIEnv *env, jclass clazz, jstring moduleId, jstring key,
        jstring value) {
    (void)clazz;
    const char *idStr = env->GetStringUTFChars(moduleId, nullptr);
    const char *keyStr = env->GetStringUTFChars(key, nullptr);
    const char *valStr = env->GetStringUTFChars(value, nullptr);
    if (idStr && keyStr && valStr) {
        pl::runtime::SetRegisteredModuleConfig(idStr, keyStr, valStr);
    }
    if (valStr) env->ReleaseStringUTFChars(value, valStr);
    if (keyStr) env->ReleaseStringUTFChars(key, keyStr);
    if (idStr) env->ReleaseStringUTFChars(moduleId, idStr);
}

JNIEXPORT jfloatArray JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetHudState(
        JNIEnv *env, jclass clazz) {
    (void)clazz;
    PLModMenu_HudState state = {};
    pl::runtime::GetHudState(state);

    jfloatArray result = env->NewFloatArray(21);
    if (!result) return nullptr;

    float data[21];
    data[0] = state.posX;
    data[1] = state.posY;
    data[2] = state.posZ;
    data[3] = state.yaw;
    data[4] = state.pitch;
    data[5] = state.velocityX;
    data[6] = state.velocityY;
    data[7] = state.velocityZ;
    data[8] = state.speed;
    data[9] = state.keyW ? 1.0f : 0.0f;
    data[10] = state.keyA ? 1.0f : 0.0f;
    data[11] = state.keyS ? 1.0f : 0.0f;
    data[12] = state.keyD ? 1.0f : 0.0f;
    data[13] = state.keySpace ? 1.0f : 0.0f;
    data[14] = state.keySneak ? 1.0f : 0.0f;
    data[15] = state.lmb ? 1.0f : 0.0f;
    data[16] = state.rmb ? 1.0f : 0.0f;
    data[17] = (float)state.cpsL;
    data[18] = (float)state.cpsR;
    data[19] = (float)state.entityCount;
    data[20] = (float)state.ping;

    env->SetFloatArrayRegion(result, 0, 21, data);
    return result;
}

} // extern "C"

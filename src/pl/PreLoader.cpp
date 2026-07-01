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
    json += ",\"hide_in_hud_editor\":" + std::string(mod.hide_in_hud_editor ? "true" : "false");
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
        json += ",\"current_value\":\"" + esc(cfg.current_value) + "\"";
        json += ",\"depends_on\":\"" + esc(cfg.depends_on) + "\"}";
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

JNIEXPORT jobjectArray JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetDrawCommands(
        JNIEnv *env, jclass clazz) {
    (void)clazz;
    std::vector<pl::runtime::InternalDrawCommand> cmds;
    pl::runtime::GetDrawCommands(cmds);

    size_t n = cmds.size();

    jintArray typesArray = env->NewIntArray(n);
    jfloatArray rectsArray = env->NewFloatArray(n * 6);
    jintArray colorsArray = env->NewIntArray(n);
    jfloatArray sizesArray = env->NewFloatArray(n);
    jobjectArray textsArray = env->NewObjectArray(n, env->FindClass("java/lang/String"), nullptr);
    jobjectArray modulesArray = env->NewObjectArray(n, env->FindClass("java/lang/String"), nullptr);
    jobjectArray fontsArray = env->NewObjectArray(n, env->FindClass("java/lang/String"), nullptr);

    if (n > 0) {
        std::vector<jint> types(n);
        std::vector<jfloat> rects(n * 6);
        std::vector<jint> colors(n);
        std::vector<jfloat> sizes(n);
        for (size_t i = 0; i < n; ++i) {
            types[i] = static_cast<jint>(cmds[i].type);
            rects[i * 6 + 0] = cmds[i].x;
            rects[i * 6 + 1] = cmds[i].y;
            rects[i * 6 + 2] = cmds[i].w;
            rects[i * 6 + 3] = cmds[i].h;
            rects[i * 6 + 4] = cmds[i].x3;
            rects[i * 6 + 5] = cmds[i].y3;
            colors[i] = static_cast<jint>(cmds[i].color);
            sizes[i] = cmds[i].size;
            if (!cmds[i].text.empty()) {
                jstring str = env->NewStringUTF(cmds[i].text.c_str());
                env->SetObjectArrayElement(textsArray, i, str);
                env->DeleteLocalRef(str);
            }
            if (!cmds[i].module_id.empty()) {
                jstring str = env->NewStringUTF(cmds[i].module_id.c_str());
                env->SetObjectArrayElement(modulesArray, i, str);
                env->DeleteLocalRef(str);
            }
            if (!cmds[i].font_id.empty()) {
                jstring str = env->NewStringUTF(cmds[i].font_id.c_str());
                env->SetObjectArrayElement(fontsArray, i, str);
                env->DeleteLocalRef(str);
            }
        }
        env->SetIntArrayRegion(typesArray, 0, n, types.data());
        env->SetFloatArrayRegion(rectsArray, 0, n * 6, rects.data());
        env->SetIntArrayRegion(colorsArray, 0, n, colors.data());
        env->SetFloatArrayRegion(sizesArray, 0, n, sizes.data());
    }

    jobjectArray result = env->NewObjectArray(7, env->FindClass("java/lang/Object"), nullptr);
    env->SetObjectArrayElement(result, 0, typesArray);
    env->SetObjectArrayElement(result, 1, rectsArray);
    env->SetObjectArrayElement(result, 2, colorsArray);
    env->SetObjectArrayElement(result, 3, sizesArray);
    env->SetObjectArrayElement(result, 4, textsArray);
    env->SetObjectArrayElement(result, 5, modulesArray);
    env->SetObjectArrayElement(result, 6, fontsArray);

    env->DeleteLocalRef(typesArray);
    env->DeleteLocalRef(rectsArray);
    env->DeleteLocalRef(colorsArray);
    env->DeleteLocalRef(sizesArray);
    env->DeleteLocalRef(textsArray);
    env->DeleteLocalRef(modulesArray);
    env->DeleteLocalRef(fontsArray);

    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetRegisteredFontBytes(
        JNIEnv *env, jclass clazz, jstring fontId) {
    (void)clazz;
    if (!fontId) return nullptr;
    const char *idStr = env->GetStringUTFChars(fontId, nullptr);
    if (!idStr) return nullptr;
    
    const std::vector<unsigned char>* fontData = pl::runtime::GetRegisteredFontBytes(idStr);
    env->ReleaseStringUTFChars(fontId, idStr);
    
    if (!fontData || fontData->empty()) return nullptr;
    
    jbyteArray result = env->NewByteArray(fontData->size());
    env->SetByteArrayRegion(result, 0, fontData->size(), reinterpret_cast<const jbyte*>(fontData->data()));
    return result;
}

} // extern "C"

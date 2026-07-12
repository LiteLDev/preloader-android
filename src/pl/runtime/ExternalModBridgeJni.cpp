#include <jni.h>

#include <limits>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.hpp"
#include "pl/runtime/ModMenuBridge.h"

extern "C" {

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
    if (!pl::runtime::GetRegisteredModuleInfo(index, mod)) {
        return env->NewStringUTF("{}");
    }

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
if (!moduleId) {
return;
}
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
if (!moduleId || !key) {
return;
}
const char *idStr = env->GetStringUTFChars(moduleId, nullptr);
const char *keyStr = env->GetStringUTFChars(key, nullptr);
const char *valStr = value ? env->GetStringUTFChars(value, nullptr) : nullptr;
if (idStr && keyStr && (!value || valStr)) {
pl::runtime::SetRegisteredModuleConfig(idStr, keyStr, valStr ? valStr : "");
}
if (valStr) {
env->ReleaseStringUTFChars(value, valStr);
}
if (keyStr) {
env->ReleaseStringUTFChars(key, keyStr);
}
if (idStr) {
env->ReleaseStringUTFChars(moduleId, idStr);
}
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
if (!pl::runtime::GetRegisteredButtonInfo(index, button)) {
return env->NewStringUTF("{}");
}

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
        {"has_icon", !button.icon_data.empty()},
        {"icon_format", static_cast<int>(button.icon_format)},
        {"hide_label_when_icon_present", button.hide_label_when_icon_present},
        {"style",
                {{"preset", static_cast<int>(button.style_preset)},
                              {"normal_bg_color", button.normal_bg_color},
                              {"active_bg_color", button.active_bg_color},
                              {"border_color", button.border_color},
                              {"text_color", button.text_color},
                              {"active_text_color", button.active_text_color},
                              {"width_scale", button.width_scale},
                              {"height_scale", button.height_scale}}},
};
const std::string json = payload.dump();
return env->NewStringUTF(json.c_str());
}

JNIEXPORT jbyteArray JNICALL
        Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetExternalButtonIconBytes(
        JNIEnv *env, jclass clazz, jstring buttonId, jint width, jint height, jboolean active) {
(void)clazz;
if (!buttonId) {
return nullptr;
}

const char *id = env->GetStringUTFChars(buttonId, nullptr);
if (!id) {
return nullptr;
}

std::vector<unsigned char> iconData;
const bool found =
        pl::runtime::GetRegisteredButtonIconBytes(id, width, height, active, iconData);
env->ReleaseStringUTFChars(buttonId, id);

if (!found || iconData.empty()) {
return nullptr;
}

const size_t maxArrayLength =
        static_cast<size_t>(std::numeric_limits<jsize>::max());
if (iconData.size() > maxArrayLength) {
preloaderLogger.error("Registered button icon is too large to marshal to "
"Java: {}",
iconData.size());
return nullptr;
}

const jsize byteCount = static_cast<jsize>(iconData.size());
jbyteArray result = env->NewByteArray(byteCount);
if (!result) {
return nullptr;
}
env->SetByteArrayRegion(result, 0, byteCount,
reinterpret_cast<const jbyte *>(iconData.data()));
if (env->ExceptionCheck()) {
return nullptr;
}
return result;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeDispatchExternalButtonEvent(
        JNIEnv *env, jclass clazz, jstring buttonId, jint event, jfloat value) {
(void)clazz;
if (!buttonId) {
return;
}

const char *id = env->GetStringUTFChars(buttonId, nullptr);
if (id) {
pl::runtime::DispatchRegisteredButtonEvent(
        id, static_cast<pl::modmenu::ButtonEvent>(event), value);
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
constexpr jsize kResultFieldCount = 8;
const size_t commandCount = cmds.size();
const size_t maxArrayLength =
        static_cast<size_t>(std::numeric_limits<jsize>::max());
if (commandCount > maxArrayLength ||
commandCount > maxArrayLength / kRectFieldsPerCommand) {
preloaderLogger.error("Too many draw commands to marshal to Java: {}",
commandCount);
return nullptr;
}

const jsize n = static_cast<jsize>(commandCount);
const jsize rectCount =
        static_cast<jsize>(commandCount * kRectFieldsPerCommand);

jclass stringClass = env->FindClass("java/lang/String");
if (!stringClass) {
return nullptr;
}
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
jobjectArray imagesArray = env->NewObjectArray(n, stringClass, nullptr);

auto cleanupRefs = [&] {
    if (typesArray) {
        env->DeleteLocalRef(typesArray);
    }
    if (rectsArray) {
        env->DeleteLocalRef(rectsArray);
    }
    if (colorsArray) {
        env->DeleteLocalRef(colorsArray);
    }
    if (sizesArray) {
        env->DeleteLocalRef(sizesArray);
    }
    if (textsArray) {
        env->DeleteLocalRef(textsArray);
    }
    if (modulesArray) {
        env->DeleteLocalRef(modulesArray);
    }
    if (fontsArray) {
        env->DeleteLocalRef(fontsArray);
    }
    if (imagesArray) {
        env->DeleteLocalRef(imagesArray);
    }
    env->DeleteLocalRef(objectClass);
    env->DeleteLocalRef(stringClass);
};

if (!typesArray || !rectsArray || !colorsArray || !sizesArray ||
!textsArray || !modulesArray || !fontsArray || !imagesArray) {
cleanupRefs();
return nullptr;
}

auto setStringElement = [&](jobjectArray array, jsize index,
                            const std::string &value) {
    if (value.empty()) {
        return true;
    }
    jstring str = env->NewStringUTF(value.c_str());
    if (!str) {
        return false;
    }
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
!setStringElement(fontsArray, index, cmds[i].font_id) ||
!setStringElement(imagesArray, index, cmds[i].image_id)) {
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
env->SetObjectArrayElement(result, 7, imagesArray);

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
if (!fontId) {
return nullptr;
}
const char *idStr = env->GetStringUTFChars(fontId, nullptr);
if (!idStr) {
return nullptr;
}

std::vector<unsigned char> fontData;
const bool found = pl::runtime::GetRegisteredFontBytes(idStr, fontData);
env->ReleaseStringUTFChars(fontId, idStr);

if (!found || fontData.empty()) {
return nullptr;
}

const size_t maxArrayLength =
        static_cast<size_t>(std::numeric_limits<jsize>::max());
if (fontData.size() > maxArrayLength) {
preloaderLogger.error(
"Registered font is too large to marshal to Java: {}", fontData.size());
return nullptr;
}

const jsize byteCount = static_cast<jsize>(fontData.size());
jbyteArray result = env->NewByteArray(byteCount);
if (!result) {
return nullptr;
}
env->SetByteArrayRegion(result, 0, byteCount,
reinterpret_cast<const jbyte *>(fontData.data()));
if (env->ExceptionCheck()) {
return nullptr;
}
return result;
}

JNIEXPORT jobjectArray JNICALL
        Java_org_levimc_launcher_core_mods_inbuilt_ExternalModBridge_nativeGetRegisteredImage(
        JNIEnv *env, jclass clazz, jstring imageId) {
(void)clazz;
if (!imageId) {
return nullptr;
}
const char *idStr = env->GetStringUTFChars(imageId, nullptr);
if (!idStr) {
return nullptr;
}

std::vector<unsigned char> imageData;
int width = 0;
int height = 0;
const bool found =
        pl::runtime::GetRegisteredImageBytes(idStr, imageData, width, height);
env->ReleaseStringUTFChars(imageId, idStr);

if (!found || imageData.empty() || width <= 0 || height <= 0) {
return nullptr;
}

const size_t maxArrayLength =
        static_cast<size_t>(std::numeric_limits<jsize>::max());
if (imageData.size() > maxArrayLength) {
preloaderLogger.error("Registered image is too large to marshal to Java: {}",
imageData.size());
return nullptr;
}

jclass objectClass = env->FindClass("java/lang/Object");
if (!objectClass) {
return nullptr;
}
jobjectArray result = env->NewObjectArray(2, objectClass, nullptr);
env->DeleteLocalRef(objectClass);
if (!result) {
return nullptr;
}

const jsize byteCount = static_cast<jsize>(imageData.size());
jbyteArray bytesArray = env->NewByteArray(byteCount);
if (!bytesArray) {
env->DeleteLocalRef(result);
return nullptr;
}
env->SetByteArrayRegion(bytesArray, 0, byteCount,
reinterpret_cast<const jbyte *>(imageData.data()));
if (env->ExceptionCheck()) {
env->DeleteLocalRef(bytesArray);
env->DeleteLocalRef(result);
return nullptr;
}

jintArray dimensionsArray = env->NewIntArray(2);
if (!dimensionsArray) {
env->DeleteLocalRef(bytesArray);
env->DeleteLocalRef(result);
return nullptr;
}
const jint dimensions[] = {static_cast<jint>(width),
                           static_cast<jint>(height)};
env->SetIntArrayRegion(dimensionsArray, 0, 2, dimensions);
if (env->ExceptionCheck()) {
env->DeleteLocalRef(dimensionsArray);
env->DeleteLocalRef(bytesArray);
env->DeleteLocalRef(result);
return nullptr;
}

env->SetObjectArrayElement(result, 0, bytesArray);
env->SetObjectArrayElement(result, 1, dimensionsArray);
env->DeleteLocalRef(dimensionsArray);
env->DeleteLocalRef(bytesArray);
if (env->ExceptionCheck()) {
env->DeleteLocalRef(result);
return nullptr;
}
return result;
}

} // extern "C"

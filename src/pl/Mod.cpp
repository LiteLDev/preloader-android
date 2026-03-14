#include "pl/Mod.h"

static jclass ModClass;

static jmethodID getFileNameID;
static jmethodID isEnabledID;
static jmethodID getOrderID;
static jmethodID getVersionID;
static jmethodID getDescriptionID;
static jmethodID getIconNameID;
static jmethodID getAuthorID;
static jmethodID getEntryID;
static jmethodID isJSmodID;

static std::string jstringToString(JNIEnv* env, jstring str) {
    const char* chars = env->GetStringUTFChars(str, nullptr);
    std::string result = chars;
    env->ReleaseStringUTFChars(str, chars);
    return result;
}

void Mod::initJNI(JNIEnv* env) {

    jclass localClass = env->FindClass("org/levimc/launcher/core/mods/Mod");
    ModClass = (jclass) env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);

    getFileNameID = env->GetMethodID(ModClass, "getFileName", "()Ljava/lang/String;");
    isEnabledID = env->GetMethodID(ModClass, "isEnabled", "()Z");
    getOrderID = env->GetMethodID(ModClass, "getOrder", "()I");

    getVersionID = env->GetMethodID(ModClass, "getVersion", "()Ljava/lang/String;");
    getDescriptionID = env->GetMethodID(ModClass, "getDescription", "()Ljava/lang/String;");
    getIconNameID = env->GetMethodID(ModClass, "getIconName", "()Ljava/lang/String;");
    getAuthorID = env->GetMethodID(ModClass, "getAuthor", "()Ljava/lang/String;");
    getEntryID = env->GetMethodID(ModClass, "getEntry", "()Ljava/lang/String;");
    isJSmodID = env->GetMethodID(ModClass, "isJSmod", "()Z");
}

Mod::Mod(JNIEnv* env, jobject obj) {

    fileName = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getFileNameID));

    enabled = env->CallBooleanMethod(obj, isEnabledID);

    order = env->CallIntMethod(obj, getOrderID);

    version = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getVersionID));

    description = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getDescriptionID));

    iconName = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getIconNameID));

    author = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getAuthorID));

    entry = jstringToString(env,
        (jstring) env->CallObjectMethod(obj, getEntryID));

    isJS = env->CallBooleanMethod(obj, isJSmodID);
}

std::string Mod::getFileName() const {
    return fileName;
}

bool Mod::isEnabled() const {
    return enabled;
}

int Mod::getOrder() const {
    return order;
}
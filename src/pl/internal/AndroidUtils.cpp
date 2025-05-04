#include "AndroidUtils.h"

jobject AndroidUtils::GetGlobalContext(JNIEnv *env) {
  jclass activity_thread = env->FindClass("android/app/ActivityThread");
  if (!activity_thread)
    return nullptr;
  jmethodID current_activity_thread =
      env->GetStaticMethodID(activity_thread, "currentActivityThread",
                             "()Landroid/app/ActivityThread;");
  if (!current_activity_thread) {
    env->DeleteLocalRef(activity_thread);
    return nullptr;
  }
  jobject at =
      env->CallStaticObjectMethod(activity_thread, current_activity_thread);
  if (!at || env->ExceptionCheck()) {
    env->ExceptionClear();
    env->DeleteLocalRef(activity_thread);
    return nullptr;
  }
  jmethodID get_application = env->GetMethodID(
      activity_thread, "getApplication", "()Landroid/app/Application;");
  jobject context = env->CallObjectMethod(at, get_application);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  env->DeleteLocalRef(activity_thread);
  env->DeleteLocalRef(at);
  return context;
}

std::string AndroidUtils::GetAbsolutePath(JNIEnv *env, jobject file) {
  jclass file_class = env->GetObjectClass(file);
  jmethodID get_abs_path =
      env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
  jstring jstr = (jstring)env->CallObjectMethod(file, get_abs_path);
  std::string result;
  if (env->ExceptionCheck() || !jstr) {
    env->ExceptionClear();
    env->DeleteLocalRef(file_class);
    return result;
  }
  const char *cstr = env->GetStringUTFChars(jstr, nullptr);
  result = cstr;
  env->ReleaseStringUTFChars(jstr, cstr);
  env->DeleteLocalRef(jstr);
  env->DeleteLocalRef(file_class);
  return result;
}

std::string AndroidUtils::GetSelectedModsDir(JNIEnv *env, jobject context) {
  jclass versionManagerClass =
      env->FindClass("org/levimc/launcher/core/versions/VersionManager");
  if (!versionManagerClass)
    return "";
  jmethodID getSelectedModsDirMethod =
      env->GetStaticMethodID(versionManagerClass, "getSelectedModsDir",
                             "(Landroid/content/Context;)Ljava/lang/String;");
  if (!getSelectedModsDirMethod) {
    env->DeleteLocalRef(versionManagerClass);
    return "";
  }
  jstring modsDirPathJstr = (jstring)env->CallStaticObjectMethod(
      versionManagerClass, getSelectedModsDirMethod, context);
  if (!modsDirPathJstr) {
    env->DeleteLocalRef(versionManagerClass);
    return "";
  }
  const char *chars = env->GetStringUTFChars(modsDirPathJstr, nullptr);
  std::string modsDirPath = chars ? chars : "";
  env->ReleaseStringUTFChars(modsDirPathJstr, chars);
  env->DeleteLocalRef(modsDirPathJstr);
  env->DeleteLocalRef(versionManagerClass);
  return modsDirPath;
}

AndroidContextPaths AndroidUtils::FetchContextPaths(JNIEnv *env) {
  AndroidContextPaths paths;
  jobject appContext = GetGlobalContext(env);
  if (!appContext)
    return paths;

  jclass contextClass = env->GetObjectClass(appContext);

  jmethodID get_cache_dir =
      env->GetMethodID(contextClass, "getCacheDir", "()Ljava/io/File;");
  jobject cacheDir = env->CallObjectMethod(appContext, get_cache_dir);
  if (cacheDir) {
    paths.cacheDir = GetAbsolutePath(env, cacheDir);
    env->DeleteLocalRef(cacheDir);
  }

  paths.modsDir = GetSelectedModsDir(env, appContext);

  env->DeleteLocalRef(contextClass);
  env->DeleteLocalRef(appContext);

  return paths;
}
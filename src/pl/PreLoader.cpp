#include "nlohmann/json.hpp"
#include <android/native_activity.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <jni.h>
#include <string>
#include <string_view>

#include "internal/Logger.h"
#include "internal/StringUtils.h"

JNIEnv *env = nullptr;

jobject getGlobalContext(JNIEnv *env) {
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

std::string getAbsolutePath(JNIEnv *env, jobject file) {
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

std::string getSelectedModsDir(JNIEnv *env, jobject context) {
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

namespace pl {
namespace fs = std::filesystem;

static bool endsWithSo(const std::string &filename) {
  if (filename.length() >= 3) {
    return (0 == filename.compare(filename.length() - 3, 3, ".so"));
  }
  return false;
}

static bool isModEnabled(const fs::path &json_path,
                         const std::string &mod_filename) {
  if (!fs::exists(json_path))
    return false;
  std::ifstream json_file(json_path);
  if (!json_file)
    return false;
  nlohmann::json mods_config;
  try {
    json_file >> mods_config;
  } catch (...) {
    return false;
  }
  auto it = mods_config.find(mod_filename);
  if (it != mods_config.end())
    return it.value().get<bool>();
  return false;
}

void loadPreloadNativeMods() {
  jobject appContext = getGlobalContext(env);
  if (!appContext)
    return;
  jclass contextClass = env->GetObjectClass(appContext);
  jmethodID get_files_dir =
      env->GetMethodID(contextClass, "getFilesDir", "()Ljava/io/File;");
  jobject filesDir = env->CallObjectMethod(appContext, get_files_dir);
  std::string dataDir = getAbsolutePath(env, filesDir);
  if (!dataDir.empty() && filesDir)
    env->DeleteLocalRef(filesDir);
  if (contextClass)
    env->DeleteLocalRef(contextClass);

  auto dirStr = getSelectedModsDir(env, appContext);
  if (appContext)
    env->DeleteLocalRef(appContext);
  fs::path modsDir(dirStr);
  fs::path configPath = modsDir / "mods_config.json";

  if (!fs::exists(modsDir))
    return;

  for (const auto &entry : fs::directory_iterator(modsDir)) {
    if (entry.is_regular_file()) {
      auto fname = entry.path().filename().string();
      if (!endsWithSo(fname))
        continue;

      std::string libName =
          pl::utils::u8str2str(entry.path().stem().u8string()) + ".so";
      if (!isModEnabled(configPath, libName)) {
        pl::log::Info("{} is not enabled, skip loading.", libName);
        continue;
      }
      fs::path destPath = fs::path(dataDir) / entry.path().filename();
      try {
        fs::copy_file(entry, destPath, fs::copy_options::overwrite_existing);
        fs::permissions(destPath, fs::perms::owner_all | fs::perms::group_all);
      } catch (std::exception &e) {
        pl::log::Error("File operation error: {}", e.what());
        continue;
      }
      if (void *handle = dlopen(destPath.c_str(), RTLD_NOW)) {
        pl::log::Info("{} Loaded.", libName);
      } else {
        pl::log::Error("Can't load {}", libName);
        pl::log::Error("%s", dlerror());
      }
    }
  }
}

void init() { loadPreloadNativeMods(); }

} // namespace pl

static void (*android_main_minecraft)(struct android_app *app) = nullptr;
static void (*ANativeActivity_onCreate_minecraft)(ANativeActivity *, void *,
                                                  size_t) = nullptr;

extern "C" void android_main(struct android_app *app) {
  if (android_main_minecraft)
    android_main_minecraft(app);
  pl::init();
}

extern "C" void ANativeActivity_onCreate(ANativeActivity *activity,
                                         void *savedState,
                                         size_t savedStateSize) {
  if (ANativeActivity_onCreate_minecraft)
    ANativeActivity_onCreate_minecraft(activity, savedState, savedStateSize);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  void *handle = dlopen("libminecraftpe.so", RTLD_LAZY);
  if (handle) {
    android_main_minecraft =
        (void (*)(struct android_app *))dlsym(handle, "android_main");
    ANativeActivity_onCreate_minecraft =
        (void (*)(ANativeActivity *, void *, size_t))dlsym(
            handle, "ANativeActivity_onCreate");
  }
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) != JNI_OK) {
    return JNI_VERSION_1_4;
  }
  return JNI_VERSION_1_4;
}
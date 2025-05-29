#include "internal/AndroidUtils.h"
#include "internal/ModManager.h"
#include <android/native_activity.h>
#include <dlfcn.h>

JavaVM *g_vm = nullptr;

static void (*ANativeActivity_onCreate_minecraft)(ANativeActivity *, void *,
                                                  size_t) = nullptr;

extern "C" void ANativeActivity_onCreate(ANativeActivity *activity,
                                         void *savedState,
                                         size_t savedStateSize) {
  if (ANativeActivity_onCreate_minecraft) {
    ANativeActivity_onCreate_minecraft(activity, savedState, savedStateSize);
  }
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
  g_vm = vm;
  JNIEnv *env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4) != JNI_OK)
    return JNI_VERSION_1_4;

  auto paths = AndroidUtils::FetchContextPaths(env);
  ModManager::LoadAndInitializeEnabledMods(paths.modsDir, paths.cacheDir, g_vm);

  void *handle = dlopen("libminecraftpe.so", RTLD_LAZY);
  if (handle) {
    ANativeActivity_onCreate_minecraft =
        (void (*)(ANativeActivity *, void *, size_t))dlsym(
            handle, "ANativeActivity_onCreate");
  }
  return JNI_VERSION_1_4;
}
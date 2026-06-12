#include "pl/runtime/JavaRuntime.h"

namespace pl::runtime {
namespace {

JavaVM *g_vm = nullptr;
jobject g_activity = nullptr;

} // namespace

void SetJavaVm(JavaVM *vm) { g_vm = vm; }

JavaVM *GetJavaVm() { return g_vm; }

void SetActivity(JNIEnv *env, jobject activity) {
  ClearActivity(env);
  if (activity) {
    g_activity = env->NewGlobalRef(activity);
  }
}

void ClearActivity(JNIEnv *env) {
  if (g_activity) {
    env->DeleteGlobalRef(g_activity);
    g_activity = nullptr;
  }
}

void CallActivityVoidMethod(const char *methodName) {
  if (!g_vm || !g_activity) {
    return;
  }

  JNIEnv *env = nullptr;
  bool attached = false;
  const jint status =
      g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4);
  if (status == JNI_EDETACHED) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      return;
    }
    attached = true;
  } else if (status != JNI_OK) {
    return;
  }

  jclass cls = env->GetObjectClass(g_activity);
  if (cls) {
    jmethodID mid = env->GetMethodID(cls, methodName, "()V");
    if (mid) {
      env->CallVoidMethod(g_activity, mid);
    }
    env->DeleteLocalRef(cls);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }

  if (attached) {
    g_vm->DetachCurrentThread();
  }
}

} // namespace pl::runtime


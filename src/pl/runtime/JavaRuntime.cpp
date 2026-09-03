#include "pl/runtime/JavaRuntime.h"
#include "pl/Platform.hpp"
#include <string_view>

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


bool SetClipboardTextImpl(std::string_view text) {
  if (!g_vm || !g_activity) return false;

  JNIEnv *env = nullptr;
  bool attached = false;
  const jint status =
      g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4);
  if (status == JNI_EDETACHED) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attached = true;
  } else if (status != JNI_OK) {
    return false;
  }

  bool success = false;
  jclass activityClass = env->GetObjectClass(g_activity);
  jstring serviceName = env->NewStringUTF("clipboard");
  jobject manager = nullptr;
  if (activityClass && serviceName) {
    jmethodID getSystemService = env->GetMethodID(
        activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (getSystemService) manager = env->CallObjectMethod(g_activity, getSystemService, serviceName);
  }

  jclass clipDataClass = env->FindClass("android/content/ClipData");
  jclass clipboardClass = env->FindClass("android/content/ClipboardManager");
  jstring label = env->NewStringUTF("");
  std::string value(text);
  jstring content = env->NewStringUTF(value.c_str());
  jobject clip = nullptr;
  if (manager && clipDataClass && label && content) {
    jmethodID newPlainText = env->GetStaticMethodID(
        clipDataClass, "newPlainText",
        "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (newPlainText) clip = env->CallStaticObjectMethod(clipDataClass, newPlainText, label, content);
  }
  if (manager && clipboardClass && clip) {
    jmethodID setPrimaryClip = env->GetMethodID(
        clipboardClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (setPrimaryClip) {
      env->CallVoidMethod(manager, setPrimaryClip, clip);
      success = !env->ExceptionCheck();
    }
  }

  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    success = false;
  }
  if (clip) env->DeleteLocalRef(clip);
  if (content) env->DeleteLocalRef(content);
  if (label) env->DeleteLocalRef(label);
  if (clipboardClass) env->DeleteLocalRef(clipboardClass);
  if (clipDataClass) env->DeleteLocalRef(clipDataClass);
  if (manager) env->DeleteLocalRef(manager);
  if (serviceName) env->DeleteLocalRef(serviceName);
  if (activityClass) env->DeleteLocalRef(activityClass);
  if (attached) g_vm->DetachCurrentThread();
  return success;
}

bool CallActivityStringMethod(const char *methodName, const std::string &value) {
  if (!g_vm || !g_activity) {
    return false;
  }

  JNIEnv *env = nullptr;
  bool attached = false;
  const jint status =
      g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4);
  if (status == JNI_EDETACHED) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      return false;
    }
    attached = true;
  } else if (status != JNI_OK) {
    return false;
  }

  bool called = false;
  jclass cls = env->GetObjectClass(g_activity);
  if (cls) {
    jmethodID mid = env->GetMethodID(cls, methodName, "(Ljava/lang/String;)V");
    if (mid) {
      jstring argument = env->NewStringUTF(value.c_str());
      if (argument) {
        env->CallVoidMethod(g_activity, mid, argument);
        env->DeleteLocalRef(argument);
        called = !env->ExceptionCheck();
      }
    }
    env->DeleteLocalRef(cls);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    called = false;
  }

  if (attached) {
    g_vm->DetachCurrentThread();
  }
  return called;
}

} // namespace pl::runtime

namespace pl::platform {

bool setClipboardText(std::string_view text) {
  return pl::runtime::SetClipboardTextImpl(text);
}

}

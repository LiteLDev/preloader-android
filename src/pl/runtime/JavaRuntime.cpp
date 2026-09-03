#include "pl/runtime/JavaRuntime.h"
#include "pl/Platform.hpp"
#include <algorithm>
#include <string_view>

namespace pl::runtime {
namespace {

JavaVM *g_vm = nullptr;
jobject g_activity = nullptr;

bool AttachCurrentThread(JNIEnv **env) {
  if (!g_vm || !env) return false;
#if defined(__ANDROID__)
  return g_vm->AttachCurrentThread(env, nullptr) == JNI_OK;
#else
  return g_vm->AttachCurrentThread(reinterpret_cast<void **>(env), nullptr) == JNI_OK;
#endif
}

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
    if (!AttachCurrentThread(&env)) {
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
    if (!AttachCurrentThread(&env)) return false;
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
    if (!AttachCurrentThread(&env)) {
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

pl::platform::HttpResponse HttpGetImpl(std::string_view url, int timeoutMs) {
  pl::platform::HttpResponse result;
  if (!g_vm || url.empty()) return result;
  if (!url.starts_with("https://") && !url.starts_with("http://")) return result;

  JNIEnv *env = nullptr;
  bool attached = false;
  const jint vmStatus = g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_4);
  if (vmStatus == JNI_EDETACHED) {
    if (!AttachCurrentThread(&env)) return result;
    attached = true;
  } else if (vmStatus != JNI_OK) {
    return result;
  }

  if (env->PushLocalFrame(32) < 0) {
    env->ExceptionClear();
    if (attached) g_vm->DetachCurrentThread();
    return result;
  }

  jobject connection = nullptr;
  jobject input = nullptr;
  jmethodID close = nullptr;
  jmethodID disconnect = nullptr;
  auto findClass = [&](const char *name) -> jclass {
    return env->ExceptionCheck() ? nullptr : env->FindClass(name);
  };
  auto method = [&](jclass cls, const char *name, const char *signature) -> jmethodID {
    return !cls || env->ExceptionCheck() ? nullptr : env->GetMethodID(cls, name, signature);
  };
  auto string = [&](const char *value) -> jstring {
    return env->ExceptionCheck() ? nullptr : env->NewStringUTF(value);
  };

  const bool success = [&]() -> bool {
    jclass urlClass = findClass("java/net/URL");
    jclass connectionClass = findClass("java/net/HttpURLConnection");
    jclass inputClass = findClass("java/io/InputStream");
    jclass outputClass = findClass("java/io/ByteArrayOutputStream");
    if (!urlClass || !connectionClass || !inputClass || !outputClass) return false;

    close = method(inputClass, "close", "()V");
    disconnect = method(connectionClass, "disconnect", "()V");
    jmethodID urlCtor = method(urlClass, "<init>", "(Ljava/lang/String;)V");
    jmethodID openConnection = method(urlClass, "openConnection", "()Ljava/net/URLConnection;");
    jmethodID setConnectTimeout = method(connectionClass, "setConnectTimeout", "(I)V");
    jmethodID setReadTimeout = method(connectionClass, "setReadTimeout", "(I)V");
    jmethodID setRequestMethod = method(connectionClass, "setRequestMethod", "(Ljava/lang/String;)V");
    jmethodID setRequestProperty = method(connectionClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID getResponseCode = method(connectionClass, "getResponseCode", "()I");
    jmethodID getHeaderField = method(connectionClass, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
    jmethodID getInputStream = method(connectionClass, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID getErrorStream = method(connectionClass, "getErrorStream", "()Ljava/io/InputStream;");
    jmethodID outputCtor = method(outputClass, "<init>", "()V");
    jmethodID read = method(inputClass, "read", "([B)I");
    jmethodID write = method(outputClass, "write", "([BII)V");
    jmethodID toByteArray = method(outputClass, "toByteArray", "()[B");
    if (!close || !disconnect || !urlCtor || !openConnection || !setConnectTimeout ||
        !setReadTimeout || !setRequestMethod || !setRequestProperty || !getResponseCode ||
        !getHeaderField || !getInputStream || !getErrorStream || !outputCtor ||
        !read || !write || !toByteArray || env->ExceptionCheck()) return false;

    std::string urlString(url);
    jstring jUrl = string(urlString.c_str());
    jstring get = string("GET");
    jstring acceptKey = string("Accept");
    jstring acceptValue = string("application/json");
    jstring agentKey = string("User-Agent");
    jstring agentValue = string("LeviPreloader/1");
    jstring retryKey = string("Retry-After");
    if (!jUrl || !get || !acceptKey || !acceptValue || !agentKey || !agentValue || !retryKey) return false;

    jobject urlObject = env->NewObject(urlClass, urlCtor, jUrl);
    if (env->ExceptionCheck() || !urlObject) return false;
    connection = env->CallObjectMethod(urlObject, openConnection);
    if (env->ExceptionCheck() || !connection) return false;
    const int safeTimeout = std::clamp(timeoutMs <= 0 ? 5000 : timeoutMs, 1000, 30000);
    env->CallVoidMethod(connection, setConnectTimeout, safeTimeout);
    if (env->ExceptionCheck()) return false;
    env->CallVoidMethod(connection, setReadTimeout, safeTimeout);
    if (env->ExceptionCheck()) return false;
    env->CallVoidMethod(connection, setRequestMethod, get);
    if (env->ExceptionCheck()) return false;
    env->CallVoidMethod(connection, setRequestProperty, acceptKey, acceptValue);
    if (env->ExceptionCheck()) return false;
    env->CallVoidMethod(connection, setRequestProperty, agentKey, agentValue);
    if (env->ExceptionCheck()) return false;

    result.status = env->CallIntMethod(connection, getResponseCode);
    if (env->ExceptionCheck()) return false;
    jstring retry = static_cast<jstring>(env->CallObjectMethod(connection, getHeaderField, retryKey));
    if (env->ExceptionCheck()) return false;
    if (retry) {
      const char *chars = env->GetStringUTFChars(retry, nullptr);
      if (env->ExceptionCheck() || !chars) return false;
      result.retryAfter = chars;
      env->ReleaseStringUTFChars(retry, chars);
    }

    input = env->CallObjectMethod(connection, result.status >= 400 ? getErrorStream : getInputStream);
    if (env->ExceptionCheck()) return false;
    if (!input) return true; // HTTP errors can legitimately have no response body.
    jobject output = env->NewObject(outputClass, outputCtor);
    if (env->ExceptionCheck() || !output) return false;
    jbyteArray buffer = env->NewByteArray(8192);
    if (env->ExceptionCheck() || !buffer) return false;

    constexpr std::size_t maxResponseBytes = 8 * 1024 * 1024;
    std::size_t total = 0;
    for (;;) {
      jint count = env->CallIntMethod(input, read, buffer);
      if (env->ExceptionCheck()) return false;
      if (count <= 0) break;
      total += static_cast<std::size_t>(count);
      if (total > maxResponseBytes) return false;
      env->CallVoidMethod(output, write, buffer, 0, count);
      if (env->ExceptionCheck()) return false;
    }
    jbyteArray bytes = static_cast<jbyteArray>(env->CallObjectMethod(output, toByteArray));
    if (env->ExceptionCheck() || !bytes) return false;
    jsize length = env->GetArrayLength(bytes);
    if (length > 0) {
      jbyte *data = env->GetByteArrayElements(bytes, nullptr);
      if (env->ExceptionCheck() || !data) return false;
      result.body.assign(reinterpret_cast<const char *>(data), static_cast<std::size_t>(length));
      env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
    }
    return true;
  }();

  if (env->ExceptionCheck()) env->ExceptionClear();
  if (!success) result = {};
  if (input && close) {
    env->CallVoidMethod(input, close);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  if (connection && disconnect) {
    env->CallVoidMethod(connection, disconnect);
    if (env->ExceptionCheck()) env->ExceptionClear();
  }
  env->PopLocalFrame(nullptr);
  if (attached) g_vm->DetachCurrentThread();
  return result;
}

} // namespace pl::runtime

namespace pl::platform {

bool setClipboardText(std::string_view text) {
  return pl::runtime::SetClipboardTextImpl(text);
}

HttpResponse httpGet(std::string_view url, int timeoutMs) {
  return pl::runtime::HttpGetImpl(url, timeoutMs);
}

}

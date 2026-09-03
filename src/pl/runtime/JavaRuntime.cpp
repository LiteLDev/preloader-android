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

  jobject connection = nullptr;
  jobject input = nullptr;
  jobject output = nullptr;
  jbyteArray buffer = nullptr;
  jclass urlClass = env->FindClass("java/net/URL");
  jclass connectionClass = env->FindClass("java/net/HttpURLConnection");
  jclass inputClass = env->FindClass("java/io/InputStream");
  jclass outputClass = env->FindClass("java/io/ByteArrayOutputStream");
  std::string urlString(url);
  jstring jUrl = env->NewStringUTF(urlString.c_str());

  if (urlClass && connectionClass && inputClass && outputClass && jUrl) {
    jmethodID urlCtor = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
    jobject urlObject = urlCtor ? env->NewObject(urlClass, urlCtor, jUrl) : nullptr;
    if (urlObject) {
      jmethodID openConnection = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
      connection = openConnection ? env->CallObjectMethod(urlObject, openConnection) : nullptr;
      if (connection && !env->ExceptionCheck()) {
        jmethodID setConnectTimeout = env->GetMethodID(connectionClass, "setConnectTimeout", "(I)V");
        jmethodID setReadTimeout = env->GetMethodID(connectionClass, "setReadTimeout", "(I)V");
        jmethodID setRequestMethod = env->GetMethodID(connectionClass, "setRequestMethod", "(Ljava/lang/String;)V");
        jmethodID setRequestProperty = env->GetMethodID(connectionClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
        const int safeTimeout = std::clamp(timeoutMs <= 0 ? 5000 : timeoutMs, 1000, 30000);
        if (setConnectTimeout) env->CallVoidMethod(connection, setConnectTimeout, safeTimeout);
        if (setReadTimeout) env->CallVoidMethod(connection, setReadTimeout, safeTimeout);
        jstring get = env->NewStringUTF("GET");
        if (setRequestMethod && get) env->CallVoidMethod(connection, setRequestMethod, get);
        jstring acceptKey = env->NewStringUTF("Accept");
        jstring acceptValue = env->NewStringUTF("application/json");
        if (setRequestProperty && acceptKey && acceptValue) env->CallVoidMethod(connection, setRequestProperty, acceptKey, acceptValue);
        jstring agentKey = env->NewStringUTF("User-Agent");
        jstring agentValue = env->NewStringUTF("LeviPreloader/1");
        if (setRequestProperty && agentKey && agentValue) env->CallVoidMethod(connection, setRequestProperty, agentKey, agentValue);

        jmethodID getResponseCode = env->GetMethodID(connectionClass, "getResponseCode", "()I");
        if (getResponseCode) result.status = env->CallIntMethod(connection, getResponseCode);
        if (!env->ExceptionCheck()) {
          jmethodID getHeaderField = env->GetMethodID(connectionClass, "getHeaderField", "(Ljava/lang/String;)Ljava/lang/String;");
          jstring retryKey = env->NewStringUTF("Retry-After");
          jstring retry = getHeaderField && retryKey ? static_cast<jstring>(env->CallObjectMethod(connection, getHeaderField, retryKey)) : nullptr;
          if (retry) {
            const char *chars = env->GetStringUTFChars(retry, nullptr);
            if (chars) {
              result.retryAfter = chars;
              env->ReleaseStringUTFChars(retry, chars);
            }
            env->DeleteLocalRef(retry);
          }
          if (retryKey) env->DeleteLocalRef(retryKey);

          jmethodID getStream = env->GetMethodID(connectionClass,
              result.status >= 400 ? "getErrorStream" : "getInputStream", "()Ljava/io/InputStream;");
          input = getStream ? env->CallObjectMethod(connection, getStream) : nullptr;
          if (input && !env->ExceptionCheck()) {
            jmethodID outputCtor = env->GetMethodID(outputClass, "<init>", "()V");
            output = outputCtor ? env->NewObject(outputClass, outputCtor) : nullptr;
            jmethodID read = env->GetMethodID(inputClass, "read", "([B)I");
            jmethodID write = env->GetMethodID(outputClass, "write", "([BII)V");
            jmethodID toByteArray = env->GetMethodID(outputClass, "toByteArray", "()[B");
            buffer = env->NewByteArray(8192);
            if (output && read && write && toByteArray && buffer) {
              constexpr std::size_t maxResponseBytes = 8 * 1024 * 1024;
              std::size_t total = 0;
              bool tooLarge = false;
              for (;;) {
                jint count = env->CallIntMethod(input, read, buffer);
                if (env->ExceptionCheck() || count <= 0) break;
                total += static_cast<std::size_t>(count);
                if (total > maxResponseBytes) {
                  tooLarge = true;
                  break;
                }
                env->CallVoidMethod(output, write, buffer, 0, count);
                if (env->ExceptionCheck()) break;
              }
              if (tooLarge) {
                result.status = 0;
                result.body.clear();
              } else if (!env->ExceptionCheck()) {
                jbyteArray bytes = static_cast<jbyteArray>(env->CallObjectMethod(output, toByteArray));
                if (bytes) {
                  jsize length = env->GetArrayLength(bytes);
                  if (length > 0) {
                    jbyte *data = env->GetByteArrayElements(bytes, nullptr);
                    if (data) {
                      result.body.assign(reinterpret_cast<const char *>(data), static_cast<std::size_t>(length));
                      env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
                    }
                  }
                  env->DeleteLocalRef(bytes);
                }
              }
            }
          }
        }
        if (input) {
          jmethodID close = env->GetMethodID(inputClass, "close", "()V");
          if (close) env->CallVoidMethod(input, close);
        }
        jmethodID disconnect = env->GetMethodID(connectionClass, "disconnect", "()V");
        if (connection && disconnect) env->CallVoidMethod(connection, disconnect);
        if (agentValue) env->DeleteLocalRef(agentValue);
        if (agentKey) env->DeleteLocalRef(agentKey);
        if (acceptValue) env->DeleteLocalRef(acceptValue);
        if (acceptKey) env->DeleteLocalRef(acceptKey);
        if (get) env->DeleteLocalRef(get);
      }
      env->DeleteLocalRef(urlObject);
    }
  }

  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    if (result.status == 0) result.body.clear();
  }
  if (buffer) env->DeleteLocalRef(buffer);
  if (output) env->DeleteLocalRef(output);
  if (input) env->DeleteLocalRef(input);
  if (connection) env->DeleteLocalRef(connection);
  if (jUrl) env->DeleteLocalRef(jUrl);
  if (outputClass) env->DeleteLocalRef(outputClass);
  if (inputClass) env->DeleteLocalRef(inputClass);
  if (connectionClass) env->DeleteLocalRef(connectionClass);
  if (urlClass) env->DeleteLocalRef(urlClass);
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

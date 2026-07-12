#include <jni.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "pl/runtime/GameHooks.h"
#include "pl/runtime/InputBridge.h"
#include "pl/runtime/JavaRuntime.h"

namespace {

struct DecodedText {
  std::string utf8;
  std::vector<unsigned int> codePoints;
};

void AppendUtf8(std::string &result, std::uint32_t codePoint) {
  if (codePoint <= 0x7fU) {
    result.push_back(static_cast<char>(codePoint));
  } else if (codePoint <= 0x7ffU) {
    result.push_back(static_cast<char>(0xc0U | (codePoint >> 6U)));
    result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
  } else if (codePoint <= 0xffffU) {
    result.push_back(static_cast<char>(0xe0U | (codePoint >> 12U)));
    result.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
    result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
  } else {
    result.push_back(static_cast<char>(0xf0U | (codePoint >> 18U)));
    result.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3fU)));
    result.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3fU)));
    result.push_back(static_cast<char>(0x80U | (codePoint & 0x3fU)));
  }
}

DecodedText DecodeText(JNIEnv *env, jstring value) {
  DecodedText result;
  if (value == nullptr) {
    return result;
  }

  const jsize length = env->GetStringLength(value);
  const jchar *chars = env->GetStringChars(value, nullptr);
  if (chars == nullptr) {
    return result;
  }

  result.utf8.reserve(static_cast<std::size_t>(length) * 3U);
  result.codePoints.reserve(static_cast<std::size_t>(length));
  for (jsize i = 0; i < length; ++i) {
    std::uint32_t codePoint = chars[i];
    if (codePoint >= 0xd800U && codePoint <= 0xdbffU && i + 1 < length) {
      const std::uint32_t low = chars[i + 1];
      if (low >= 0xdc00U && low <= 0xdfffU) {
        codePoint = 0x10000U + ((codePoint - 0xd800U) << 10U) + (low - 0xdc00U);
        ++i;
      } else {
        codePoint = 0xfffdU;
      }
    } else if (codePoint >= 0xd800U && codePoint <= 0xdfffU) {
      codePoint = 0xfffdU;
    }

    AppendUtf8(result.utf8, codePoint);
    result.codePoints.push_back(codePoint);
  }

  env->ReleaseStringChars(value, chars);
  return result;
}

std::string ToStdString(JNIEnv *env, jstring value) {
  if (!value) {
    return {};
  }

  const char *chars = env->GetStringUTFChars(value, nullptr);
  if (!chars) {
    return {};
  }

  std::string result(chars);
  env->ReleaseStringUTFChars(value, chars);
  return result;
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTouch(
    JNIEnv *env, jclass clazz, jint action, jint pointerId, jfloat x,
    jfloat y) {
  (void)env;
  (void)clazz;

  const bool consumed = pl::runtime::DispatchTouch(action, pointerId, x, y);
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
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnTextInput(
    JNIEnv *env, jclass clazz, jstring text) {
  (void)clazz;

  auto decoded = DecodeText(env, text);
  bool consumed = pl::runtime::DispatchTextInput(std::move(decoded.utf8));
  if (!consumed) {
    for (const unsigned int codePoint : decoded.codePoints) {
      consumed |= pl::runtime::DispatchKeyEvent(0, codePoint, true);
    }
  }
  return consumed ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeOnMouse(
    JNIEnv *env, jclass clazz, jint button, jboolean isDown) {
  (void)env;
  (void)clazz;

  const bool consumed =
      pl::runtime::DispatchMouse(static_cast<int>(button), isDown == JNI_TRUE);
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

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsPauseMenuOpen(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsPauseMenuOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsHudScreenOpen(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsHudScreenOpen() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeIsShowingMenu(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::IsShowingMenu() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeShouldForceGlobalModMenu(
    JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return pl::runtime::ShouldForceGlobalModMenu() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_preloader_PreloaderInput_nativeConfigureSignatureRules(
    JNIEnv *env, jclass clazz, jstring rulesPath, jstring minecraftVersion) {
  (void)clazz;

  pl::runtime::ConfigureGameHooks(ToStdString(env, rulesPath),
                                  ToStdString(env, minecraftVersion));
}

} // extern "C"

#pragma once

#include <jni.h>
#include <string>
#include <string_view>
#include "pl/Platform.hpp"

namespace pl::runtime {

void SetJavaVm(JavaVM *vm);
JavaVM *GetJavaVm();

void SetActivity(JNIEnv *env, jobject activity);
void ClearActivity(JNIEnv *env);
void CallActivityVoidMethod(const char *methodName);
bool CallActivityStringMethod(const char *methodName, const std::string &value);
pl::platform::HttpResponse HttpGetImpl(std::string_view url, int timeoutMs);

} // namespace pl::runtime

#pragma once

#include <jni.h>
#include <string>

namespace pl::runtime {

void SetJavaVm(JavaVM *vm);
JavaVM *GetJavaVm();

void SetActivity(JNIEnv *env, jobject activity);
void ClearActivity(JNIEnv *env);
void CallActivityVoidMethod(const char *methodName);
bool CallActivityStringMethod(const char *methodName, const std::string &value);

} // namespace pl::runtime

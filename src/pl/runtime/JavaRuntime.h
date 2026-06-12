#pragma once

#include <jni.h>

namespace pl::runtime {

void SetJavaVm(JavaVM *vm);
JavaVM *GetJavaVm();

void SetActivity(JNIEnv *env, jobject activity);
void ClearActivity(JNIEnv *env);
void CallActivityVoidMethod(const char *methodName);

} // namespace pl::runtime


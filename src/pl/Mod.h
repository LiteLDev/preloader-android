#pragma once
#include <jni.h>
#include <string>

class Mod {
private:
    std::string fileName;
    bool enabled;
    int order;

    std::string version;
    std::string description;
    std::string iconName;
    std::string author;
    std::string entry;
    bool isJS;

public:
    static void initJNI(JNIEnv* env);

    Mod(JNIEnv* env, jobject obj);

    std::string getFileName() const;
    bool isEnabled() const;
    int getOrder() const;
};
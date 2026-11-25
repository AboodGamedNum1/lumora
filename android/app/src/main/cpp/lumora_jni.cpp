#include <jni.h>

#include "lumora/version.h"

extern "C" JNIEXPORT jstring JNICALL
Java_dev_lumora_app_NativeBridge_versionString(JNIEnv* env, jobject /*thiz*/) {
  return env->NewStringUTF(lumora::version_string().c_str());
}

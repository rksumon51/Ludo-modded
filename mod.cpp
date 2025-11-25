#include <jni.h>
#include <string>

extern "C" {

// simple test function
jstring Java_com_example_mod_NativeLib_stringFromJNI(JNIEnv* env, jobject /* this */) {
    return env->NewStringUTF("Mod SO Loaded Successfully!");
}

}

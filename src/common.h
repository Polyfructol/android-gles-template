
#include <stdbool.h>

#include <android/log.h> // TODO: Remove? Or #ifdef it

#define ARRAYSIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define OFFSETOF(type, member) __builtin_offsetof(type, member)

#define DEBUG 1
#define LOG_TAG "EmptyApp"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif


#include <stdlib.h> // calloc/free
#include <string.h> // memmove
#include <unistd.h> // chdir
#include <assert.h> // assert

#include <pthread.h>

// Basic Android Lifecycle Recommendations : https://docs.nvidia.com/tegra/Content/AN_LC_Common_Steps_When.html
#include <android/native_window_jni.h> // for native window JNI

#include <glad/egl.h>
#include <glad/gles2.h>

#define PACKAGE_PATH "com/example/app"

#include "common.h"
#include "event.h"

#include "sound_device.h"

#include "game.h"

#include "imgui_test.h"


#include <time.h>
int64_t getNow()
{
    static bool init = false;
    static int64_t startTime = 0;
    if (!init)
    {
        init = true;
        startTime = getNow();
    }

    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return ((t.tv_nsec + (t.tv_sec * 1000000000)) / 1000000) - startTime;
}

typedef struct EGL
{
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
} EGL;

typedef enum EventType
{
    EventType_Create,
    EventType_Destroy,
    EventType_Start,
    EventType_Stop,
    EventType_Resume,
    EventType_Pause,
    EventType_WindowFocusChanged,
    EventType_DispatchKeyEvent,
    EventType_DispatchMotionEvent,
    EventType_SurfaceCreated,
    EventType_SurfaceChanged,
    EventType_SurfaceDestroyed,
} EventType;

const char* eventTypeStr[] =
{
    "Create",
    "Destroy",
    "Start",
    "Stop",
    "Resume",
    "Pause",
    "WindowFocusChanged",
    "DispatchKeyEvent",
    "DispatchTouchEvent",
    "SurfaceCreated",
    "SurfaceChanged",
    "SurfaceDestroyed",
};

typedef struct Config
{
    char filesDir[256];
    int audioOutputSampleRate;
    int audioOutputFramesPerBuffer;
} Config;

typedef struct Event
{
    EventType type;

    union
    {
        struct Create
        {
            Config config;
        } create;

        struct WindowFocusChanged
        {
            bool hasFocus;
        } windowFocusChanged;

        InputEvent dispatchKeyEvent;
        InputEvent dispatchTouchEvent;

        struct SurfaceCreated
        {
            ANativeWindow* nativeWindow;
        } surfaceCreated;

        struct SurfaceChanged
        {
            int format;
            int width;
            int height;
        } surfaceChanged;
    };
} Event;

typedef struct NativeActivityProto
{
    jclass clazz;
    jobject object;
    jmethodID showSoftInput;
    jmethodID hideSoftInput;
    jmethodID vibrate;
} NativeActivityProto;

typedef struct ConfigProto
{
    jclass clazz;
    jfieldID filesDir;
    jfieldID audioOutputSampleRate;
    jfieldID audioOutputFramesPerBuffer;
} ConfigProto;

typedef struct KeyEventProto
{
    jclass clazz;
    jfieldID action;
    jfieldID keyCode;
    jfieldID scanCode;
    jfieldID unicodeChar;
    jfieldID metaState;
} KeyEventProto;

typedef struct MotionEventProto
{
    jclass clazz;
    jfieldID action;
    jfieldID pointerCount;
    jfieldID x;
    jfieldID y;
} MotionEventProto;

typedef struct JavaClasses
{
    NativeActivityProto nativeActivity;
    KeyEventProto keyEvent;
    MotionEventProto motionEvent;
    ConfigProto config;
} JavaClasses;

typedef struct AppThread
{
    pthread_t thread;
    JavaVM* javaVM;
    JNIEnv* jniEnv;
    JavaClasses javaClasses;

    ANativeWindow* nativeWindow;

    // Simplest fifo using a static array (memmove to pop event)
    Event events[64];
    int eventCount;
    pthread_mutex_t mutex;
    pthread_cond_t eventAddedCond;
    pthread_cond_t allEventsProcessed;
} AppThread;

typedef struct App
{
    bool activityPaused;
    bool canRender;

    EGL egl;

    sound_device_t* soundDevice;
    Game* game;
    GameInputs gameInputs;
    ImGuiTest* imguiTest;
} App;

static const char* egl_ErrorString(const EGLint error)
{
    switch (error)
    {
        case EGL_SUCCESS:             return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
        default:                      return "unknown";
    }
}

static void debugGLCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    const char* severityStr;
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_NOTIFICATION_KHR: severityStr = "NOTIFICATION"; break;
        case GL_DEBUG_SEVERITY_LOW_KHR:          severityStr = "LOW"; break;
        case GL_DEBUG_SEVERITY_MEDIUM_KHR:       severityStr = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_HIGH_KHR:         severityStr = "HIGH"; break;
        default:                                 severityStr = "UNKNOWN"; break;
    }
    ALOGE("GL_DEBUG[%s] (source=0x%x, type=0x%x, id=0x%x) %s", severityStr, source, type, id, message);
}

static void egl_Terminate(EGL* egl)
{
    ALOGV("egl_Shutdown()");
    eglTerminate(egl->display);
}

static void egl_DestroyAndUnbindSurface(EGL* egl)
{
    ALOGV("egl_DestroyAndUnbindSurface()");
    eglDestroySurface(egl->display, egl->surface);
    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    
    egl->surface = NULL;
}

static void egl_MakeCurrent(EGL* egl, ANativeWindow* nativeWindow)
{
    bool contextCreation = (egl->context == NULL);
    if (contextCreation)
    {
        ALOGV("egl_CreateContext()");
        
        // https://github.com/Dav1dde/glad/blob/42a246c2ec4deb4d6406eb43a163f1d672de3803/example/c/egl_x11/egl_x11.c#L51
        // Glad load egl in two steps
        // First step is to get enough to initialize egl
        // Second step to be able to request the version string in order to choose the functions to load
        assert(gladLoaderLoadEGL(NULL));

        egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        assert(egl->display);

        eglInitialize(egl->display, NULL, NULL);
        ALOGV("EGL_CLIENT_APIS: %s", eglQueryString(egl->display, EGL_CLIENT_APIS));
        ALOGV("EGL_VENDOR: %s",      eglQueryString(egl->display, EGL_VENDOR));
        ALOGV("EGL_VERSION: %s",     eglQueryString(egl->display, EGL_VERSION));
        //ALOGV("EGL_EXTENSIONS: %s",  eglQueryString(egl->display, EGL_EXTENSIONS));

        gladLoaderUnloadEGL();
        assert(gladLoadEGL(egl->display, (GLADloadfunc)eglGetProcAddress));

        EGLint attribs[] =
        {
            EGL_RED_SIZE,    8,
            EGL_GREEN_SIZE,  8,
            EGL_BLUE_SIZE,   8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
        };
        EGLint numConfig;
        eglChooseConfig(egl->display, attribs, &egl->config, 1, &numConfig);

        EGLint contextAttribs[] = { 
            EGL_CONTEXT_CLIENT_VERSION, 3, // gles version
            EGL_NONE 
        };
        egl->context = eglCreateContext(egl->display, egl->config, NULL, contextAttribs);
        assert(egl->context);
    }

    {
        ALOGV("egl_CreateAndBindSurface()");
        
        EGLint attribs[] =
        {
            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE
        };
        egl->surface = eglCreateWindowSurface(egl->display, egl->config, nativeWindow, attribs);
        assert(egl->surface);

        eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);
        eglSwapInterval(egl->display, 1); // Add to be done each time (default to 1)
    }

    if (contextCreation)
    {
        ALOGV("egl_LoadGLFuncs()");

        assert(gladLoadGLES2((GLADloadfunc)eglGetProcAddress));
        ALOGV("GL_VERSION: %s", glGetString(GL_VERSION));
        ALOGV("GL_VENDOR: %s", glGetString(GL_VENDOR));
        ALOGV("GL_RENDERER: %s", glGetString(GL_RENDERER));
        ALOGV("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
        ALOGV("GL_KHR_debug: %d", GLAD_GL_KHR_debug);
        ALOGV("GL_EXT_texture_filter_anisotropic: %d", GLAD_GL_EXT_texture_filter_anisotropic);

        if (GLAD_GL_KHR_debug)
        { 
            glEnable(GL_DEBUG_OUTPUT_KHR);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
            glDebugMessageCallbackKHR(debugGLCallback, NULL);
        }

        // List GL extensions
        if (0)
        {
            int numExtensions;
            glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
            for (int i = 0; i < numExtensions; ++i)
                ALOGV("%s", glGetStringi(GL_EXTENSIONS, i));
        }
    }
}

static void nativeActivity_ShowSoftInput(JNIEnv* env, const NativeActivityProto* proto)
{
    (*env)->CallVoidMethod(env, proto->object, proto->showSoftInput);
}

static void nativeActivity_HideSoftInput(JNIEnv* env, const NativeActivityProto* proto)
{
    (*env)->CallVoidMethod(env, proto->object, proto->hideSoftInput);
}

static void nativeActivity_Vibrate(JNIEnv* env, const NativeActivityProto* proto, int effectId)
{
    (*env)->CallVoidMethod(env, proto->object, proto->vibrate, effectId);
}

static void nativeActivity_Register(JNIEnv* env, NativeActivityProto* proto)
{
    assert(proto->object);
    proto->clazz = (*env)->GetObjectClass(env, proto->object);
    proto->showSoftInput = (*env)->GetMethodID(env, proto->clazz, "showSoftInput", "()V");
    proto->hideSoftInput = (*env)->GetMethodID(env, proto->clazz, "hideSoftInput", "()V");
    proto->vibrate = (*env)->GetMethodID(env, proto->clazz, "vibrate", "(I)V");
}

static void keyEvent_Register(JNIEnv* env, KeyEventProto* proto)
{
    proto->clazz = (*env)->FindClass(env, PACKAGE_PATH "/NativeWrapper$KeyEvent");
    proto->action = (*env)->GetFieldID(env, proto->clazz, "action", "I");
    proto->keyCode = (*env)->GetFieldID(env, proto->clazz, "keyCode", "I");
    proto->scanCode = (*env)->GetFieldID(env, proto->clazz, "scanCode", "I");
    proto->unicodeChar = (*env)->GetFieldID(env, proto->clazz, "unicodeChar", "I");
    proto->metaState = (*env)->GetFieldID(env, proto->clazz, "metaState", "I");
}

static InputEvent keyEvent_FromJava(JNIEnv* env, KeyEventProto* proto, jobject object)
{
    InputEvent event = { AINPUT_EVENT_TYPE_KEY };
    event.keyEvent.action = (*env)->GetIntField(env, object, proto->action);
    event.keyEvent.keyCode = (*env)->GetIntField(env, object, proto->keyCode);
    event.keyEvent.scanCode = (*env)->GetIntField(env, object, proto->scanCode);
    event.keyEvent.unicodeChar = (*env)->GetIntField(env, object, proto->unicodeChar);
    event.keyEvent.metaState = (*env)->GetIntField(env, object, proto->metaState);
    return event;
}

static void motionEvent_Register(JNIEnv* env, MotionEventProto* proto)
{
    proto->clazz = (*env)->FindClass(env, PACKAGE_PATH "/NativeWrapper$MotionEvent");
    proto->action = (*env)->GetFieldID(env, proto->clazz, "action", "I");
    proto->pointerCount = (*env)->GetFieldID(env, proto->clazz, "pointerCount", "I");
    proto->x = (*env)->GetFieldID(env, proto->clazz, "x", "[I");
    proto->y = (*env)->GetFieldID(env, proto->clazz, "y", "[I");
}

static void jni_FillIntArray(int* dst, int size, JNIEnv* env, jobject object, jfieldID field)
{
    jintArray intArrJava = (*env)->GetObjectField(env, object, field);
    int* intArrNative = (*env)->GetIntArrayElements(env, intArrJava, NULL);
    memcpy(dst, intArrNative, size * sizeof(int));
    (*env)->ReleaseIntArrayElements(env, intArrJava, intArrNative, 0);
}

static InputEvent motionEvent_FromJava(JNIEnv* env, MotionEventProto* proto, jobject object)
{
    InputEvent event = { AINPUT_EVENT_TYPE_MOTION };

    event.motionEvent.pointerCount = (*env)->GetIntField(env, object, proto->pointerCount);
    jni_FillIntArray(event.motionEvent.x, event.motionEvent.pointerCount, env, object, proto->x);
    jni_FillIntArray(event.motionEvent.y, event.motionEvent.pointerCount, env, object, proto->y);
    event.motionEvent.action = (*env)->GetIntField(env, object, proto->action);

    return event;
}

static void config_Register(JNIEnv* env, ConfigProto* proto)
{
    proto->clazz = (*env)->FindClass(env, PACKAGE_PATH "/NativeWrapper$Config");
    proto->filesDir = (*env)->GetFieldID(env, proto->clazz, "filesDir", "Ljava/lang/String;");
    proto->audioOutputSampleRate = (*env)->GetFieldID(env, proto->clazz, "audioOutputSampleRate", "I");
    proto->audioOutputFramesPerBuffer = (*env)->GetFieldID(env, proto->clazz, "audioOutputFramesPerBuffer", "I");
}

static Config config_FromJava(JNIEnv* env, ConfigProto* proto, jobject object)
{
    Config config = {};

    jstring filesDirJava = (jstring)(*env)->GetObjectField(env, object, proto->filesDir);
    const char* filesDir = (*env)->GetStringUTFChars(env, filesDirJava, NULL);
    strncpy(config.filesDir, filesDir, ARRAYSIZE(config.filesDir)-1);
    (*env)->ReleaseStringUTFChars(env, filesDirJava, filesDir);

    config.audioOutputSampleRate = (*env)->GetIntField(env, object, proto->audioOutputSampleRate);
    config.audioOutputFramesPerBuffer = (*env)->GetIntField(env, object, proto->audioOutputFramesPerBuffer);

    return config;
}

static void javaClasses_Register(JNIEnv* env, JavaClasses* classes)
{
    nativeActivity_Register(env, &classes->nativeActivity);
    keyEvent_Register(env, &classes->keyEvent);
    motionEvent_Register(env, &classes->motionEvent);
    config_Register(env, &classes->config);
}

static bool appThread_PollEvent(AppThread* appThread, Event* event, bool waitForEvent)
{
    pthread_mutex_lock(&appThread->mutex);
    //ALOGV("appThread_PollEvent() %d (count=%d)", waitForEvent, appThread->eventCount);

    if (appThread->eventCount == 0)
        pthread_cond_signal(&appThread->allEventsProcessed);

    while (appThread->eventCount == 0 && waitForEvent)
        pthread_cond_wait(&appThread->eventAddedCond, &appThread->mutex);

    bool result = false;
    if (appThread->eventCount > 0)
    {
        // Pop front event
        *event = appThread->events[0];
        appThread->eventCount--;
        if (appThread->eventCount > 0)
            memmove(appThread->events, appThread->events + 1, appThread->eventCount * sizeof(Event));

        result = true;
    }

    pthread_mutex_unlock(&appThread->mutex);
    return result;
}

static void appThread_AddEvent(AppThread* appThread, Event event, bool synchronous)
{
    pthread_mutex_lock(&appThread->mutex);
    //ALOGV("appThread_AddEvent() %s (count=%d)", eventTypeStr[event.type], appThread->eventCount);

    if (appThread->eventCount == ARRAYSIZE(appThread->events))
    {
        ALOGE("appThread_AddEvent() MAX_EVENT_COUNT reached");
    }
    else
    {
        appThread->events[appThread->eventCount++] = event;
        pthread_cond_signal(&appThread->eventAddedCond);
    }

    while (appThread->eventCount != 0 && synchronous)
        pthread_cond_wait(&appThread->allEventsProcessed, &appThread->mutex);

    pthread_mutex_unlock(&appThread->mutex);
}

static bool filterLogEvents(EventType type)
{
    return true;
    return type == EventType_DispatchKeyEvent
        || type == EventType_DispatchMotionEvent;
}

#include <math.h>
float SOUND_VOL = 0.2f;
float SOUND_FREQ = 220.f;
void SoundCallback(float* Buf, int NbFrames, int NbChannels, int SamplingRate, void* UserData)
{
    static float Phase = 0;

    for (int i = 0; i < NbFrames; ++i)
    {
        const float TAU = 6.28318530717958f;

        float Sample = sinf(Phase * TAU) * SOUND_VOL;
        Phase += SOUND_FREQ / SamplingRate;
        Phase = fmodf(Phase, 1.f);

        for (int c = 0; c < NbChannels; ++c)
        {
            Buf[i * NbChannels + c] = Sample;
        }
    }
}

static bool appThread_HandleEvents(AppThread* appThread, App* app)
{
    static int eventIndex = 0; // For debug purpose

    // Thread sleep if activity is paused or there is no surface attached
    // TODO: Maybe we can only wait until a surface is created
    Event event;
    while (appThread_PollEvent(appThread, &event, (app->activityPaused || !app->canRender)))
    {
        if (!filterLogEvents(event.type))
            ALOGV("Handle event[%d] %s ()", eventIndex, eventTypeStr[event.type]);

        switch (event.type)
        {
            case EventType_Create:
                
                app->soundDevice = SoundDevice_Create(event.create.config.audioOutputFramesPerBuffer, event.create.config.audioOutputSampleRate);
                app->game = game_Init();
                app->imguiTest = test_Init();

                SoundDevice_SetCallback(app->soundDevice, SoundCallback, NULL);

                break;

            case EventType_Destroy:
                game_UnloadGPUData(app->game);
                test_UnloadGPUData(app->imguiTest);
                eglTerminate(app->egl.display);

                test_Terminate(app->imguiTest);
                game_Terminate(app->game);
                SoundDevice_Destroy(app->soundDevice);
                return false;

            case EventType_Start:
                break;

            case EventType_Stop:
                break;

            case EventType_SurfaceCreated:
                {
                    bool onContextCreation = (app->egl.context == NULL);
                    egl_MakeCurrent(&app->egl, event.surfaceCreated.nativeWindow);

                    if (onContextCreation)
                    {
                        game_LoadGPUData(app->game);
                        test_LoadGPUData(app->imguiTest);
                    }
                }
                break;

            case EventType_SurfaceChanged:
                app->gameInputs.displayWidth = event.surfaceChanged.width;
                app->gameInputs.displayHeight = event.surfaceChanged.height;
                test_SizeChanged(event.surfaceChanged.width, event.surfaceChanged.height);
                app->canRender = true;
                break;

            case EventType_SurfaceDestroyed:
                egl_DestroyAndUnbindSurface(&app->egl);
                app->canRender = false;
                break;

            case EventType_Resume:
                app->activityPaused = false;
                SoundDevice_Resume(app->soundDevice);
                break;

            case EventType_Pause:
                app->activityPaused = true;
                SoundDevice_Pause(app->soundDevice);
                break;

            case EventType_WindowFocusChanged:
                break;

            case EventType_DispatchKeyEvent:
                test_HandleEvent(app->imguiTest, &event.dispatchKeyEvent);
                if (event.dispatchKeyEvent.keyEvent.unicodeChar != 0)
                    test_InputUnicodeChar(app->imguiTest, event.dispatchKeyEvent.keyEvent.unicodeChar);
                break;

            case EventType_DispatchMotionEvent:

                if (event.dispatchTouchEvent.type == AINPUT_EVENT_TYPE_MOTION)
                test_HandleEvent(app->imguiTest, &event.dispatchTouchEvent);
                app->gameInputs.touchX = event.dispatchTouchEvent.motionEvent.x[0];
                app->gameInputs.touchY = event.dispatchTouchEvent.motionEvent.y[0];

                if (test_GetIO(app->imguiTest)->disableVSYNCOnMotion)
                {
                    if (event.dispatchTouchEvent.motionEvent.action == AMOTION_EVENT_ACTION_DOWN)
                        eglSwapInterval(app->egl.display, 0);
                    if (event.dispatchTouchEvent.motionEvent.action == AMOTION_EVENT_ACTION_UP)
                        eglSwapInterval(app->egl.display, 1);
                }

                //ALOGV("Touch pressed: %d %d %d", event.dispatchTouchEvent.motionEvent.x, event.dispatchTouchEvent.motionEvent.y, event.dispatchTouchEvent.motionEvent.action);
                break;
            default:;
        }
    }

    return true;
}

static void* appThread_Func(void* arg)
{
    ALOGV("appThread_Func() start");
    AppThread* appThread = (AppThread*)arg;

    // Init thread related data
    (*appThread->javaVM)->AttachCurrentThread(appThread->javaVM, &appThread->jniEnv, NULL);

    App* app = calloc(1, sizeof(App));
    int frameIndex = 0;

    while (appThread_HandleEvents(appThread, app))
    {
        // update
        app->gameInputs.deltaTime = 1.f / 60.f;
        game_Update(app->game, &app->gameInputs);

        assert(app->imguiTest);
        {
            ImGuiTestIO* io = test_GetIO(app->imguiTest);
            
            test_UpdateAndDraw(app->imguiTest);
            if (io->showKeyboard)
            {
                nativeActivity_Vibrate(appThread->jniEnv, &appThread->javaClasses.nativeActivity, 2);
                nativeActivity_ShowSoftInput(appThread->jniEnv, &appThread->javaClasses.nativeActivity);
            }
            if (io->hideKeyboard)
            {
                nativeActivity_HideSoftInput(appThread->jniEnv, &appThread->javaClasses.nativeActivity);
            }    
        }

        eglSwapBuffers(app->egl.display, app->egl.surface);
        
        frameIndex++;

        // We should try to render everything that is not related to input before polling inputs
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    (*appThread->javaVM)->DetachCurrentThread(appThread->javaVM);

    free(app);
    ALOGV("appThread_Func() end");
    return NULL;
}

/*
================================================================================
Activity lifecycle
================================================================================
*/

JNIEXPORT jlong JNICALL
Java_com_example_app_NativeWrapper_onCreate(JNIEnv* jniEnv, jobject obj, jobject activity, jobject configJava)
{
    ALOGV("NativeWrapper::onCreate()");

    AppThread* appThread = calloc(1, sizeof(AppThread));
    (*jniEnv)->GetJavaVM(jniEnv, &appThread->javaVM);

    appThread->javaClasses.nativeActivity.object = (*jniEnv)->NewGlobalRef(jniEnv, activity);
    javaClasses_Register(jniEnv, &appThread->javaClasses);

    Config config = config_FromJava(jniEnv, &appThread->javaClasses.config, configJava);

    ALOGV("chdir to '%s'", config.filesDir);
    chdir(config.filesDir);

    pthread_mutex_init(&appThread->mutex, NULL);
    pthread_cond_init(&appThread->eventAddedCond, NULL);
    pthread_cond_init(&appThread->allEventsProcessed, NULL);

    pthread_create(&appThread->thread, NULL, appThread_Func, appThread);

    appThread_AddEvent(appThread, (Event){ 
        .type = EventType_Create,
        .create = { config }
    }, true);

    ALOGV("NativeWrapper::onCreate() ended");
    return (jlong)((size_t)appThread);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onDestroy(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onDestroy() begin");

    AppThread* appThread = (AppThread*)((size_t)handle);
    appThread_AddEvent(appThread, (Event){ EventType_Destroy }, true);

    // We usually never reach this point because app is killed before

    pthread_join(appThread->thread, NULL);
    (*jniEnv)->DeleteGlobalRef(jniEnv, appThread->javaClasses.nativeActivity.object);
    appThread->javaClasses.nativeActivity = (NativeActivityProto){};

    pthread_mutex_destroy(&appThread->mutex);
    pthread_cond_destroy(&appThread->eventAddedCond);
    pthread_cond_destroy(&appThread->allEventsProcessed);

    free(appThread);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onStart(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onStart()");
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){ EventType_Start }, false);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onStop(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onStop()");
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){ EventType_Stop }, true);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onResume(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onResume()");
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){ EventType_Resume }, false);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onPause(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onPause()");
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){ EventType_Pause }, true);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onWindowFocusChanged(JNIEnv* jniEnv, jobject obj, jlong handle, bool hasFocus)
{
    ALOGV("NativeWrapper::onWindowFocusChanged(%d)", hasFocus);
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){
        .type = EventType_WindowFocusChanged,
        .windowFocusChanged = { hasFocus }
    }, false);
}

/*
================================================================================
Surface lifecycle
================================================================================
*/

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_surfaceCreated(JNIEnv* jniEnv, jobject obj, jlong handle, jobject surface)
{
    ALOGV("NativeWrapper::surfaceCreated()");
    AppThread* appThread = (AppThread*)((size_t)handle);
    appThread->nativeWindow = ANativeWindow_fromSurface(jniEnv, surface);

    appThread_AddEvent(appThread, (Event){ 
        .type = EventType_SurfaceCreated,
        .surfaceCreated = {
            .nativeWindow = appThread->nativeWindow
        }}, true);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_surfaceChanged(JNIEnv* jniEnv, jobject obj, jlong handle, int format, int width, int height)
{
    ALOGV("NativeWrapper::surfaceChanged()");
    AppThread* appThread = (AppThread*)((size_t)handle);

    appThread_AddEvent(appThread, (Event){ 
        .type = EventType_SurfaceChanged,
        .surfaceChanged = {
            .format = format,
            .width = width,
            .height = height
        } }, false);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_surfaceDestroyed(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::surfaceDestroyed()");
    AppThread* appThread = (AppThread*)((size_t)handle);

    appThread_AddEvent(appThread, (Event){ EventType_SurfaceDestroyed }, true);

    ANativeWindow_release(appThread->nativeWindow);
    appThread->nativeWindow = NULL;
}

/*
================================================================================
Input
================================================================================
*/

JNIEXPORT bool JNICALL
Java_com_example_app_NativeWrapper_dispatchKeyEvent(JNIEnv* jniEnv, jobject obj, jlong handle, jobject keyEvent)
{
    AppThread* appThread = (AppThread*)((size_t)handle);
    InputEvent keyEventNative = keyEvent_FromJava(jniEnv, &appThread->javaClasses.keyEvent, keyEvent);
    ALOGV("NativeWrapper::dispatchKeyEvent(%d, %d)", keyEventNative.keyEvent.keyCode, keyEventNative.keyEvent.action);

    bool isHandled = false;
    keyEventNative.isHandled = &isHandled;

    appThread_AddEvent(appThread, (Event){
        .type = EventType_DispatchKeyEvent,
        .dispatchKeyEvent = keyEventNative
    }, false);

    return isHandled;
}

JNIEXPORT bool JNICALL
Java_com_example_app_NativeWrapper_dispatchTouchEvent(JNIEnv* jniEnv, jobject obj, jlong handle, jobject motionEvent)
{
    //ALOGV("NativeWrapper::dispatchTouchEvent()");
    AppThread* appThread = (AppThread*)((size_t)handle);

    InputEvent motionEventNative = motionEvent_FromJava(jniEnv, &appThread->javaClasses.motionEvent, motionEvent);

    bool isHandled = true;
    motionEventNative.isHandled = &isHandled;

    appThread_AddEvent(appThread, (Event){
        .type = EventType_DispatchMotionEvent,
        .dispatchTouchEvent = motionEventNative,
    }, false);

    return isHandled;
}


#include <stdlib.h> // calloc/free
#include <string.h> // memmove
#include <unistd.h> // chdir
#include <assert.h> // assert

#include <pthread.h>

// Basic Android Lifecycle Recommendations : https://docs.nvidia.com/tegra/Content/AN_LC_Common_Steps_When.html
#include <android/native_window_jni.h> // for native window JNI
#include <android/input.h>

#include <glad/egl.h>
#include <glad/gles2.h>

#include "common.h"

#include "game.h"

#include "imgui_test.h"

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
    EventType_DispatchTouchEvent,
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

typedef struct Event
{
    EventType type;

    union
    {
        struct WindowFocusChanged
        {
            bool hasFocus;
        } windowFocusChanged;

        struct DispatchKeyEvent
        {
            const AInputEvent* nativeEvent;
            int action;
            int keyCode;
            int unicodeChar;
            bool* resultPtr;
        } dispatchKeyEvent;
        
        struct DispatchTouchEvent
        {
            const AInputEvent* nativeEvent;
            int action;
            int x;
            int y;
        } dispatchTouchEvent;

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

typedef struct NativeActivity
{
    jclass clazz;
    jobject object;
    jmethodID showSoftInput;
    jmethodID hideSoftInput;
    jmethodID vibrate;
} NativeActivity;

typedef struct AppThread
{
    pthread_t thread;
    JavaVM* javaVM;
    JNIEnv* jniEnv;
    NativeActivity activity;

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

    EGL egl;

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

static bool egl_Init(EGL* egl)
{
    ALOGV("egl_Init()");

    // https://github.com/Dav1dde/glad/blob/42a246c2ec4deb4d6406eb43a163f1d672de3803/example/c/egl_x11/egl_x11.c#L51
    // Glad load egl in two steps
    // First step is to get enough to initialize egl
    // Second step to be able to request the version string in order to choose the functions to load
    assert(gladLoaderLoadEGL(NULL));

    egl->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl->display, NULL, NULL);
    ALOGV("EGL_CLIENT_APIS: %s", eglQueryString(egl->display, EGL_CLIENT_APIS));
    ALOGV("EGL_VENDOR: %s",      eglQueryString(egl->display, EGL_VENDOR));
    ALOGV("EGL_VERSION: %s",     eglQueryString(egl->display, EGL_VERSION));
    ALOGV("EGL_EXTENSIONS: %s",  eglQueryString(egl->display, EGL_EXTENSIONS));

    gladLoaderUnloadEGL();
    assert(gladLoadEGL(egl->display, (GLADloadfunc)eglGetProcAddress));

    EGLint attribs[] =
    {
        EGL_RED_SIZE,    8,
        EGL_GREEN_SIZE,  8,
        EGL_BLUE_SIZE,   8,
        EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    EGLint numConfig;
    eglChooseConfig(egl->display, attribs, &egl->config, 1, &numConfig);

    EGLint contextAttribs[] = { 
        EGL_CONTEXT_CLIENT_VERSION, 3, // gles version
        EGL_CONTEXT_OPENGL_DEBUG,   1,
        EGL_NONE 
    };
    egl->context = eglCreateContext(egl->display, egl->config, NULL, contextAttribs);

    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->context);
    eglSwapInterval(egl->display, 1);

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

    return true;
}

static void egl_Shutdown(EGL* egl)
{
    ALOGV("egl_Shutdown()");
    eglDestroyContext(egl->display, egl->context);
    eglTerminate(egl->display);
    ALOGV("egl_Shutdown() ended");
}

static void egl_DestroyAndUnbindSurface(EGL* egl)
{
    ALOGV("egl_DestroyAndUnbindSurface()");
    eglDestroySurface(egl->display, egl->surface);
    eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    egl->surface = NULL;
}

static void egl_CreateAndBindSurface(EGL* egl, ANativeWindow* nativeWindow)
{
    if (egl->surface != NULL)
        egl_DestroyAndUnbindSurface(egl);

    ALOGV("egl_CreateAndBindSurface()");
    
    egl->surface = eglCreateWindowSurface(egl->display, egl->config, nativeWindow, NULL);
    if (egl->surface == NULL)
        ALOGV("eglCreateWindowSurface failed");
    eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context);
}

static void nativeActivity_Register(JNIEnv* env, NativeActivity* activity)
{
    activity->clazz = (*env)->GetObjectClass(env, activity->object);
    activity->showSoftInput = (*env)->GetMethodID(env, activity->clazz, "showSoftInput", "()V");
    activity->hideSoftInput = (*env)->GetMethodID(env, activity->clazz, "hideSoftInput", "()V");
    activity->vibrate = (*env)->GetMethodID(env, activity->clazz, "vibrate", "(I)V");
}

static void nativeActivity_ShowSoftInput(JNIEnv* env, const NativeActivity* activity)
{
    (*env)->CallVoidMethod(env, activity->object, activity->showSoftInput);
}

static void nativeActivity_HideSoftInput(JNIEnv* env, const NativeActivity* activity)
{
    (*env)->CallVoidMethod(env, activity->object, activity->hideSoftInput);
}

static void nativeActivity_Vibrate(JNIEnv* env, const NativeActivity* activity, int effectId)
{
    (*env)->CallVoidMethod(env, activity->object, activity->vibrate, effectId);
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
        || type == EventType_DispatchTouchEvent;
}

static bool appThread_HandleEvents(AppThread* appThread, App* app)
{
    static int eventIndex = 0; // For debug purpose

    // Thread sleep if activity is paused or there is no surface attached
    // TODO: Maybe we can only wait until a surface is created
    Event event;
    while (appThread_PollEvent(appThread, &event, (app->activityPaused || app->egl.surface == NULL)))
    {
        if (!filterLogEvents(event.type))
            ALOGV("Handle event[%d] %s ()", eventIndex, eventTypeStr[event.type]);

        switch (event.type)
        {
            case EventType_Create:
                egl_Init(&app->egl);
                app->game = game_Init();
                app->imguiTest = test_Init();
                game_LoadGPUData(app->game);
                break;

            case EventType_Destroy:
                egl_Shutdown(&app->egl);
                test_Shutdown(app->imguiTest);
                game_UnloadGPUData(app->game);
                game_Destroy(app->game);
                return false;

            case EventType_Start:
                break;

            case EventType_Stop:
                break;

            case EventType_SurfaceCreated:
                egl_CreateAndBindSurface(&app->egl, event.surfaceCreated.nativeWindow);
                test_Load(app->imguiTest, event.surfaceCreated.nativeWindow);
                break;

            case EventType_SurfaceChanged:
                app->gameInputs.displayWidth = event.surfaceChanged.width;
                app->gameInputs.displayHeight = event.surfaceChanged.height;
                break;

            case EventType_SurfaceDestroyed:
                test_Unload(app->imguiTest);
                egl_DestroyAndUnbindSurface(&app->egl);
                break;

            case EventType_Resume:
                app->activityPaused = false;
                break;

            case EventType_Pause:
                app->activityPaused = true;
                break;

            case EventType_WindowFocusChanged:
                break;

            case EventType_DispatchKeyEvent:
                test_HandleEvent(app->imguiTest, event.dispatchKeyEvent.nativeEvent);
                if (event.dispatchKeyEvent.unicodeChar != 0)
                    test_InputUnicodeChar(app->imguiTest, event.dispatchKeyEvent.unicodeChar);
                break;

            case EventType_DispatchTouchEvent:
                test_HandleEvent(app->imguiTest, event.dispatchTouchEvent.nativeEvent);
                app->gameInputs.touchX = event.dispatchTouchEvent.x;
                app->gameInputs.touchY = event.dispatchTouchEvent.y;

                //ALOGV("Touch pressed: %d %d %d", event.dispatchTouchEvent.x, event.dispatchTouchEvent.y, event.dispatchTouchEvent.action);
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

    // Register NativeActivity wrapper
    nativeActivity_Register(appThread->jniEnv, &appThread->activity);

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
                nativeActivity_Vibrate(appThread->jniEnv, &appThread->activity, 2);
                nativeActivity_ShowSoftInput(appThread->jniEnv, &appThread->activity);
            }
            if (io->hideKeyboard)
            {
                nativeActivity_HideSoftInput(appThread->jniEnv, &appThread->activity);
            }    
        }

        eglSwapBuffers(app->egl.display, app->egl.surface);
        frameIndex++;
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
Java_com_example_app_NativeWrapper_onCreate(JNIEnv* jniEnv, jobject obj, jobject activity, jstring filesDir)
{
    ALOGV("NativeWrapper::onCreate()");

    AppThread* appThread = calloc(1, sizeof(AppThread));
    (*jniEnv)->GetJavaVM(jniEnv, &appThread->javaVM);
    appThread->activity.object = (*jniEnv)->NewGlobalRef(jniEnv, activity);

    pthread_mutex_init(&appThread->mutex, NULL);
    pthread_cond_init(&appThread->eventAddedCond, NULL);
    pthread_cond_init(&appThread->allEventsProcessed, NULL);

    pthread_create(&appThread->thread, NULL, appThread_Func, appThread);

    // Chdir to filesDir to be able to fopen copied files
    {
        const char* filesDirChars = (*jniEnv)->GetStringUTFChars(jniEnv, filesDir, NULL);
        ALOGV("chdir to '%s'", filesDirChars);
        chdir(filesDirChars);
        (*jniEnv)->ReleaseStringUTFChars(jniEnv, filesDir, filesDirChars);
    }

    appThread_AddEvent(appThread, (Event){ EventType_Create }, true);

    ALOGV("NativeWrapper::onCreate() ended");
    return (jlong)((size_t)appThread);
}

JNIEXPORT void JNICALL
Java_com_example_app_NativeWrapper_onDestroy(JNIEnv* jniEnv, jobject obj, jlong handle)
{
    ALOGV("NativeWrapper::onDestroy() begin");

    AppThread* appThread = (AppThread*)((size_t)handle);
    appThread_AddEvent(appThread, (Event){ EventType_Destroy }, true);

    // We usually never reach this point because app is killed

    pthread_join(appThread->thread, NULL);
    (*jniEnv)->DeleteGlobalRef(jniEnv, appThread->activity.object);
    appThread->activity = (NativeActivity){};

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
        }}, false);
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
Java_com_example_app_NativeWrapper_dispatchKeyEvent(JNIEnv* jniEnv, jobject obj, jlong handle, jobject keyEvent, int keyCode, int unicodeChar, int action)
{
    ALOGV("NativeWrapper::dispatchKeyEvent(%d, %d)", keyCode, action);

    bool result = false;
    appThread_AddEvent((AppThread*)((size_t)handle), (Event){
        .type = EventType_DispatchKeyEvent,
        .dispatchKeyEvent = 
        {
            .nativeEvent = AKeyEvent_fromJava(jniEnv, keyEvent),
            .action = action,
            .keyCode = keyCode,
            .unicodeChar = unicodeChar,
            .resultPtr = &result,
        }
    }, true);
    return result;
}

JNIEXPORT bool JNICALL
Java_com_example_app_NativeWrapper_dispatchTouchEvent(JNIEnv* jniEnv, jobject obj, jlong handle, jobject motionEvent, int x, int y, int action)
{
    //ALOGV("NativeWrapper::dispatchTouchEvent()");

    appThread_AddEvent((AppThread*)((size_t)handle), (Event){
        .type = EventType_DispatchTouchEvent,
        .dispatchTouchEvent = 
        {
            .nativeEvent = AMotionEvent_fromJava(jniEnv, motionEvent),
            .action = action,
            .x = x,
            .y = y,
        }
    }, false);
    return true; // Capture all touch events
}


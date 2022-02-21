package com.example.app;
import android.app.Activity;
import android.view.Surface;
import android.view.KeyEvent;
import android.view.MotionEvent;

// Wrapper for native library
public class NativeWrapper
{
    // Activity lifecycle
    public static native long onCreate(Activity obj, String filesDir);
    public static native void onDestroy(long handle);
    public static native void onStart(long handle);
    public static native void onStop(long handle);
    public static native void onResume(long handle);
    public static native void onPause(long handle);
    public static native void onWindowFocusChanged(long handle, boolean hasFocus);

    // Input
    public static native boolean dispatchKeyEvent(long handle, KeyEvent keyEvent, int keyCode, int unicodeChar, int action);
    public static native boolean dispatchTouchEvent(long handle, MotionEvent motionEvent, int x, int y, int action);

    // Surface lifecycle
    public static native void surfaceCreated(long handle, Surface s);
    public static native void surfaceChanged(long handle, int format, int width, int height);
    public static native void surfaceDestroyed(long handle);
}

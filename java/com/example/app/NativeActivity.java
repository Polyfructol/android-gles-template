package com.example.app;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Vibrator;
import android.os.VibrationEffect;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.view.InputQueue;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.File;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;

// Inspiration: https://github.com/aosp-mirror/platform_frameworks_base/blob/master/core/java/android/app/NativeActivity.java
public class NativeActivity extends Activity implements SurfaceHolder.Callback
{
    static
    {
        System.loadLibrary("app");
    }

    private static final String TAG = "EmptyApp";

    private long mNativeHandle;
    private SurfaceView mView;

    private InputMethodManager mInputMethodManager;
    private Vibrator mVibrator;
    private AudioManager mAudioManager;

    // Kept in cache to avoid micro allocation
    private NativeWrapper.Config mConfig = new NativeWrapper.Config();
    private NativeWrapper.KeyEvent mKeyEvent = new NativeWrapper.KeyEvent();
    private NativeWrapper.MotionEvent mMotionEvent = new NativeWrapper.MotionEvent();

    private void copyFile(InputStream in, OutputStream out) throws IOException
    {
        byte[] buffer = new byte[1024];
        int read;
        while((read = in.read(buffer)) != -1)
            out.write(buffer, 0, read);
    }

    private void moveAssetsToFilesDir()
    {
        // Copy assets to data folder
        AssetManager assets = this.getAssets();

        // Ugly code to copy assets to local data (for easy access with fopen)
        try
        {
            String files[] = assets.list("assets");
            for (String file : files)
            {
                try
                {
                    InputStream in = assets.open("assets/" + file);
                    FileOutputStream out = openFileOutput(file, Context.MODE_PRIVATE);
                    copyFile(in, out);
                    in.close();
                    out.flush();
                    out.close();
                    Log.v(TAG, "\"files/" + file + "\" copied");
                }
                catch (IOException e)
                {
                    // TODO: Add copy of subfolders (An exception is triggered if file is a directory)
                    Log.v(TAG, "Cannot copy folder (TODO): " + e);
                }
            }
        }
        catch (IOException e)
        {
            Log.v(TAG, "IOException: " + e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        Log.v(TAG, "----------------------------------------------------------------");
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mInputMethodManager = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        mVibrator = (Vibrator)getSystemService(Context.VIBRATOR_SERVICE);
        mAudioManager = (AudioManager)getSystemService(Context.AUDIO_SERVICE);

        mView = new SurfaceView(this);
        mView.getHolder().addCallback(this);
        setContentView(mView);
        
        moveAssetsToFilesDir();

        // Call NativeWrapper
        mConfig.filesDir = getApplicationContext().getFilesDir().getAbsolutePath();
        mConfig.audioOutputSampleRate = Integer.parseInt(mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
        mConfig.audioOutputFramesPerBuffer = Integer.parseInt(mAudioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
        
        mNativeHandle = NativeWrapper.onCreate(this, mConfig);
    }

    @Override
    protected void onDestroy()
    {
        NativeWrapper.onDestroy(mNativeHandle);
        super.onDestroy();
        mNativeHandle = 0;
    }

    @Override
    protected void onStart()
    {
        super.onStart();
        NativeWrapper.onStart(mNativeHandle);
    }

    @Override
    protected void onStop()
    {
        NativeWrapper.onStop(mNativeHandle);
        super.onStop();
    }

    @Override
    protected void onPause()
    {
        NativeWrapper.onPause(mNativeHandle);
        super.onPause();
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        NativeWrapper.onResume(mNativeHandle);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        NativeWrapper.onWindowFocusChanged(mNativeHandle, hasFocus);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder)
    {
        NativeWrapper.surfaceCreated(mNativeHandle, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
    {
        NativeWrapper.surfaceChanged(mNativeHandle, format, width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder)
    {
        NativeWrapper.surfaceDestroyed(mNativeHandle);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event)
    {
        int keyCode = event.getKeyCode();
        int action = event.getAction();

        int unicodeChar = 0;
        if (event.getAction() == KeyEvent.ACTION_DOWN)
            unicodeChar = event.getUnicodeChar(event.getMetaState());

        // Hack to get at least some special characters
        else if (event.getAction() == KeyEvent.ACTION_MULTIPLE && keyCode == 0)
            unicodeChar = event.getCharacters().charAt(0);

        mKeyEvent.action = action;
        mKeyEvent.keyCode = keyCode;
        mKeyEvent.scanCode = event.getScanCode();
        mKeyEvent.unicodeChar = unicodeChar;
        mKeyEvent.metaState = event.getMetaState();

        return NativeWrapper.dispatchKeyEvent(mNativeHandle, mKeyEvent) || super.dispatchKeyEvent(event);
    }
    
    @Override
    public boolean dispatchTouchEvent(MotionEvent event)
    {
        int x = (int)event.getRawX();
        int y = (int)event.getRawY();
        int action = event.getAction();

        // Hack to correct location when view is not fullscreen...
        // TODO: What if multiple pointers...
        // getX() and getY() were not correct
        {
            int offsets[] = { 0, 0 };
            mView.getLocationOnScreen(offsets);
            event.setLocation(x - offsets[0], y - offsets[1]);
        }

        mMotionEvent.action = action;
        mMotionEvent.pointerCount = event.getPointerCount();
        for (int i = 0; i < mMotionEvent.pointerCount; ++i)
        {
            mMotionEvent.x[i] = (int)event.getX(i);
            mMotionEvent.y[i] = (int)event.getY(i);
        }

        return NativeWrapper.dispatchTouchEvent(mNativeHandle, mMotionEvent) || super.dispatchTouchEvent(event);
    }

    public void showSoftInput()
    {
        mInputMethodManager.showSoftInput(this.getWindow().getDecorView(), InputMethodManager.SHOW_IMPLICIT);
    }

    public void hideSoftInput()
    {
        mInputMethodManager.hideSoftInputFromWindow(mView.getWindowToken(), InputMethodManager.HIDE_IMPLICIT_ONLY);
    }

    public void vibrate(int effectId)
    {
        mVibrator.vibrate(VibrationEffect.createPredefined(effectId));
    }
}

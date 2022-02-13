package com.example.app;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.File;

import android.app.AlertDialog;
import android.app.AlertDialog.Builder;

public class NativeActivity extends Activity implements SurfaceHolder.Callback
{
    static
    {
        System.loadLibrary("app");
    }

    private static final String TAG = "EmptyApp";

    private SurfaceView mView;
    private SurfaceHolder mSurfaceHolder;
    private long mNativeHandle;

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
                    in = null;
                    out.flush();
                    out.close();
                    out = null;
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
            //log the exception
            Log.v(TAG, "IOException: " + e);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        Log.v(TAG, "----------------------------------------------------------------");
        moveAssetsToFilesDir();
        super.onCreate(savedInstanceState);

        mView = new SurfaceView(this);
        mView.getHolder().addCallback(this);
        setContentView(mView);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        String filesDir = getApplicationContext().getFilesDir().getAbsolutePath();
        mNativeHandle = NativeWrapper.onCreate(this, filesDir);
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
        super.onResume();
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
        mSurfaceHolder = holder;
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder)
    {
        NativeWrapper.surfaceDestroyed(mNativeHandle);
        mSurfaceHolder = null;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent ev)
    {
        int keyCode = ev.getKeyCode();
        int action = ev.getAction();

        return NativeWrapper.dispatchKeyEvent(mNativeHandle, keyCode, action) || super.dispatchKeyEvent(ev);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev)
    {
        int x = (int)ev.getRawX();
        int y = (int)ev.getRawY();
        int action = ev.getAction();

        return NativeWrapper.dispatchTouchEvent(mNativeHandle, x, y, action) || super.dispatchTouchEvent(ev);
    }
}

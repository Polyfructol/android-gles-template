# Env

source set_android_env.sh

# dlopen possible!

In native code:
```c++
    // adb shell setprop debug.ld.all dlerror,dlopen
    void* dlhandle = dlopen("./hot.so", RTLD_NOW);
    if (dlhandle)
    {
        ALOGV("dlopen SUCCESS!!!");
        int (*add)(int,int) = dlsym(dlhandle, "add");
        if (add)
        {
            ALOGV("add 21 = %d", add(21, 21));
        }
        dlclose(dlhandle);
    }
    else
        ALOGV("dlopen FAILURE!!!");
```

For quick & dirty solution: disable strict mode to allow download of files in main thread
```java
    protected void onCreate(Bundle savedInstanceState)
    {
        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);
```

Download .so files
```java
        try
        {
            URL url = new URL("https://website.net/hot.so");
            HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
            try {
                InputStream in = new BufferedInputStream(urlConnection.getInputStream());
                FileOutputStream out = openFileOutput("hot.so", Context.MODE_PRIVATE);
                copyFile(in, out);
                in.close();
                in = null;
                out.flush();
                out.close();
                out = null;
            } finally {
                urlConnection.disconnect();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
```
Imports:
```java
import java.net.HttpURLConnection;
import java.net.URL;
import java.io.BufferedInputStream;
import android.os.StrictMode;
```


# GPU context

onCreate: egl context available to load data
onSurfaceCreated: able to start to render
onSurfaceDestroyed: cannot render but can keep data
onTrimMemory: perhaps useful to unload gpu data
onDestroy: elg context destroyed at this point, no need to unload gpu data, to be extra clean we should unvalidate all gpu handles (but the activity is destroyed)

## Mathching interface

- event
> called once

- onCreate:
>loadCPUData
>loadGPUData
- onSurfaceCreated:
- onSurfaceChanged:
render
render
render
- onSurfaceDestroyed
nothing will happen until onSurfaceCreated is created again
- onTrimMemory (TODO) https://developer.android.com/games/optimize/memory-allocation
unloadGPUData
unloadCPUData


## Multiple devices
ANDROID_SERIAL=emulator-5554 make run


## Print callstack
```c++


#include <unwind.h>
#include <dlfcn.h>
#include <stdio.h>

namespace {

struct BacktraceState
{
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}

}

size_t captureBacktrace(void** buffer, size_t max)
{
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - buffer;
}

#include "common.h"
extern "C" void dumpBacktrace(void)
{
    const size_t max = 30;
    void* buffer[max];
    size_t count = captureBacktrace(buffer, max);
    for (size_t idx = 0; idx < count; ++idx) {
        const void* addr = buffer[idx];

        Dl_info info;
        if (dladdr(addr, &info) && info.dli_sname) {
            ALOGV(" # %02zu : %p (%s %s)\n", idx, addr, info.dli_fname, info.dli_sname);
        }
        else {
            ALOGV(" # %02zu : %p\n", idx, addr);
        }
}
```

Then convert address to symbols by using `(gdb) info line *[Address]`
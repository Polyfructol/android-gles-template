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
            URL url = new URL("https://paulgalindo.net/hot.so");
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
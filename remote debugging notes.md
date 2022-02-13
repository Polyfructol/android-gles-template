TODO: Try to launch CDT inside folder to avoid absolute path in config + .gdbinit

## Divers
Compile with -O0 -g !!

## Attach gdb after app launched

Faster when gdb-server is forwarded using adb !!
forward tcp:8123 tcp:8123

make run
adb shell am start-activity -n com.example.app/com.example.app.NativeActivity

make attach-debugger
adb push C:\Users\p.galindo\Documents\dev\android\android-sdk\ndk\21.3.6528147/prebuilt/android-arm64/gdbserver/gdbserver /data/local/tmp
adb shell "cat /data/local/tmp/gdbserver | run-as com.example.app sh -c 'cat > /data/data/com.example.app/gdbserver && chmod 700 /data/data/com.example.app/gdbserver'"
adb shell "echo /data/data/com.example.app/gdbserver --attach localhost:8123 \`pidof com.example.app\` | run-as com.example.app"

.gdbinit
set osabi GNU/Linux
handle SIG33 nostop noprint
handle SIG35 nostop noprint
set sysroot C:/Users/p.galindo/Documents/dev/oculus/empty/symbols
set solib-search-path C:/Users/p.galindo/Documents/dev/oculus/empty/lib/arm64-v8a

set sysroot is useful even if sysroot has not been downloaded because it avoid to read symbol on remote
To download symbols...
First, list them :
(gdb) info sharedlibrary
Then :
adb pull ...
Using the same directory structure

debug using ndk gdb is not handy (even launching it in cygwin...)
C:\Users\p.galindo\Documents\dev\android\android-sdk\ndk\21.3.6528147\prebuilt\windows-x86_64\bin\gdb.exe
target remote localhost:8123
...

## Using eclipse cdt standalone debugger
CDT debugger tips
 - Use breadcrumb inside the debug view to speed up debugging
   - Bug when to try to show thread that contains too much frame (so switch between Automatic<->Breadcrumb)

- DO NOT SPECIFY libapp.so as a C/C++ Application, because it will break the breakpoints...
- I had to create an empty binary as C/C++ Application because it can't start if no exe are specified.
- BUT, it's handy to drag-and-drop libapp.so inside Executables view (bottom) to be able to explore files
- There is almost no lag when stepping inside code !!!

## Attach gdb on startup

First we need to launch the app in debug mode, so it will wait for jdb to connect

adb shell am start-activity -D -n com.example.app/com.example.app.NativeActivity

make attach-debugger

adb forward tcp:1234 jdwp:"PID"

PID is displayed when gdb-server is launched

Launch CDT debugger and launch debugger

Unblock debug wait
jdb -connect com.sun.jdi.SocketAttach:hostname=localhost,port=1234

Worked only if sysroot has been used (TODO: Identify which library are needed)
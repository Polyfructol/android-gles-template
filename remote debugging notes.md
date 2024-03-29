# Android native debugging (notes)

Linux only.

## Prerequisite
Properly configure paths in `set_android_env.sh` then execute `source set_android_env.sh` to populate `$path` env vars with android toolchain.

`source set_android_env.sh` has to be called in every new terminal.

# Version change
## NDK 25
`gdb` and `gdbserver` prebuilt binaries are not provided in ndk >= 25... It seems that `lldb` is favorited over `gdb`. 

`lldb-server` path:
```ndk/25.1.8937393/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/14.0.6/lib/linux/aarch64/lldb-server```

## TL;DR Debug in vscode
- Launch vscode with command line `source set_android_env.sh && code`
- Execute `make run && make start-gdbserver` in integrated terminal
- Start Debugging (F5)

## Debug from command line
```bash
$ make run # Start the app on the phone
$ make start-gdbserver # Attach gdbserver (app is now frozen)

# In another terminal
$ gdb
# It's best to whitelist .gdbinit by following the instructions so the following commands will be executed automatically

# Or simply execute them manually (.gdbinit content)
(gdb) set auto-solib-add on
# Do not set sysroot if the libraries are not present (next subject)
(gdb) set sysroot .
(gdb) set solib-search-path symbols:lib/arm64-v8a
(gdb) file app_process64
(gdb) target remote localhost:8123
```

Details of `make run` commands:
```bash
adb install -r app.apk
adb shell am start-activity -n com.example.app/com.example.app.NativeActivity
```

Details of `make start-gdbserver` commands:
```bash
adb pull /system/bin/app_process64
adb push /home/paul/Android/Sdk/ndk/23.1.7779620/prebuilt/android-arm64/gdbserver/gdbserver /data/local/tmp
adb shell "cat /data/local/tmp/gdbserver | run-as com.example.app sh -c 'cat > /data/data/com.example.app/gdbserver && chmod 700 /data/data/com.example.app/gdbserver'"
adb forward tcp:8123 tcp:8123
adb shell "echo /data/data/com.example.app/gdbserver --attach localhost:8123 \`pidof com.example.app\` | run-as com.example.app"
```

## Fix slow symbols loading
To avoid reading symbols from remote target, we can copy them locally. Attach gdb then :

```bash
gdb
# Print shared library dependencies into gdb.txt
(gdb) set logging on
(gdb) info sharedlibrary
# press 'c' to complete output then quit ('q')
```

Extract each library with `adb pull` inside a `symbols/` folder
```bash
mkdir symbols
cat gdb.txt | cut -c 53- | grep -v " Yes \| process " | tail -n +2 | xargs -I{} adb pull {} symbols/
```

Now that gdb use the local files, the startup time should be reduced dramatically.

## Attach gdb at application startup

Sometimes, it might be necessary to debug right at the beggining of the execution. [Link to official doc](https://source.android.com/docs/core/tests/debug/gdb#app-startup).

First, start the application in a special "Waiting For Debugger" state.
```bash
$ adb shell am start-activity -D -n com.example.app/com.example.app.NativeActivity
$ make start-gdbserver
...
Attached; pid = 7776
Listening on port 8123
# Note the pid (here 7776)
```

Launch gdb in another terminal.
```bash
$ gdb
(gdb) ... # Place breakpoints (e.g. __pthread_start)
```

Connect jdb to unlock the app.
```bash
$ adb forward tcp:1234 jdwp:7776
$ jdb -connect com.sun.jdi.SocketAttach:hostname=localhost,port=1234
```

Return to gdb then:
```bash
(gdb) continue
```

TODO: Fix that `libapp.so` symbols are not loaded automatically because the lib is not loaded at the start of the application...

Workaround:
```bash
(gdb) ... # Advance the program, blindly... Continue 5-6 times on __pthread_start breakpoint.
(gdb) info sharedlibrary # Until libapp.so is listed there
(gdb) sharedlibrary # Then force loading missing symbols
```

## Use vscode gdb frontend

Launch vscode through terminal.
```bash
$ source set_android_env.sh
$ code
```

Check launch configuration in [.vscode/launch.json](.vscode/launch.json).

Start app like before `make run` then `make start-gdbserver`

## Troubleshooting

Cannot attach gdbserver:
```bash
$ make killall
adb shell run-as com.example.app killall gdbserver
adb shell run-as com.example.app killall com.example.app
```

Inspect vscode gdb commands by enabling `engineLogging` in `launch.json`

## LLDB
Start a lldb-server instance on target device
```bash
$ make start-lldb-server
```

```bash
$ lldb
platform select remote-android
platform connect localhost:8123
process attach --pid 1406
```

## (OLD) Attach gdb after app launched

```bash
adb pull /system/bin/app_process64
appThread_HandleEvents
gdb-server is faster when  is forwarded using adb !!

.gdbinit
set osabi GNU/Linux
handle SIG33 nostop noprint
handle SIG35 nostop noprint

set sysroot is useful even if sysroot has not been downloaded because it avoid to read symbol on remote
```

## (OLD) Using eclipse cdt standalone debugger
CDT debugger tips
 - Use breadcrumb inside the debug view to speed up debugging
   - Bug when to try to show thread that contains too much frame (so switch between Automatic<->Breadcrumb)

- DO NOT SPECIFY libapp.so as a C/C++ Application, because it will break the breakpoints...
- I had to create an empty binary as C/C++ Application because it can't start if no exe are specified.
- BUT, it's handy to drag-and-drop libapp.so inside Executables view (bottom) to be able to explore files
- There is almost no lag when stepping inside code !!!

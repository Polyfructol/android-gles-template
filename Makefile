
# TODO: Use aapt2
ANDROID_PLATFORM=$(ANDROID_HOME)/platforms/android-32

PACKAGE=com.example.app
PACKAGE_DIR=$(subst .,/,$(PACKAGE))
FINAL_APK=app.apk
APK=$(FINAL_APK).unaligned

#TARGET_HOST=arm-linux-androideabi
CC=clang --target=aarch64-linux-android
CFLAGS=-Wall -O0 -g -funwind-tables -fPIC
CFLAGS+=-Wno-unused-function
CPPFLAGS=-MMD -Isrc
LDFLAGS=-Wl,--no-undefined
LDLIBS=-llog -landroid -lGLESv3 -lEGL -lm

# Javac flags
# bootclasspath "" to avoid warnings
JAVA_SRCS=java/$(PACKAGE_DIR)/NativeWrapper.java java/$(PACKAGE_DIR)/NativeActivity.java
JAVA_OBJS=bin/$(PACKAGE_DIR)/NativeWrapper.class bin/$(PACKAGE_DIR)/NativeActivity.class
JAVACFLAGS=-classpath $(ANDROID_PLATFORM)/android.jar:bin -bootclasspath "" -target 8 -source 8 -d 'bin'

#TODO: find why there is this 30/31 folder
#TODO: HACK: I just moved 31 content into parent...
#LDFLAGS+=-L/home/paul/android/android-sdk/ndk/23.1.7779620/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/31/

OBJS=src/activity.o src/gles2.o src/egl.o src/game.o
DEPS=$(OBJS:.o=.d)

BINARIES=lib/arm64-v8a/libapp.so

.DELETE_ON_ERROR:

.PHONY: all clean run attach-debugger install killall log

all: $(FINAL_APK)

-include $(DEPS)

bin gen lib/arm64-v8a:
	mkdir -p $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lib/arm64-v8a/libapp.so: $(OBJS) | lib/arm64-v8a $(APK)
	$(CC) -shared $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	zip -u $(APK) $@

# Explicitly list java dependencies
bin/$(PACKAGE_DIR)/NativeActivity.class: java/$(PACKAGE_DIR)/NativeWrapper.java

classes.dex: $(JAVA_SRCS) | $(APK)
	javac $(JAVACFLAGS) $(JAVA_SRCS)
	d8 --no-desugaring --classpath bin $(JAVA_OBJS)
	zip -u $(APK) $@

res_compiled.zip:
	aapt2 compile --dir res -o res_compiled.zip

$(APK): res_compiled.zip AndroidManifest.xml
	aapt2 link res_compiled.zip -o $(APK) -I $(ANDROID_PLATFORM)/android.jar -A assets --manifest AndroidManifest.xml

$(FINAL_APK): $(APK) res_compiled.zip AndroidManifest.xml lib/arm64-v8a/libapp.so classes.dex
	jarsigner -keystore debug.keystore -storepass 'android' $(APK) androiddebugkey
	zipalign -f 4 $(APK) $@

clean:
	rm -rf gen bin lib classes.dex $(APK) $(FINAL_APK) res_compiled.zip
	rm -rf src/*.o src/*.d debug-executable

install: $(FINAL_APK)
	adb install -r $(FINAL_APK)

run: install
	adb shell am start-activity -n $(PACKAGE)/$(PACKAGE).NativeActivity

# Debug executable needed for debugging with CDT
debug-executable:
	echo "" | $(CC) -x c++ -shared -o $@ -

attach-debugger: $(ANDROID_NDK_HOME)/prebuilt/android-arm64/gdbserver/gdbserver
	-adb push $^ /data/local/tmp
	-adb shell "cat /data/local/tmp/gdbserver | run-as $(PACKAGE) sh -c 'cat > /data/data/$(PACKAGE)/gdbserver && chmod 700 /data/data/$(PACKAGE)/gdbserver'"
	adb forward tcp:8123 tcp:8123
	adb shell "echo /data/data/$(PACKAGE)/gdbserver --attach localhost:8123 \`pidof $(PACKAGE)\` | run-as $(PACKAGE)"

log:
	adb logcat --pid=`adb shell pidof $(PACKAGE) | sed 's/\r//g'`

killall:
	-adb shell run-as $(PACKAGE) killall gdbserver
	-adb shell run-as $(PACKAGE) killall $(PACKAGE)

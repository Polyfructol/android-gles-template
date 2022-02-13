
# TODO: Use aapt2
ANDROID_PLATFORM=$(ANDROID_HOME)/platforms/android-32

PACKAGE=com.example.app
PACKAGE_DIR=$(subst .,/,$(PACKAGE))
APK_NAME=app.apk

#TARGET_HOST=arm-linux-androideabi
CC=clang --target=aarch64-linux-android
CFLAGS=-Wall -O0 -g -funwind-tables -fPIC
CFLAGS+=-Wno-unused-function
CPPFLAGS=-MMD -Isrc
LDFLAGS=-Wl,--no-undefined
LDLIBS=-llog -landroid -lGLESv3 -lEGL -lm

# Javac flags
# bootclasspath "" to avoid warnings
JAVACFLAGS=-classpath $(ANDROID_PLATFORM)/android.jar:bin -bootclasspath "" -target 8 -source 8 -d 'bin'

#TODO: find why there is this 30/31 folder
#TODO: HACK: I just moved 31 content into parent...
#LDFLAGS+=-L/home/paul/android/android-sdk/ndk/23.1.7779620/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/31/

OBJS=src/activity.o src/gles2.o src/egl.o src/game.o
DEPS=$(OBJS:.o=.d)

BINARIES=lib/arm64-v8a/libapp.so

.DELETE_ON_ERROR:

.PHONY: all clean run attach-debugger install killall log

all: $(APK_NAME)

-include $(DEPS)

bin gen lib/arm64-v8a:
	mkdir -p $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

lib/arm64-v8a/libapp.so: $(OBJS) | lib/arm64-v8a
	$(CC) -shared $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Explicitly list java dependencies
java/$(PACKAGE_DIR)/NativeActivity.java: bin/$(PACKAGE_DIR)/NativeWrapper.class

bin/$(PACKAGE_DIR)/%.class: java/$(PACKAGE_DIR)/%.java | bin
	javac $(JAVACFLAGS) $<

gen/$(PACKAGE_DIR)/R.java: AndroidManifest.xml $(wildcard res/*/*) | gen
	aapt package -f -m -J gen -M $< -S res -I $(ANDROID_PLATFORM)/android.jar

bin/$(PACKAGE_DIR)/%.class: gen/$(PACKAGE_DIR)/%.java | bin
	javac $(JAVACFLAGS) $<

classes.dex: bin/$(PACKAGE_DIR)/R.class bin/$(PACKAGE_DIR)/NativeWrapper.class bin/$(PACKAGE_DIR)/NativeActivity.class
	d8 --no-desugaring --classpath bin $^

$(APK_NAME).unaligned: classes.dex $(BINARIES)
	aapt package -f -M AndroidManifest.xml -S res -I $(ANDROID_PLATFORM)/android.jar -F $(APK_NAME).unaligned
	aapt add $(APK_NAME).unaligned lib/**/*
	aapt add $(APK_NAME).unaligned classes.dex
	find assets -type f | xargs aapt add $(APK_NAME).unaligned
	jarsigner -keystore debug.keystore -storepass 'android' $(APK_NAME).unaligned androiddebugkey

$(APK_NAME): $(APK_NAME).unaligned
	zipalign -f 4 $^ $@

# TODO: Move to aapt2
# https://community.e.foundation/t/bacol-build-apks-from-the-command-line-aka-bacol/20891
# https://github.com/TheDotLabs/Bacol/blob/master/bacol.sh
# aapt2 link -o output.apk -I $ANDROID_PLATFORM/android.jar -A assets/assets  --manifest AndroidManifest.xml

# aapt2 compile --dir res -o compiled/res.zip
# aapt2 link compiled/res.zip -o output.apk -I $ANDROID_PLATFORM/android.jar -A assets/assets  --manifest AndroidManifest.xml
# zip -u output.apk classes.dex
# zip -u output.apk lib/arm64-v8a/libapp.so
# jarsigner -keystore debug.keystore -storepass 'android' output.apk androiddebugkey
clean:
	rm -rf gen bin lib classes.dex $(APK_NAME) $(APK_NAME).unaligned
	rm -rf src/*.o src/*.d debug-executable

install: $(APK_NAME)
	adb install -r $(APK_NAME)

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


# TODO: Use aapt2
ANDROID_PLATFORM=$(ANDROID_HOME)/platforms/android-32

PACKAGE=com.example.app
PACKAGE_DIR=$(subst .,/,$(PACKAGE))
FINAL_APK=app.apk
APK=$(FINAL_APK).unaligned

#TARGET_HOST=arm-linux-androideabi
CC=clang --target=aarch64-linux-android
CXX=clang++ --target=aarch64-linux-android
CFLAGS=-Wall -O0 -g -funwind-tables -fPIC -fvisibility=hidden
CFLAGS+=-Wno-unused-function
CXXFLAGS=$(CFLAGS) -fno-exceptions -fno-rtti
CPPFLAGS=-MMD -Isrc -Iexternals/include
LDFLAGS=-Wl,--no-undefined
LDLIBS=-llog -landroid -lGLESv3 -lEGL -lm -static-libstdc++

FILES_TO_ZIP=lib/arm64-v8a/libapp.so classes.dex
FILES_TO_ZIP_FLAGS=$(addsuffix .zipped_to_apk,$(FILES_TO_ZIP))

RESOURCES=res/values/strings.xml res/values/style.xml res/layout/activity_main.xml

CL_RESOURCES=externals/constraintlayout/res/values/attrs.xml externals/constraintlayout/res/values/ids.xml
# Javac flags
# bootclasspath "" to avoid warnings
JAVA_SRCS=java/$(PACKAGE_DIR)/NativeWrapper.java java/$(PACKAGE_DIR)/NativeActivity.java java/$(PACKAGE_DIR)/MainActivity.java

JAVA_GENS=gen/$(PACKAGE_DIR)/R.java

JAVA_OBJS=$(subst .java,.class,$(subst java/,bin/,$(JAVA_SRCS)) $(subst gen/,bin/,$(JAVA_GENS)))
JAVACFLAGS=-classpath $(ANDROID_PLATFORM)/android.jar:bin:externals/constraintlayout/java -bootclasspath "" -target 8 -source 8 -d 'bin'

#TODO: find why there is this 30/31 folder
#TODO: HACK: I just moved 31 content into parent...
#LDFLAGS+=-L/home/paul/android/android-sdk/ndk/23.1.7779620/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/31/

OBJS=src/activity.o src/game.o
OBJS+=src/imgui_test.o
OBJS+=externals/src/gles2.o externals/src/egl.o
OBJS+=externals/src/imgui.o externals/src/imgui_draw.o externals/src/imgui_tables.o externals/src/imgui_widgets.o
OBJS+=externals/src/imgui_impl_android.o externals/src/imgui_impl_opengl3.o
OBJS+=externals/src/imgui_demo.o
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

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

%.zipped_to_apk: % | $(APK)
	touch $@
	zip -u $(APK) $<

lib/arm64-v8a/libapp.so: $(OBJS) | lib/arm64-v8a
	$(CXX) -shared $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
	llvm-strip $@

# Explicitly list java dependencies
bin/$(PACKAGE_DIR)/NativeActivity.class: java/$(PACKAGE_DIR)/NativeWrapper.java

bin/$(PACKAGE_DIR)/R.class: gen/$(PACKAGE_DIR)/R.java | bin
	javac -classpath "$(ANDROID_PLATFORM)/android.jar" -sourcepath 'src:gen' -target 1.8 -source 1.8 -d 'bin' $<

classes.dex: $(JAVA_SRCS) | $(APK)
	javac $(JAVACFLAGS) $(JAVA_SRCS) $(JAVA_GENS)
	d8 --no-desugaring --classpath bin $(JAVA_OBJS)

res_compiled.zip: $(RESOURCES)
	aapt2 compile --dir res -o $@

$(APK): res_compiled.zip AndroidManifest.xml
	rm -f $(FILES_TO_ZIP_FLAGS)
	aapt2 link res_compiled.zip -o $(APK) -I $(ANDROID_PLATFORM)/android.jar -A assets --java gen --manifest AndroidManifest.xml

$(FINAL_APK): $(APK) res_compiled.zip AndroidManifest.xml $(FILES_TO_ZIP_FLAGS)
	jarsigner -keystore debug.keystore -storepass 'android' $(APK) androiddebugkey
	zipalign -f 4 $(APK) $@

clean:
	rm -rf gen bin lib classes.dex $(FILES_TO_ZIP_FLAGS) $(APK) $(FINAL_APK) res_compiled.zip
	rm -rf $(OBJS) $(DEPS) debug-executable

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


# TODO: Use aapt2
ANDROID_PLATFORM=$(ANDROID_HOME)/platforms/android-32

PACKAGE=com.example.app
PACKAGE_DIR=$(subst .,/,$(PACKAGE))
FINAL_APK=app.apk
APK=$(FINAL_APK).unaligned

TARGET_HOST=aarch64-linux-android21
ABI=arm64-v8a
#TARGET_HOST=i686-linux-android21
#ABI=x86

CC=clang --target=$(TARGET_HOST)
CXX=clang++ --target=$(TARGET_HOST)
CFLAGS=-Wall -O0 -ggdb -funwind-tables -fPIC -fvisibility=hidden
CFLAGS+=-Wno-unused-function
CXXFLAGS=$(CFLAGS) -fno-exceptions -fno-rtti
CPPFLAGS=-MMD -Isrc -Iexternals/include
LDFLAGS=-Wl,--no-undefined
LDLIBS=-llog -landroid -lGLESv3 -lOpenSLES -lEGL -lm -static-libstdc++
 
FILES_TO_ZIP=lib/$(ABI)/libapp.so classes.dex
FILES_TO_ZIP_FLAGS=$(addsuffix .zipped_to_apk.flag,$(FILES_TO_ZIP))

RESOURCES=res/values/strings.xml res/values/style.xml res/layout/activity_main.xml

CL_RESOURCES=externals/constraintlayout/res/values/attrs.xml externals/constraintlayout/res/values/ids.xml
# Javac flags
# bootclasspath "" to avoid warnings
JAVA_SRCS=java/$(PACKAGE_DIR)/NativeWrapper.java java/$(PACKAGE_DIR)/NativeActivity.java java/$(PACKAGE_DIR)/MainActivity.java

JAVA_GENS=gen/$(PACKAGE_DIR)/R.java

JAVA_OBJS=$(subst .java,.class,$(subst java/,bin/,$(JAVA_SRCS)) $(subst gen/,bin/,$(JAVA_GENS)))
JAVACFLAGS=-classpath $(ANDROID_PLATFORM)/android.jar:bin:externals/constraintlayout/java -bootclasspath "" -target 8 -source 8 -d 'bin'

# $(ASSETS_FILES) is only used for dependency checks (apk remade on changes)
ASSETS_FILES=$(shell find assets/ -type f)

OBJS=src/activity.o src/game.o
OBJS+=src/sound_device_opensl.o
OBJS+=src/imgui_test.o
OBJS+=src/imgui_impl_android.o src/imgui_impl_opengl3.o
OBJS+=externals/src/gles2.o externals/src/egl.o
OBJS+=externals/src/imgui.o externals/src/imgui_draw.o externals/src/imgui_tables.o externals/src/imgui_widgets.o
OBJS+=externals/src/imgui_demo.o
DEPS=$(OBJS:.o=.d)

BINARIES=lib/$(ABI)/libapp.so

.DELETE_ON_ERROR:

.PHONY: all clean run start-gdbserver install killall log

all: $(FINAL_APK)

-include $(DEPS)

bin gen lib/$(ABI):
	mkdir -p $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

%.zipped_to_apk.flag: % | $(APK)
	touch $@
	zip -u $(APK) $<

lib/$(ABI)/libapp.so: $(OBJS) | lib/$(ABI)
	$(CXX) -shared $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
#llvm-strip $@

# Explicitly list java dependencies
bin/$(PACKAGE_DIR)/NativeActivity.class: java/$(PACKAGE_DIR)/NativeWrapper.java

gen/$(PACKAGE_DIR)/R.java: $(APK)

bin/$(PACKAGE_DIR)/R.class: gen/$(PACKAGE_DIR)/R.java | bin
	javac -classpath "$(ANDROID_PLATFORM)/android.jar" -sourcepath 'src:gen' -target 1.8 -source 1.8 -d 'bin' $<

java_compiled.flag: $(JAVA_SRCS) | $(APK)
	javac $(JAVACFLAGS) $(JAVA_SRCS) $(JAVA_GENS)
	touch $@

classes.dex: java_compiled.flag
# Ugly hack to include nested classes (Class$Nested), and escape '$' character...
	d8 --no-desugaring --classpath bin $(subst $$,\$$,$(shell find bin -name *.class))

res_compiled.zip: $(RESOURCES)
	aapt2 compile --dir res -o $@

$(APK): res_compiled.zip AndroidManifest.xml $(ASSETS_FILES)
	rm -f $(FILES_TO_ZIP_FLAGS)
	aapt2 link res_compiled.zip -o $(APK) -I $(ANDROID_PLATFORM)/android.jar -A assets --java gen --manifest AndroidManifest.xml

$(FINAL_APK): $(APK) res_compiled.zip AndroidManifest.xml $(FILES_TO_ZIP_FLAGS)
	zipalign -f 4 $(APK) $@.aligned
	apksigner sign --min-sdk-version 21 --max-sdk-version 32 --ks debug.keystore --ks-pass pass:android --in $@.aligned --out $@

clean:
	rm -rf gen bin lib classes.dex java_compiled.flag $(FILES_TO_ZIP_FLAGS) $(APK) $(FINAL_APK) $(FINAL_APK).aligned $(FINAL_APK).idsig res_compiled.zip
	rm -rf $(OBJS) $(DEPS) app_process64

install: $(FINAL_APK)
	adb install -r $(FINAL_APK)

run: install
	adb shell am start-activity -n $(PACKAGE)/$(PACKAGE).NativeActivity

app_process64:
	adb pull /system/bin/app_process64

start-gdbserver: $(ANDROID_NDK_HOME)/prebuilt/android-arm64/gdbserver/gdbserver | app_process64
	-adb push $< /data/local/tmp
	-adb shell "cat /data/local/tmp/gdbserver | run-as $(PACKAGE) sh -c 'cat > /data/data/$(PACKAGE)/gdbserver && chmod 700 /data/data/$(PACKAGE)/gdbserver'"
	adb forward tcp:8123 tcp:8123
	adb shell "echo /data/data/$(PACKAGE)/gdbserver --attach localhost:8123 \`pidof $(PACKAGE)\` | run-as $(PACKAGE)"

log:
	adb logcat --pid=`adb shell pidof $(PACKAGE) | sed 's/\r//g'`

killall:
	-adb shell run-as $(PACKAGE) killall gdbserver
	-adb shell run-as $(PACKAGE) killall $(PACKAGE)

debug.keystore:
	keytool -genkey -v -keystore debug.keystore -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000

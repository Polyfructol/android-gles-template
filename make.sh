
# Env vars needed by make
export ANDROID_HOME=/home/paul/android/android-sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/23.1.7779620

# Customize path with android build tools and ndc clang
export BUILD_TOOLS=$ANDROID_HOME/build-tools/32.1.0-rc1
export PLATFORM_TOOLS=$ANDROID_HOME/platform-tools/
export CLANG=$ANDROID_HOME/ndk/23.1.7779620/toolchains/llvm/prebuilt/linux-x86_64/bin/
export PATH=$CLANG:$BUILD_TOOLS:$PLATFORM_TOOLS:$PATH

#d8 --help
make $@
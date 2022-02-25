#!/bin/bash

# Env vars needed by make
export JAVA_HOME=/var/lib/snapd/snap/android-studio/119/android-studio/jre
export ANDROID_HOME=/home/paul/Android/Sdk
#export ANDROID_HOME=/home/paul/android/android-sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/23.1.7779620

# Customize path with android build tools and ndc clang
export CMDLINE_TOOLS=$ANDROID_HOME/cmdline-tools/latest
export BUILD_TOOLS=$ANDROID_HOME/build-tools/32.1.0-rc1
export PLATFORM_TOOLS=$ANDROID_HOME/platform-tools/
export CLANG=$ANDROID_HOME/ndk/23.1.7779620/toolchains/llvm/prebuilt/linux-x86_64/bin/
export PATH=$CMDLINE_TOOLS/bin:$JAVA_HOME/bin:$CLANG:$BUILD_TOOLS:$PLATFORM_TOOLS:$PATH

#!/bin/bash

NDK_VERSION=23.1.7779620

# Env vars needed by make
export JAVA_HOME=/var/lib/snapd/snap/android-studio/current/android-studio/jre
export ANDROID_HOME=/home/paul/Android/Sdk
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/$NDK_VERSION

# Customize path with android build tools and ndc clang
export CMDLINE_TOOLS=$ANDROID_HOME/cmdline-tools/latest
export BUILD_TOOLS=$ANDROID_HOME/build-tools/32.1.0-rc1
export PLATFORM_TOOLS=$ANDROID_HOME/platform-tools/
export LLVM_TOOLCHAIN=$ANDROID_HOME/ndk/$NDK_VERSION/toolchains/llvm/prebuilt/linux-x86_64
export PREBUILD_BINARIES=$ANDROID_HOME/ndk/$NDK_VERSION/prebuilt/linux-x86_64/bin/
export PATH=$PREBUILD_BINARIES:$LLVM_TOOLCHAIN/bin:$CMDLINE_TOOLS/bin:$JAVA_HOME/bin:$BUILD_TOOLS:$PLATFORM_TOOLS:$PATH

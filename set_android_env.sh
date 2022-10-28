#!/bin/bash

# (Env vars needed by make : ANDROID_HOME, ANDROID_NDK_HOME and ANDROID_PLATFORM)
# ANDROID_PLATFORM is not standardized...

# Customize Java and Android SDK root path
export JAVA_HOME=~/dev/wsl-android/jdk-19.0.1
export ANDROID_HOME=~/dev/wsl-android/android-sdk

# Customize versions here
PLATFORMS_VERSION=android-33
NDK_VERSION=23.1.7779620
BUILD_TOOLS_VERSION=33.0.0
CMDLINE_TOOLS_VERSION=latest

# Vars should not be changed from this point
# =============================================================================
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/$NDK_VERSION
# ANDROID_PLATFORM is only needed by the Makefile to target android.jar
export ANDROID_PLATFORM=$ANDROID_HOME/platforms/$PLATFORMS_VERSION

# Customize PATH with android build tools and ndk clang
CMDLINE_TOOLS=$ANDROID_HOME/cmdline-tools/latest/bin
BUILD_TOOLS=$ANDROID_HOME/build-tools/$BUILD_TOOLS_VERSION
PLATFORM_TOOLS=$ANDROID_HOME/platform-tools
LLVM_TOOLCHAIN=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin
PREBUILD_BINARIES=$ANDROID_NDK_HOME/prebuilt/linux-x86_64/bin
export PATH=$PREBUILD_BINARIES:$LLVM_TOOLCHAIN:$CMDLINE_TOOLS:$JAVA_HOME/bin:$BUILD_TOOLS:$PLATFORM_TOOLS:$PATH

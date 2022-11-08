
*(If Android and Java environment is already set, go directly to the [last section](#setup-script))*

Installation and setup of the Android environment on Linux. Tested on Linux and WSL2.

This installation is *self-contained*, so it does not pollute system environment. To remove it, just delete the parent folder.

## Prerequisites
```bash
sudo apt install make zip unzip
```

## Step by step installation
`cd` into a folder where to place JDK and Android SDK
```bash
mkdir android-env
cd android-env
```

Download **JDK** https://jdk.java.net/19/ (tar.gz) 

Download **Android Command-line tools** https://developer.android.com/studio#command-line-tools-only (zip)

Decompress them manually or following those commands
```bash
# JDK
tar xzf openjdk-19.0.1_linux-x64_bin.tar.gz
# Android
mkdir -p $(pwd)/android-sdk/cmdline-tools/latest
unzip commandlinetools-linux-8512546_latest.zip
```

Place `cmdline-tools` inside the expected folder (this way, `sdkmanager` auto-detect the sdk installation path and we won't have to download it twice)
```bash
mv cmdline-tools/* $(pwd)/android-sdk/cmdline-tools/latest
```

Prepare environment vars `JAVA_HOME` and `PATH` needed later
```bash
export JAVA_HOME=$(pwd)/jdk-19.0.1
export PATH=$(pwd)/android-sdk/cmdline-tools/latest/bin:$PATH
```

Accept Android licenses (to avoid confirmation on install)
```bash
yes | sdkmanager --licenses
```

List Android packages and choose versions that fit your needs
```bash
sdkmanager --list
```

Then install
```bash
sdkmanager --install "platforms;android-33" "platform-tools" "ndk;25.1.8937393" "build-tools;33.0.0"
```

## Setup script
Modify `set_android_env.sh` to match folders and package versions accordingly (modify `CHANGE_DIR_HERE` to the absolute dir path)
```bash
export JAVA_HOME=CHANGE_DIR_HERE/jdk-19.0.1
export ANDROID_HOME=CHANGE_DIR_HERE/android-sdk

# Customize versions here
PLATFORMS_VERSION=android-33
NDK_VERSION=25.1.8937393
BUILD_TOOLS_VERSION=33.0.0
CMDLINE_TOOLS_VERSION=latest

# ... 
```

## WSL specifics
`adb devices` cannot connnect through USB: https://stackoverflow.com/questions/60166965/adb-device-list-empty-using-wsl2



/home/paul/dev/wsl-android/android-sdk/ndk/25.1.8937393/prebuilt/android-arm64/gdbserver/gdbserver

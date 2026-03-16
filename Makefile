# Butterscotch Root Makefile

.PHONY: all glfw android apk clean local.properties

# ===[ Android SDK / NDK Detection ]===
# Priority: ANDROID_NDK > ANDROID_NDK_HOME > ANDROID_SDK_ROOT/ndk/<latest>
ifdef ANDROID_NDK
  NDK_DIR := $(ANDROID_NDK)
else ifdef ANDROID_NDK_HOME
  NDK_DIR := $(ANDROID_NDK_HOME)
else ifdef ANDROID_SDK_ROOT
  # Pick the latest NDK version installed under the SDK
  NDK_DIR := $(lastword $(sort $(wildcard $(ANDROID_SDK_ROOT)/ndk/*)))
else
  NDK_DIR :=
endif

# SDK dir (for local.properties)
ifdef ANDROID_SDK_ROOT
  SDK_DIR := $(ANDROID_SDK_ROOT)
else ifdef ANDROID_HOME
  SDK_DIR := $(ANDROID_HOME)
else
  SDK_DIR :=
endif

# ===[ Targets ]===

all: glfw

# Build GLFW desktop version
glfw:
	mkdir -p build_glfw
	cd build_glfw && cmake .. -DPLATFORM=glfw
	$(MAKE) -C build_glfw -j$$(nproc)

ps2:
	mkdir -p build_ps2
	cd build_ps2 && cmake .. -DPLATFORM=ps2
	$(MAKE) -C build_ps2 -j$$(nproc)

# Build Android native library (.so only, without Gradle)
android: _check_ndk
	mkdir -p build_android
	cd build_android && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=$(NDK_DIR)/build/cmake/android.toolchain.cmake \
		-DANDROID_ABI=arm64-v8a \
		-DANDROID_PLATFORM=android-24 \
		-DPLATFORM=android
	$(MAKE) -C build_android -j$$(nproc)

# Build Android APK (compiles native + Kotlin + packages)
apk: android/local.properties
	cd android && ./gradlew assembleDebug

# Generate android/local.properties from env vars
android/local.properties: _check_sdk _check_ndk
	@echo "sdk.dir=$(SDK_DIR)" > android/local.properties
	@echo "ndk.dir=$(NDK_DIR)" >> android/local.properties
	@echo "[local.properties] Generated: sdk=$(SDK_DIR)  ndk=$(NDK_DIR)"

# Guards
_check_ndk:
	@test -n "$(NDK_DIR)" || (echo "\nERROR: Android NDK not found.\nSet one of: ANDROID_NDK, ANDROID_NDK_HOME, or ANDROID_SDK_ROOT\n" && exit 1)
	@test -d "$(NDK_DIR)" || (echo "\nERROR: NDK directory does not exist: $(NDK_DIR)\n" && exit 1)

_check_sdk:
	@test -n "$(SDK_DIR)" || (echo "\nERROR: Android SDK not found.\nSet ANDROID_SDK_ROOT or ANDROID_HOME\n" && exit 1)
	@test -d "$(SDK_DIR)" || (echo "\nERROR: SDK directory does not exist: $(SDK_DIR)\n" && exit 1)

# Clean all builds
clean:
	rm -rf build_glfw build_android build android/app/build android/build

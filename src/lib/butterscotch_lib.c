#include "butterscotch_lib.h"
#include "data_win.h"
#include "vm.h"
#include "runner.h"
#include "runner_keyboard.h"
#include "overlay_file_system.h"
#include "ma_audio_system.h"
#include "noop_audio_system.h"
#include "gl/gl_renderer.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_ANDROID
#include <android/native_window.h>
#include <EGL/egl.h>
#else
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#endif

// ===[ Internal Context ]===

struct ButterscotchContext {
    DataWin* dataWin;
    VMContext* vm;
    Runner* runner;
    Renderer* renderer;
    AudioSystem* audioSystem;
    FileSystem* fileSystem;
    int32_t winW;
    int32_t winH;
    uint8_t* fbBuffer; // flipped (top-down) output for butterscotch_getFramebuffer
    uint8_t* rawBuffer; // GL readback (bottom-up) before flip
    int fbW;
    int fbH;
    bool isGLES;
    char* dataWinPath; // current data.win path (updated on game_change)
    char* savesPath;  // saved games path (persists across game_change)
    bool audioIsHost; // true when the host installed custom audio callbacks
    ButterscotchAudioCallbacks hostAudioCallbacks;
#ifndef PLATFORM_ANDROID
    GLFWwindow* window;
#else
    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLContext eglContext;
    EGLConfig eglConfig;
    bool usesHostWindow;
#endif
};

// ===[ GL context setup ]===

#ifdef PLATFORM_ANDROID
static bool libEnsureContext(ButterscotchContext* ctx, int w, int h, void* nativeWindow) {
    ctx->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx->eglDisplay == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(ctx->eglDisplay, NULL, NULL)) return false;

    EGLint attr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint num;
    if (!eglChooseConfig(ctx->eglDisplay, attr, &config, 1, &num) || num < 1) return false;
    ctx->eglConfig = config;

    EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    ctx->eglContext = eglCreateContext(ctx->eglDisplay, config, EGL_NO_CONTEXT, ctxAttr);
    if (ctx->eglContext == EGL_NO_CONTEXT) return false;

    if (nativeWindow) {
        ctx->eglSurface = eglCreateWindowSurface(ctx->eglDisplay, config, (EGLNativeWindowType) nativeWindow, NULL);
        ctx->usesHostWindow = true;
    } else {
        EGLint pbAttr[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE };
        ctx->eglSurface = eglCreatePbufferSurface(ctx->eglDisplay, config, pbAttr);
        ctx->usesHostWindow = false;
    }
    if (ctx->eglSurface == EGL_NO_SURFACE) return false;
    if (!eglMakeCurrent(ctx->eglDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) return false;
    ctx->isGLES = true;
    return true;
}
#else
static int libInitGlad(void) {
    glGetString = (PFNGLGETSTRINGPROC)(GLADloadproc)glfwGetProcAddress("glGetString");
    const char *version;
    if (glGetString) {
        version = (const char*)glGetString(GL_VERSION);
    } else {
        return 0;
    }
    if (version && strstr(version, "OpenGL ES")) {
        if (!gladLoadGLES2Loader((GLADloadproc) glfwGetProcAddress))
            return 0;
        return 2;
    } else {
        if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
            return 0;
        return 1;
    }
}

static bool libEnsureContext(ButterscotchContext* ctx, int w, int h, MAYBE_UNUSED void* nativeWindow) {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ctx->window = glfwCreateWindow(w, h, "Butterscotch (embedded)", NULL, NULL);
    if (!ctx->window) return false;
    glfwMakeContextCurrent(ctx->window);
    int glad_ret = libInitGlad();
    if (glad_ret == 0) return false;
    ctx->isGLES = (glad_ret == 2);
    return true;
}
#endif

// Make the GL/EGL context current on the calling thread. The host (e.g. TeiaHub) typically
// creates the context on one thread but drives step/draw on a separate game thread, so we must
// ensure the context is current here or GL calls (including FBO creation during Runner_step) fail.
static void libMakeContextCurrent(ButterscotchContext* ctx) {
#ifdef PLATFORM_ANDROID
    if (ctx->eglDisplay != EGL_NO_DISPLAY)
        eglMakeCurrent(ctx->eglDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext);
#else
    if (ctx->window) glfwMakeContextCurrent(ctx->window);
#endif
}

// ===[ Helpers ]===

static char* dirnameOf(const char* path) {
    const char* slash = strrchr(path, '/');
    if (!slash) slash = strrchr(path, '\\');
    if (!slash) return safeStrdup("./");
    size_t len = (size_t) (slash - path + 1);
    char* out = safeMalloc(len + 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

static bool libGetWindowSize(MAYBE_UNUSED int32_t* outW, MAYBE_UNUSED int32_t* outH) {
    // Sizes are passed explicitly into Runner_beginFrame; this hook is unused.
    return false;
}

static void libSetWindowTitle(MAYBE_UNUSED const char* title) {
    // No window to title in embedded mode.
}

// ===[ CallbackAudioSystem (host-provided audio) ]===
// Defined early so the game (re)load helper can reuse it across game_change.

typedef struct {
    AudioSystem base;
    ButterscotchAudioCallbacks callbacks;
} CallbackAudioSystem;

static void callbackInit(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED DataWin* dataWin, MAYBE_UNUSED FileSystem* fileSystem) {}
static void callbackDestroy(AudioSystem* audio) { free(audio); }
static void callbackUpdate(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED float deltaTime) {}

static int32_t callbackPlaySound(AudioSystem* audio, int32_t soundIndex, MAYBE_UNUSED int32_t priority, bool loop) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.playSound != NULL)
        return cb->callbacks.playSound(cb->callbacks.userData, soundIndex, loop);
    return -1;
}

static void callbackStopSound(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.stopSound != NULL)
        cb->callbacks.stopSound(cb->callbacks.userData, soundOrInstance);
}

static void callbackStopAll(AudioSystem* audio) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.stopAll != NULL)
        cb->callbacks.stopAll(cb->callbacks.userData);
}

static bool callbackIsPlaying(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.isPlaying != NULL)
        return cb->callbacks.isPlaying(cb->callbacks.userData, soundOrInstance);
    return false;
}

static void callbackPauseSound(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.pauseSound != NULL)
        cb->callbacks.pauseSound(cb->callbacks.userData, soundOrInstance);
}

static void callbackResumeSound(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.resumeSound != NULL)
        cb->callbacks.resumeSound(cb->callbacks.userData, soundOrInstance);
}

static void callbackPauseAll(AudioSystem* audio) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.pauseAll != NULL)
        cb->callbacks.pauseAll(cb->callbacks.userData);
}

static void callbackResumeAll(AudioSystem* audio) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.resumeAll != NULL)
        cb->callbacks.resumeAll(cb->callbacks.userData);
}

static void callbackSetSoundGain(AudioSystem* audio, int32_t soundOrInstance, float gain, uint32_t timeMs) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.setSoundGain != NULL)
        cb->callbacks.setSoundGain(cb->callbacks.userData, soundOrInstance, gain, timeMs);
}

static float callbackGetSoundGain(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.getSoundGain != NULL)
        return cb->callbacks.getSoundGain(cb->callbacks.userData, soundOrInstance);
    return 1.0f;
}

static void callbackSetSoundPitch(AudioSystem* audio, int32_t soundOrInstance, float pitch) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.setSoundPitch != NULL)
        cb->callbacks.setSoundPitch(cb->callbacks.userData, soundOrInstance, pitch);
}

static float callbackGetSoundPitch(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.getSoundPitch != NULL)
        return cb->callbacks.getSoundPitch(cb->callbacks.userData, soundOrInstance);
    return 1.0f;
}

static float callbackGetTrackPosition(AudioSystem* audio, int32_t soundOrInstance) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.getTrackPosition != NULL)
        return cb->callbacks.getTrackPosition(cb->callbacks.userData, soundOrInstance);
    return 0.0f;
}

static void callbackSetTrackPosition(AudioSystem* audio, int32_t soundOrInstance, float positionSeconds) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.setTrackPosition != NULL)
        cb->callbacks.setTrackPosition(cb->callbacks.userData, soundOrInstance, positionSeconds);
}

static void callbackSetMasterGain(AudioSystem* audio, float gain) {
    CallbackAudioSystem* cb = (CallbackAudioSystem*) audio;
    if (cb->callbacks.setMasterGain != NULL)
        cb->callbacks.setMasterGain(cb->callbacks.userData, gain);
}

static void callbackSetChannelCount(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED int32_t count) {}
static void callbackGroupLoad(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED int32_t groupIndex) {}
static bool callbackGroupIsLoaded(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED int32_t groupIndex) { return true; }
static int32_t callbackCreateStream(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED const char* filename) { return -1; }
static bool callbackDestroyStream(MAYBE_UNUSED AudioSystem* audio, MAYBE_UNUSED int32_t streamIndex) { return false; }

static AudioSystemVtable callbackVtable = {
    .init = callbackInit,
    .destroy = callbackDestroy,
    .update = callbackUpdate,
    .playSound = callbackPlaySound,
    .stopSound = callbackStopSound,
    .stopAll = callbackStopAll,
    .isPlaying = callbackIsPlaying,
    .pauseSound = callbackPauseSound,
    .resumeSound = callbackResumeSound,
    .pauseAll = callbackPauseAll,
    .resumeAll = callbackResumeAll,
    .setSoundGain = callbackSetSoundGain,
    .getSoundGain = callbackGetSoundGain,
    .setSoundPitch = callbackSetSoundPitch,
    .getSoundPitch = callbackGetSoundPitch,
    .getTrackPosition = callbackGetTrackPosition,
    .setTrackPosition = callbackSetTrackPosition,
    .setMasterGain = callbackSetMasterGain,
    .setChannelCount = callbackSetChannelCount,
    .groupLoad = callbackGroupLoad,
    .groupIsLoaded = callbackGroupIsLoaded,
    .createStream = callbackCreateStream,
    .destroyStream = callbackDestroyStream,
};

static AudioSystem* libCreateAudio(ButterscotchContext* ctx, DataWin* dataWin) {
    if (ctx->audioIsHost) {
        CallbackAudioSystem* cb = calloc(1, sizeof(CallbackAudioSystem));
        cb->base.vtable = &callbackVtable;
        cb->callbacks = ctx->hostAudioCallbacks;
        return (AudioSystem*) cb;
    }
    AudioSystem* audioSystem = (AudioSystem*) MaAudioSystem_create(dataWin);
    if (audioSystem == NULL) {
        fprintf(stderr, "Butterscotch: MaAudioSystem_create returned NULL; falling back to silent audio\n");
        audioSystem = (AudioSystem*) NoopAudioSystem_create();
    }
    return audioSystem;
}

static DataWinParserOptions libParseOptions(void) {
    DataWinParserOptions options = {0};
    options.parseGen8 = true;
    options.parseOptn = true;
    options.parseLang = true;
    options.parseExtn = true;
    options.parseSond = true;
    options.parseAgrp = true;
    options.parseSprt = true;
    options.parseBgnd = true;
    options.parsePath = true;
    options.parseScpt = true;
    options.parseGlob = true;
    options.parseShdr = true;
    options.parseFont = true;
    options.parseTmln = true;
    options.parseObjt = true;
    options.parseRoom = true;
    options.parseTpag = true;
    options.parseCode = true;
    options.parseVari = true;
    options.parseFunc = true;
    options.parseStrg = true;
    options.parseTxtr = true;
    options.parseAudo = true;
    options.skipLoadingPreciseMasksForNonPreciseSprites = true;
    options.loadType = DATAWINLOADTYPE_LOAD_IN_MEMORY_AHEAD_OF_TIME;
    options.lazyLoadRooms = false;
    options.eagerlyLoadedRooms = NULL;
    return options;
}

// Resize the GL/EGL surface to match the (possibly new) game dimensions so that
// framebuffer readback stays consistent. For host-owned windows (Android native window)
// the host resizes the surface itself, so we only manage our own offscreen surfaces.
static void libResizeContext(ButterscotchContext* ctx, int32_t w, int32_t h) {
    if (w <= 0 || h <= 0) return;
    ctx->winW = w;
    ctx->winH = h;
#ifndef PLATFORM_ANDROID
    if (ctx->window) glfwSetWindowSize(ctx->window, w, h);
#else
    if (ctx->eglDisplay != EGL_NO_DISPLAY && !ctx->usesHostWindow) {
        libMakeContextCurrent(ctx);
        if (ctx->eglSurface != EGL_NO_SURFACE) eglDestroySurface(ctx->eglDisplay, ctx->eglSurface);
        EGLint pbAttr[] = { EGL_WIDTH, w, EGL_HEIGHT, h, EGL_NONE };
        ctx->eglSurface = eglCreatePbufferSurface(ctx->eglDisplay, ctx->eglConfig, pbAttr);
        if (ctx->eglSurface == EGL_NO_SURFACE) return;
        eglMakeCurrent(ctx->eglDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext);
    }
#endif
}

// Load (or reload, on game_change) a data.win into the context. The GL context and
// audio-callback configuration are preserved; the previous game's resources are freed.
static bool libLoadGame(ButterscotchContext* ctx, const char* dataWinPath, const char* savesPath) {
    libMakeContextCurrent(ctx);

    DataWin* dataWin = DataWin_parse(dataWinPath, libParseOptions());
    if (dataWin == NULL) {
        fprintf(stderr, "Butterscotch: Failed to parse data.win for game_change: %s\n", dataWinPath);
        return false;
    }

    VMContext* vm = VM_create(dataWin);

    char* bundleDir = dirnameOf(dataWinPath);
    FileSystem* fileSystem = (FileSystem*) OverlayFileSystem_create(bundleDir, savesPath ? savesPath : bundleDir);
    free(bundleDir);

    Renderer* renderer = GLRenderer_create();
    ((GLRenderer*) renderer)->hostFramebuffer = 0; // render into the (offscreen / host) default framebuffer
    ((GLRenderer*) renderer)->isGLES = ctx->isGLES;

    AudioSystem* audioSystem = libCreateAudio(ctx, dataWin);

    Runner* runner = Runner_create(dataWin, vm, renderer, fileSystem, audioSystem);
    runner->getWindowSize = libGetWindowSize;
    runner->setWindowTitle = libSetWindowTitle;
    runner->windowHasFocus = NULL;

    // Tear down the previous game's state. The GL context itself stays alive.
    if (ctx->runner != NULL) {
        ctx->runner->audioSystem->vtable->destroy(ctx->runner->audioSystem);
        ctx->renderer->vtable->destroy(ctx->renderer);
        Runner_free(ctx->runner);
        OverlayFileSystem_destroy((OverlayFileSystem*) ctx->fileSystem);
        VM_free(ctx->vm);
        DataWin_free(ctx->dataWin);
    }

    ctx->dataWin = dataWin;
    ctx->vm = vm;
    ctx->renderer = renderer;
    ctx->audioSystem = audioSystem;
    ctx->fileSystem = fileSystem;
    ctx->runner = runner;

    libResizeContext(ctx, (int32_t) dataWin->gen8.defaultWindowWidth, (int32_t) dataWin->gen8.defaultWindowHeight);

    Runner_initFirstRoom(runner);
    fprintf(stderr, "Butterscotch: Loaded \"%s\" (%d) [WAD Version %u]\n",
        dataWin->gen8.name, dataWin->gen8.gameID, dataWin->gen8.wadVersion);
    return true;
}

// Minimal parse to discover the default window size for GL context creation.
static bool libProbeSize(const char* dataWinPath, int32_t* outW, int32_t* outH) {
    DataWinParserOptions options = {0};
    options.parseGen8 = true;
    DataWin* dataWin = DataWin_parse(dataWinPath, options);
    if (dataWin == NULL) return false;
    *outW = (int32_t) dataWin->gen8.defaultWindowWidth;
    *outH = (int32_t) dataWin->gen8.defaultWindowHeight;
    DataWin_free(dataWin);
    return true;
}

// ===[ Lifecycle ]===

static ButterscotchContext* createCommon(const char* dataWinPath, const char* savesPath, void* nativeWindow) {
    if (dataWinPath == NULL) return NULL;

    ButterscotchContext* ctx = safeMalloc(sizeof(ButterscotchContext));
    memset(ctx, 0, sizeof(*ctx));
    ctx->dataWinPath = safeStrdup(dataWinPath);
    ctx->savesPath = savesPath ? safeStrdup(savesPath) : NULL;

    // Probe the default window size so we can size the GL context before parsing the
    // full data.win (libLoadGame does the full, heavy parse).
    if (!libProbeSize(dataWinPath, &ctx->winW, &ctx->winH)) {
        fprintf(stderr, "Butterscotch: Failed to parse data.win\n");
        free(ctx->dataWinPath);
        free(ctx->savesPath);
        free(ctx);
        return NULL;
    }

    fprintf(stderr, "Butterscotch: Loading %s...\n", dataWinPath);

    // GL context must be current before GLRenderer_create (it compiles shaders / creates GL objects).
    if (!libEnsureContext(ctx, ctx->winW, ctx->winH, nativeWindow)) {
        fprintf(stderr, "Butterscotch: Failed to create GL context\n");
        free(ctx->dataWinPath);
        free(ctx->savesPath);
        free(ctx);
        return NULL;
    }

    if (!libLoadGame(ctx, dataWinPath, ctx->savesPath)) {
        fprintf(stderr, "Butterscotch: Failed to load game\n");
#ifndef PLATFORM_ANDROID
        if (ctx->window) {
            glfwDestroyWindow(ctx->window);
            glfwTerminate();
        }
#else
        if (ctx->eglDisplay != EGL_NO_DISPLAY) {
            if (ctx->eglSurface != EGL_NO_SURFACE) eglDestroySurface(ctx->eglDisplay, ctx->eglSurface);
            if (ctx->eglContext != EGL_NO_CONTEXT) eglDestroyContext(ctx->eglDisplay, ctx->eglContext);
            eglTerminate(ctx->eglDisplay);
        }
#endif
        free(ctx->dataWinPath);
        free(ctx->savesPath);
        free(ctx);
        return NULL;
    }

    // Release the GL context from the main thread so the game thread can claim it.
    // On Wayland/GLFW, a context tied to one thread can block make-current on another.
#ifndef PLATFORM_ANDROID
    glfwMakeContextCurrent(NULL);
#endif

    fprintf(stderr, "Butterscotch: Initialized successfully\n");
    return ctx;
}

BUTTERSCOTCH_API ButterscotchContext* butterscotch_create(const char* dataWinPath) {
    return createCommon(dataWinPath, NULL, NULL);
}

BUTTERSCOTCH_API ButterscotchContext* butterscotch_createWithSaves(const char* dataWinPath, const char* savesPath) {
    return createCommon(dataWinPath, savesPath, NULL);
}

BUTTERSCOTCH_API ButterscotchContext* butterscotch_create_with_native_window(const char* dataWinPath, const char* savesPath, void* nativeWindow) {
    return createCommon(dataWinPath, savesPath, nativeWindow);
}

BUTTERSCOTCH_API void butterscotch_free(ButterscotchContext* ctx) {
    if (ctx == NULL) return;

    ctx->audioSystem->vtable->destroy(ctx->audioSystem);
    ctx->renderer->vtable->destroy(ctx->renderer);
    Runner_free(ctx->runner);
    VM_free(ctx->vm);
    DataWin_free(ctx->dataWin);
    OverlayFileSystem_destroy((OverlayFileSystem*) ctx->fileSystem);

#ifndef PLATFORM_ANDROID
    if (ctx->window) {
        glfwMakeContextCurrent(ctx->window);
        glfwDestroyWindow(ctx->window);
        glfwTerminate();
    }
#else
    if (ctx->eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(ctx->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx->eglSurface != EGL_NO_SURFACE) eglDestroySurface(ctx->eglDisplay, ctx->eglSurface);
        if (ctx->eglContext != EGL_NO_CONTEXT) eglDestroyContext(ctx->eglDisplay, ctx->eglContext);
        eglTerminate(ctx->eglDisplay);
    }
#endif

    free(ctx->fbBuffer);
    free(ctx->rawBuffer);
    free(ctx->dataWinPath);
    free(ctx->savesPath);
    free(ctx);
}

BUTTERSCOTCH_API void butterscotch_resize(ButterscotchContext* ctx, int32_t width, int32_t height) {
    if (ctx == NULL || width <= 0 || height <= 0) return;
    ctx->winW = width;
    ctx->winH = height;
#ifndef PLATFORM_ANDROID
    if (ctx->window) glfwSetWindowSize(ctx->window, width, height);
#endif
    // Android host window surfaces resize with the ANativeWindow automatically.
}

BUTTERSCOTCH_API void butterscotch_beginFrame(ButterscotchContext* ctx) {
    if (ctx == NULL) return;
    RunnerKeyboard_beginFrame(ctx->runner->keyboard);
}

// Tokenize a raw launch-parameters string (like the desktop runner's extractRunnerArguments).
static char** libExtractRunnerArguments(const char* raw, int* outCount) {
    char* copy = safeStrdup(raw);
    char* saveptr = NULL;
    char** arr = NULL;
    int count = 0;
    char* token = strtok_r(copy, " \t\r\n", &saveptr);
    while (token != NULL) {
        arr = realloc(arr, sizeof(char*) * (size_t) (count + 1));
        arr[count++] = safeStrdup(token);
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(copy);
    *outCount = count;
    return arr;
}

// Handle a pending game_change request (set by the GML game_change builtin). Rebuilds the
// runner with a new data.win, reusing the existing GL context and audio configuration.
static void libHandleGameChange(ButterscotchContext* ctx) {
    char* nextWorkingDirectory = ctx->runner->pendingWorkingDirectory;
    char* nextLaunchParameters = ctx->runner->pendingLaunchParameters;
    ctx->runner->pendingWorkingDirectory = NULL;
    ctx->runner->pendingLaunchParameters = NULL;

    int argCount = 0;
    char** newArguments = libExtractRunnerArguments(nextLaunchParameters, &argCount);

    // Extract the data.win filename from the "-game <file>" entry in the launch parameters.
    char* dataWinFilename = NULL;
    for (int i = 0; i < argCount - 1; i++) {
        if (strcmp(newArguments[i], "-game") == 0) {
            dataWinFilename = newArguments[i + 1];
            break;
        }
    }

    if (dataWinFilename == NULL) {
        fprintf(stderr, "Butterscotch: game_change launch parameters '%s' did not contain a '-game <file>' entry! Ignoring.\n", nextLaunchParameters);
        for (int i = 0; i < argCount; i++) free(newArguments[i]);
        free(newArguments);
        free(nextWorkingDirectory);
        free(nextLaunchParameters);
        return;
    }

    // The pendingWorkingDirectory has a leading slash (e.g. "/chapter3"). Resolve the new
    // data.win path relative to the current one's parent directory.
    char* parentDir = safeStrdup(ctx->dataWinPath);
    {
        char* lastSlash = strrchr(parentDir, '/');
        char* lastBackslash = strrchr(parentDir, '\\');
        char* sep = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
        if (sep != NULL) {
            *sep = '\0';
        } else {
            parentDir[0] = '.';
            parentDir[1] = '\0';
        }
    }

    size_t newPathLen = strlen(parentDir) + strlen(nextWorkingDirectory) + 1 + strlen(dataWinFilename) + 1;
    char* newPath = (char*) safeMalloc(newPathLen);
    snprintf(newPath, newPathLen, "%s%s/%s", parentDir, nextWorkingDirectory, dataWinFilename);
    free(parentDir);

    // Build the new game args: keep a placeholder argv[0], then append the parsed launch args.
    char** newGameArgs = (char**) safeMalloc(sizeof(char*) * (size_t) (argCount + 1));
    newGameArgs[0] = safeStrdup(ctx->dataWinPath);
    for (int i = 0; i < argCount; i++) newGameArgs[i + 1] = safeStrdup(newArguments[i]);

    free(ctx->dataWinPath);
    ctx->dataWinPath = newPath;

    if (!libLoadGame(ctx, newPath, ctx->savesPath)) {
        fprintf(stderr, "Butterscotch: game_change failed to load %s; shutting down.\n", newPath);
        ctx->runner->shouldExit = true;
    } else {
        Runner_setGameArgs(ctx->runner, newGameArgs, argCount + 1);
    }

    for (int i = 0; i <= argCount; i++) free(newGameArgs[i]);
    free(newGameArgs);
    for (int i = 0; i < argCount; i++) free(newArguments[i]);
    free(newArguments);
    free(nextWorkingDirectory);
    free(nextLaunchParameters);
}

BUTTERSCOTCH_API void butterscotch_step(ButterscotchContext* ctx) {
    if (ctx == NULL) return;
    libMakeContextCurrent(ctx);
    if (ctx->runner->pendingWorkingDirectory != NULL && ctx->runner->pendingLaunchParameters != NULL) {
        libHandleGameChange(ctx);
        if (ctx->runner->shouldExit) return;
    }
    Runner_step(ctx->runner);
    ctx->audioSystem->vtable->update(ctx->audioSystem, 1.0f / 30.0f);
}

BUTTERSCOTCH_API void butterscotch_draw(ButterscotchContext* ctx) {
    if (ctx == NULL) return;
    libMakeContextCurrent(ctx);

    Runner* runner = ctx->runner;
    int32_t winW = ctx->winW;
    int32_t winH = ctx->winH;
    Gen8* gen8 = &runner->dataWin->gen8;

    if (!runner->appSurfaceEnabled) {
        runner->applicationWidth = winW;
        runner->applicationHeight = winH;
        runner->usingAppSurface = false;
    } else {
        if (runner->applicationWidth <= 0 || runner->applicationHeight <= 0) {
            runner->applicationWidth = (int32_t) gen8->defaultWindowWidth;
            runner->applicationHeight = (int32_t) gen8->defaultWindowHeight;
        }
        runner->usingAppSurface = true;
    }

    int32_t gameW = runner->applicationWidth;
    int32_t gameH = runner->applicationHeight;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Runner_drawPre(runner, winW, winH);
    Runner_beginFrame(runner, gameW, gameH, winW, winH, winW, winH);
    Runner_updateMousePosition(runner, winW, winH, 0.0, 0.0);
    Runner_drawViews(runner, gameW, gameH, false);
    runner->renderer->vtable->endFrameInit(runner->renderer);
    Runner_drawPost(runner, winW, winH);
    runner->renderer->vtable->endFrameEnd(runner->renderer);
    Runner_drawGUI(runner, winW, winH, gameW, gameH);
    Runner_handlePendingRoomChange(runner);

#ifdef PLATFORM_ANDROID
    if (ctx->usesHostWindow) eglSwapBuffers(ctx->eglDisplay, ctx->eglSurface);
#endif
}

BUTTERSCOTCH_API const uint8_t* butterscotch_getFramebuffer(ButterscotchContext* ctx) {
    if (ctx == NULL) return NULL;
    libMakeContextCurrent(ctx);
    int w = ctx->winW;
    int h = ctx->winH;
    if (ctx->rawBuffer == NULL || ctx->fbW != w || ctx->fbH != h) {
        free(ctx->rawBuffer);
        free(ctx->fbBuffer);
        ctx->rawBuffer = malloc((size_t) w * h * 4);
        ctx->fbBuffer = malloc((size_t) w * h * 4);
        ctx->fbW = w;
        ctx->fbH = h;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ctx->rawBuffer);

    // GL framebuffers are bottom-up; TeiaHub expects top-down scanline order.
    size_t row = (size_t) w * 4;
    for (int y = 0; y < h; y++) {
        memcpy(ctx->fbBuffer + (size_t) (h - 1 - y) * row,
               ctx->rawBuffer + (size_t) y * row,
               row);
    }
    return ctx->fbBuffer;
}

BUTTERSCOTCH_API int32_t butterscotch_getFramebufferWidth(ButterscotchContext* ctx) {
    if (ctx == NULL) return 0;
    return ctx->winW;
}

BUTTERSCOTCH_API int32_t butterscotch_getFramebufferHeight(ButterscotchContext* ctx) {
    if (ctx == NULL) return 0;
    return ctx->winH;
}

BUTTERSCOTCH_API int32_t butterscotch_getRoomSpeed(ButterscotchContext* ctx) {
    if (ctx == NULL) return 30;
    if (ctx->runner->currentRoom == NULL) return 30;
    return ctx->runner->currentRoom->speed;
}

BUTTERSCOTCH_API bool butterscotch_shouldExit(ButterscotchContext* ctx) {
    if (ctx == NULL) return true;
    return ctx->runner->shouldExit;
}

BUTTERSCOTCH_API void butterscotch_keyDown(ButterscotchContext* ctx, int32_t keyCode) {
    if (ctx == NULL) return;
    RunnerKeyboard_onKeyDown(ctx->runner->keyboard, keyCode);
}

BUTTERSCOTCH_API void butterscotch_keyUp(ButterscotchContext* ctx, int32_t keyCode) {
    if (ctx == NULL) return;
    RunnerKeyboard_onKeyUp(ctx->runner->keyboard, keyCode);
}

// ===[ CallbackAudioSystem ]===

BUTTERSCOTCH_API void butterscotch_setAudioCallbacks(ButterscotchContext* ctx, ButterscotchAudioCallbacks* callbacks) {
    if (ctx == NULL || callbacks == NULL) return;

    ctx->audioSystem->vtable->destroy(ctx->audioSystem);

    CallbackAudioSystem* cb = calloc(1, sizeof(CallbackAudioSystem));
    cb->base.vtable = &callbackVtable;
    cb->callbacks = *callbacks;

    ctx->audioSystem = (AudioSystem*) cb;
    ctx->runner->audioSystem = ctx->audioSystem;

    // Remember the host audio config so game_change can rebuild it for the new game.
    ctx->audioIsHost = true;
    ctx->hostAudioCallbacks = *callbacks;
}

// ===[ Sound Info API ]===

BUTTERSCOTCH_API int32_t butterscotch_getSoundCount(ButterscotchContext* ctx) {
    if (ctx == NULL) return 0;
    return (int32_t) ctx->dataWin->sond.count;
}

BUTTERSCOTCH_API void butterscotch_getSoundInfo(ButterscotchContext* ctx, int32_t soundIndex, ButterscotchSoundInfo* outInfo) {
    if (ctx == NULL || outInfo == NULL) return;
    if (0 > soundIndex || soundIndex >= (int32_t) ctx->dataWin->sond.count) return;

    Sound* snd = &ctx->dataWin->sond.sounds[soundIndex];
    outInfo->name = snd->name;
    outInfo->file = snd->file;
    outInfo->isEmbedded = (snd->flags & 0x01) != 0;
    outInfo->volume = snd->volume;
    outInfo->pitch = snd->pitch;
}

BUTTERSCOTCH_API const uint8_t* butterscotch_getSoundData(ButterscotchContext* ctx, int32_t soundIndex, int32_t* outSize) {
    if (outSize != NULL) *outSize = 0;
    if (ctx == NULL) return NULL;
    if (0 > soundIndex || soundIndex >= (int32_t) ctx->dataWin->sond.count) return NULL;

    Sound* snd = &ctx->dataWin->sond.sounds[soundIndex];
    if (0 > snd->audioFile || snd->audioFile >= (int32_t) ctx->dataWin->audo.count) return NULL;

    AudioEntry* entry = &ctx->dataWin->audo.entries[snd->audioFile];
    if (outSize != NULL) *outSize = (int32_t) entry->dataSize;
    return entry->data;
}

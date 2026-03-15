#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_win.h"
#include "vm.h"
#include "runner.h"
#include "runner_keyboard.h"
#include "gl_renderer.h"

// VK_LBUTTON is the Windows VK code for left mouse button (1)
// Defined here since this isn't a Windows build
#ifndef VK_LBUTTON
#define VK_LBUTTON 1
#endif

#define LOG_TAG "Butterscotch"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static DataWin* globalDataWin = NULL;
static VMContext* globalVm = NULL;
static Runner* globalRunner = NULL;
static Renderer* globalRenderer = NULL;

JNIEXPORT void JNICALL
Java_com_butterscotch_ButterscotchNative_nativeInit(JNIEnv *env, jobject thiz, jstring dataPathStr, jboolean debugMode, jboolean headlessMode) {
    const char *dataPath = (*env)->GetStringUTFChars(env, dataPathStr, NULL);
    LOGI("Loading %s...", dataPath);

    globalDataWin = DataWin_parse(
        dataPath,
        (DataWinParserOptions) {
            .parseGen8 = true,
            .parseOptn = true,
            .parseLang = true,
            .parseExtn = true,
            .parseSond = true,
            .parseAgrp = true,
            .parseSprt = true,
            .parseBgnd = true,
            .parsePath = true,
            .parseScpt = true,
            .parseGlob = true,
            .parseShdr = true,
            .parseFont = true,
            .parseTmln = true,
            .parseObjt = true,
            .parseRoom = true,
            .parseTpag = true,
            .parseCode = true,
            .parseVari = true,
            .parseFunc = true,
            .parseStrg = true,
            .parseTxtr = true,
            .parseAudo = true,
            .skipLoadingPreciseMasksForNonPreciseSprites = true
        }
    );

    (*env)->ReleaseStringUTFChars(env, dataPathStr, dataPath);

    if (!globalDataWin) {
        LOGE("Failed to parse data.win");
        return;
    }

    LOGI("Loaded \"%s\" (%d) successfully!", globalDataWin->gen8.name, globalDataWin->gen8.gameID);

    globalVm = VM_create(globalDataWin);
    
    // We don't disable fixed seed here, keep it random as we don't handle CLI args
    
    globalRunner = Runner_create(globalDataWin, globalVm);
    globalRunner->debugMode = debugMode;

    // Initialize renderer
    globalRenderer = GLRenderer_create();
    globalRenderer->vtable->init(globalRenderer, globalDataWin);
    globalRunner->renderer = globalRenderer;

    Runner_initFirstRoom(globalRunner);
}

JNIEXPORT void JNICALL
Java_com_butterscotch_ButterscotchNative_nativeResize(JNIEnv *env, jobject thiz, jint width, jint height) {
    // We could store the width/height to be passed to renderer beginFrame
    LOGI("Resized to %d x %d", width, height);
}

JNIEXPORT jboolean JNICALL
Java_com_butterscotch_ButterscotchNative_nativeStep(JNIEnv *env, jobject thiz) {
    if (!globalRunner || globalRunner->shouldExit) {
        // Cleanup
        if (globalRenderer) {
            globalRenderer->vtable->destroy(globalRenderer);
            globalRenderer = NULL;
        }
        if (globalRunner) {
            Runner_free(globalRunner);
            globalRunner = NULL;
        }
        if (globalVm) {
            VM_free(globalVm);
            globalVm = NULL;
        }
        if (globalDataWin) {
            DataWin_free(globalDataWin);
            globalDataWin = NULL;
        }
        return JNI_FALSE;
    }

    RunnerKeyboard_beginFrame(globalRunner->keyboard);
    
    // Run logic
    Runner_step(globalRunner);

    Room* activeRoom = globalRunner->currentRoom;

    // We fetch current framebuffer sizes. In Android GLES, it's bound.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int fbWidth = viewport[2];
    int fbHeight = viewport[3];

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    int32_t gameW = (int32_t) globalDataWin->gen8.defaultWindowWidth;
    int32_t gameH = (int32_t) globalDataWin->gen8.defaultWindowHeight;

    globalRenderer->vtable->beginFrame(globalRenderer, gameW, gameH, fbWidth, fbHeight);

    // Clear FBO
    if (globalRunner->drawBackgroundColor) {
        int rInt = BGR_R(globalRunner->backgroundColor);
        int gInt = BGR_G(globalRunner->backgroundColor);
        int bInt = BGR_B(globalRunner->backgroundColor);
        glClearColor(rInt / 255.0f, gInt / 255.0f, bInt / 255.0f, 1.0f);
    } else {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    bool viewsEnabled = (activeRoom->flags & 1) != 0;
    bool anyViewRendered = false;

    if (viewsEnabled) {
        for(int vi = 0; vi < 8; vi++) {
            if (!activeRoom->views[vi].enabled) continue;

            int32_t viewX = activeRoom->views[vi].viewX;
            int32_t viewY = activeRoom->views[vi].viewY;
            int32_t viewW = activeRoom->views[vi].viewWidth;
            int32_t viewH = activeRoom->views[vi].viewHeight;
            int32_t portX = activeRoom->views[vi].portX;
            int32_t portY = activeRoom->views[vi].portY;
            int32_t portW = activeRoom->views[vi].portWidth;
            int32_t portH = activeRoom->views[vi].portHeight;
            float viewAngle = globalRunner->viewAngles[vi];

            globalRunner->viewCurrent = vi;
            globalRenderer->vtable->beginView(globalRenderer, viewX, viewY, viewW, viewH, portX, portY, portW, portH, viewAngle);

            Runner_draw(globalRunner);

            globalRenderer->vtable->endView(globalRenderer);
            anyViewRendered = true;
        }
    }

    if (!anyViewRendered) {
        globalRunner->viewCurrent = 0;
        globalRenderer->vtable->beginView(globalRenderer, 0, 0, gameW, gameH, 0, 0, gameW, gameH, 0.0f);
        Runner_draw(globalRunner);
        globalRenderer->vtable->endView(globalRenderer);
    }

    globalRunner->viewCurrent = 0;
    globalRenderer->vtable->endFrame(globalRenderer);

    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_butterscotch_ButterscotchNative_nativeTouch(JNIEnv *env, jobject thiz, jint pointerId, jfloat x, jfloat y, jint action) {
    if (!globalRunner || !globalRunner->keyboard) return;

    // Simulate mouse click on touch. Just a basic implementation.
    // 0 = DOWN, 1 = MOVE, 2 = UP
    if (action == 0) {
        RunnerKeyboard_onKeyDown(globalRunner->keyboard, VK_LBUTTON);
        // Maybe update mouse positions? This is engine-dependent on how the variables are handled
    } else if (action == 2) {
        RunnerKeyboard_onKeyUp(globalRunner->keyboard, VK_LBUTTON);
    }
}

JNIEXPORT void JNICALL
Java_com_butterscotch_ButterscotchNative_nativeKey(JNIEnv *env, jobject thiz, jint keyCode, jboolean down) {
    if (!globalRunner || !globalRunner->keyboard) return;

    if (down) {
        RunnerKeyboard_onKeyDown(globalRunner->keyboard, keyCode);
    } else {
        RunnerKeyboard_onKeyUp(globalRunner->keyboard, keyCode);
    }
}

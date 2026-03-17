#include <jni.h>
#include <android/log.h>
#include <GLES3/gl3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glfw_file_system.h"

#include <time.h>
#include "data_win.h"
#include "vm.h"
#include "runner.h"
#include "runner_keyboard.h"
#include "gl_renderer.h"
#include "android/gl_renderer.h"

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
static GlfwFileSystem* glfwFileSystem = NULL;
static int32_t s_windowWidth = 0;
static int32_t s_windowHeight = 0;
static struct timespec s_lastFrameTime = {0, 0};

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
    
    // Initialize the file system
    GlfwFileSystem* glfwFileSystem = GlfwFileSystem_create(dataPath);

    // We don't disable fixed seed here, keep it random as we don't handle CLI args
    
    globalRunner = Runner_create(globalDataWin, globalVm, (FileSystem*) glfwFileSystem);
    globalRunner->debugMode = debugMode;

    // Initialize renderer
    globalRenderer = GLRenderer_create();
    globalRenderer->vtable->init(globalRenderer, globalDataWin);
    globalRunner->renderer = globalRenderer;

    Runner_initFirstRoom(globalRunner);
}

JNIEXPORT void JNICALL
Java_com_butterscotch_ButterscotchNative_nativeResize(JNIEnv *env, jobject thiz, jint width, jint height) {
    s_windowWidth = width;
    s_windowHeight = height;
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
        if (glfwFileSystem) {
            GlfwFileSystem_destroy(glfwFileSystem);
            glfwFileSystem = NULL;
        }
        s_lastFrameTime.tv_sec = 0;
        s_lastFrameTime.tv_nsec = 0;
        return JNI_FALSE;
    }

    // Limit frame rate to room speed
    if (globalRunner->currentRoom->speed > 0) {
        double targetFrameTime = 1.0 / (double) globalRunner->currentRoom->speed;
        
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if (s_lastFrameTime.tv_sec == 0) {
            s_lastFrameTime = now;
        } else {
            double elapsed = (double) (now.tv_sec - s_lastFrameTime.tv_sec) + 
                            (double) (now.tv_nsec - s_lastFrameTime.tv_nsec) / 1e9;
            
            double remaining = targetFrameTime - elapsed;
            if (remaining > 0) {
                // Sleep for most of the remaining time
                if (remaining > 0.002) {
                    struct timespec ts;
                    ts.tv_sec = (time_t) (remaining - 0.001);
                    ts.tv_nsec = (long) ((remaining - 0.001 - (double) ts.tv_sec) * 1e9);
                    nanosleep(&ts, NULL);
                }
                
                // Spin-wait for precision
                while (1) {
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    elapsed = (double) (now.tv_sec - s_lastFrameTime.tv_sec) + 
                             (double) (now.tv_nsec - s_lastFrameTime.tv_nsec) / 1e9;
                    if (elapsed >= targetFrameTime) break;
                }
            }
            s_lastFrameTime = now;
        }
    } else {
        clock_gettime(CLOCK_MONOTONIC, &s_lastFrameTime);
    }

    RunnerKeyboard_beginFrame(globalRunner->keyboard);
    
    // Run logic
    Runner_step(globalRunner);

    Room* activeRoom = globalRunner->currentRoom;

    // Use stored window dimensions
    int fbWidth = s_windowWidth;
    int fbHeight = s_windowHeight;

    // Safety: ensure no clipping when clearing or blitting to default framebuffer
    glDisable(GL_SCISSOR_TEST);
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
    if (!globalRunner || !globalRunner->keyboard || !globalRenderer) return;

    // Map screen coordinates to game space
    GLRenderer* gl = (GLRenderer*) globalRenderer;
    
    // Map from window space [0, s_windowWidth] to game space [0, gameW]
    // Using s_windowWidth/Height as source and gl->renderOffsetX/W as source-inside-window
    float mappedX = (x - (float) gl->renderOffsetX) * ((float) gl->gameW / (float) gl->renderW);
    float mappedY = (y - (float) gl->renderOffsetY) * ((float) gl->gameH / (float) gl->renderH);

    // Update global mouse positions
    globalRunner->mouseX = (double) mappedX;
    globalRunner->mouseY = (double) mappedY;

    // Simulate mouse click on touch.
    // 0 = DOWN, 1 = MOVE, 2 = UP
    if (action == 0) {
        RunnerKeyboard_onKeyDown(globalRunner->keyboard, VK_LBUTTON);
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

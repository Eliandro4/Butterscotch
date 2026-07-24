#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*onTitleChanged)(const char* title, void* userData);
    void (*onGameSizeChanged)(int width, int height, void* userData);
    void* userData;
} ButterscotchCallbacks;

void Butterscotch_setCallbacks(ButterscotchCallbacks callbacks);

void Butterscotch_init(void);

int Butterscotch_getTargetFrameHz(void);

// out must be an array of at least 4 ints (x, y, w, h)
void Butterscotch_getViewport(int* out);

int Butterscotch_getRoomCount(void);

const char* Butterscotch_getRoomName(int roomIndex);

void Butterscotch_gotoRoom(int roomIndex);

void Butterscotch_setWidescreenHackAspectRatio(float aspectRatio);

void Butterscotch_setFreeCamera(float panX, float panY, float zoom);

void Butterscotch_setNormalizedCursorPosition(float x, float y);

void Butterscotch_setMouseButtonState(int button, bool down);

void* Butterscotch_dataWinParseLight(const char* wadPath);

void Butterscotch_dataWinFree(void* handle);

const char* Butterscotch_dataWinName(void* handle);

const char* Butterscotch_dataWinDisplayName(void* handle);

int Butterscotch_dataWinWadVersion(void* handle);

void Butterscotch_dataWinGmsVersion(void* handle, char* outBuffer, int bufferSize);

void Butterscotch_dataWinDetectedGmsVersion(void* handle, char* outBuffer, int bufferSize);

bool Butterscotch_startRunner(const char* dataWinPath, const char* savesPath, int osType, int hostFramebuffer);

void* Butterscotch_getRunningDataWinHandle(void);

long long Butterscotch_getRunnerFrameCount(void);

bool Butterscotch_isProfilerEnabled(void);

void Butterscotch_setProfilerEnabled(bool enabled);

long long Butterscotch_getProfilerStartedAtFrame(void);

long long Butterscotch_getProfilerEntriesCount(void);

const char* Butterscotch_getProfilerEntryKey(long long index);

long long Butterscotch_getProfilerEntryNanos(long long index);

long long Butterscotch_getProfilerEntryOps(long long index);

void Butterscotch_beginFrame(void);

void Butterscotch_onKeyDown(int keyCode);

void Butterscotch_onKeyUp(int keyCode);

void Butterscotch_onCharacter(int codePoint);

void Butterscotch_gamepadConnected(int device, const char* name);

void Butterscotch_gamepadDisconnected(int device);

void Butterscotch_gamepadButton(int device, int button, bool down);

void Butterscotch_gamepadAxis(int device, int axis, float value);

int Butterscotch_stepAndDraw(int winW, int winH, float deltaTime);

void Butterscotch_suspendAudio(void);

void Butterscotch_resumeAudio(void);

void Butterscotch_stopRunner(void);

#ifdef __cplusplus
}
#endif

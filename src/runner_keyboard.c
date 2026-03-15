#include "runner_keyboard.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static bool isValidKey(int32_t key) {
    return key >= 0 && GML_KEY_COUNT > key;
}

RunnerKeyboardState* RunnerKeyboard_create(void) {
    RunnerKeyboardState* kb = safeCalloc(1, sizeof(RunnerKeyboardState));
    kb->lastKey = VK_NOKEY;
    return kb;
}

void RunnerKeyboard_free(RunnerKeyboardState* kb) {
    free(kb);
}

void RunnerKeyboard_beginFrame(RunnerKeyboardState* kb) {
    memset(kb->keyPressed, 0, sizeof(kb->keyPressed));
    memset(kb->keyReleased, 0, sizeof(kb->keyReleased));

#ifndef __STDC_NO_ATOMICS__
    uint32_t head = atomic_load_explicit(&kb->eventHead, memory_order_relaxed);
    uint32_t tail = atomic_load_explicit(&kb->eventTail, memory_order_acquire);
#else
    uint32_t head = kb->eventHead;
    uint32_t tail = kb->eventTail;
#endif

    while (head != tail) {
        int32_t key = kb->eventQueue[head].keyCode;
        bool down = kb->eventQueue[head].down;

        if (down) {
            kb->keyDown[key] = true;
            kb->keyPressed[key] = true;
            kb->lastKey = key;
        } else {
            kb->keyDown[key] = false;
            kb->keyReleased[key] = true;
        }

        head = (head + 1) % 128;
    }

#ifndef __STDC_NO_ATOMICS__
    atomic_store_explicit(&kb->eventHead, head, memory_order_release);
#else
    kb->eventHead = head;
#endif
}

void RunnerKeyboard_onKeyDown(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (!isValidKey(gmlKeyCode)) return;

#ifndef __STDC_NO_ATOMICS__
    uint32_t tail = atomic_load_explicit(&kb->eventTail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&kb->eventHead, memory_order_acquire);
#else
    uint32_t tail = kb->eventTail;
    uint32_t head = kb->eventHead;
#endif

    uint32_t nextTail = (tail + 1) % 128;
    if (nextTail != head) {
        kb->eventQueue[tail].keyCode = gmlKeyCode;
        kb->eventQueue[tail].down = true;

#ifndef __STDC_NO_ATOMICS__
        atomic_store_explicit(&kb->eventTail, nextTail, memory_order_release);
#else
        kb->eventTail = nextTail;
#endif
    }
}

void RunnerKeyboard_onKeyUp(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (!isValidKey(gmlKeyCode)) return;

#ifndef __STDC_NO_ATOMICS__
    uint32_t tail = atomic_load_explicit(&kb->eventTail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&kb->eventHead, memory_order_acquire);
#else
    uint32_t tail = kb->eventTail;
    uint32_t head = kb->eventHead;
#endif

    uint32_t nextTail = (tail + 1) % 128;
    if (nextTail != head) {
        kb->eventQueue[tail].keyCode = gmlKeyCode;
        kb->eventQueue[tail].down = false;

#ifndef __STDC_NO_ATOMICS__
        atomic_store_explicit(&kb->eventTail, nextTail, memory_order_release);
#else
        kb->eventTail = nextTail;
#endif
    }
}

bool RunnerKeyboard_check(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (gmlKeyCode == VK_ANYKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyDown[i]) return true;
        }
        return false;
    }
    if (gmlKeyCode == VK_NOKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyDown[i]) return false;
        }
        return true;
    }
    if (!isValidKey(gmlKeyCode)) return false;
    return kb->keyDown[gmlKeyCode];
}

bool RunnerKeyboard_checkPressed(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (gmlKeyCode == VK_ANYKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyPressed[i]) return true;
        }
        return false;
    }
    if (gmlKeyCode == VK_NOKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyPressed[i]) return false;
        }
        return true;
    }
    if (!isValidKey(gmlKeyCode)) return false;
    return kb->keyPressed[gmlKeyCode];
}

bool RunnerKeyboard_checkReleased(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (gmlKeyCode == VK_ANYKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyReleased[i]) return true;
        }
        return false;
    }
    if (gmlKeyCode == VK_NOKEY) {
        for (int32_t i = 2; GML_KEY_COUNT > i; i++) {
            if (kb->keyReleased[i]) return false;
        }
        return true;
    }
    if (!isValidKey(gmlKeyCode)) return false;
    return kb->keyReleased[gmlKeyCode];
}

void RunnerKeyboard_simulatePress(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (!isValidKey(gmlKeyCode)) return;
    kb->keyDown[gmlKeyCode] = true;
    kb->keyPressed[gmlKeyCode] = true;
    kb->lastKey = gmlKeyCode;
}

void RunnerKeyboard_simulateRelease(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (!isValidKey(gmlKeyCode)) return;
    kb->keyDown[gmlKeyCode] = false;
    kb->keyReleased[gmlKeyCode] = true;
}

void RunnerKeyboard_clear(RunnerKeyboardState* kb, int32_t gmlKeyCode) {
    if (gmlKeyCode == VK_ANYKEY) {
        memset(kb->keyDown, 0, sizeof(kb->keyDown));
        memset(kb->keyPressed, 0, sizeof(kb->keyPressed));
        memset(kb->keyReleased, 0, sizeof(kb->keyReleased));
        kb->lastKey = VK_NOKEY;
#ifndef __STDC_NO_ATOMICS__
        atomic_store_explicit(&kb->eventHead, atomic_load_explicit(&kb->eventTail, memory_order_relaxed), memory_order_release);
#else
        kb->eventHead = kb->eventTail;
#endif
        return;
    }
    if (!isValidKey(gmlKeyCode)) return;
    kb->keyDown[gmlKeyCode] = false;
    kb->keyPressed[gmlKeyCode] = false;
    kb->keyReleased[gmlKeyCode] = false;
}

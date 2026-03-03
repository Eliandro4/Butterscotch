#pragma once

#include "data_win.h"
#include "instance.h"
#include "vm.h"

typedef struct {
    DataWin* dataWin;
    VMContext* vmContext;
    Room* currentRoom;
    Instance* instances;
    int frameCount;
} Runner;

Runner* Runner_create(DataWin* dataWin, VMContext* vm);
void Runner_free(Runner* runner);
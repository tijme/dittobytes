#ifndef BEACON_H
#define BEACON_H

#include <windows.h>

typedef struct _CTX CTX, *PCTX;

#define BEACON_MESSAGE "Hello from the beacon!"

UINT32 EntryFunction();

VOID function_one(PCTX context);
VOID function_two(PCTX context);

#endif // BEACON_H
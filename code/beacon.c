#include "inc/beacon.h"
#include "inc/context/context.h"

UINT32 EntryFunction() {
    CTX context = {0};

    if(!init_context(&context)) {
        return context.last_error;
    }

    function_one(&context);
    function_two(&context);
    
    return 0;
}

VOID function_one(PCTX context) {
    context->functions.MessageBoxA(NULL, BEACON_MESSAGE, "Beacon", MB_OK);
}

VOID function_two(PCTX context) {
    context->functions.MessageBoxA(NULL, BEACON_MESSAGE, "Beacon", MB_OK);
}
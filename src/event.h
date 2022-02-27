#include <stdint.h>

#include <android/input.h>

typedef struct InputEvent
{
    int32_t type;
    bool* isHandled;
    union
    {
        struct 
        {
            int action;
            int keyCode;
            int scanCode;
            int unicodeChar;
            int metaState;
        } keyEvent;

        struct 
        {
            int action;
            int pointerCount;
            int x[10];
            int y[10];
        } motionEvent;
    };
} InputEvent;

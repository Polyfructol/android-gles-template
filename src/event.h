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
            int x;
            int y;
        } motionEvent;
    };
} InputEvent;

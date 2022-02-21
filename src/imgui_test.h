#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ANativeWindow ANativeWindow;
typedef struct AInputEvent AInputEvent;

typedef struct ImGuiTest ImGuiTest;

typedef struct ImGuiTestIO
{
    // outputs
    bool showKeyboard;
    bool hideKeyboard;
} ImGuiTestIO;

ImGuiTest* test_Init();
ImGuiTestIO* test_GetIO(ImGuiTest* self);
void test_Load(ImGuiTest* self, ANativeWindow* window);
void test_Unload(ImGuiTest* self);
void test_HandleEvent(ImGuiTest*, const AInputEvent* event);
void test_InputUnicodeChar(ImGuiTest* self, int unicodeChar);
void test_UpdateAndDraw(ImGuiTest* self);
void test_Shutdown(ImGuiTest* self);

#ifdef __cplusplus
}
#endif
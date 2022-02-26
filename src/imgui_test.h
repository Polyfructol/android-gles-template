#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct InputEvent InputEvent;

typedef struct ImGuiTest ImGuiTest;

typedef struct ImGuiTestIO
{
    // outputs
    bool showKeyboard;
    bool hideKeyboard;
} ImGuiTestIO;

ImGuiTest* test_Init();
void test_Terminate(ImGuiTest* self);
ImGuiTestIO* test_GetIO(ImGuiTest* self);
void test_LoadGPUData(ImGuiTest* self);
void test_UnloadGPUData(ImGuiTest* self);
void test_SizeChanged(float width, float height);
void test_HandleEvent(ImGuiTest*, const InputEvent* event);
void test_InputUnicodeChar(ImGuiTest* self, int unicodeChar);
void test_UpdateAndDraw(ImGuiTest* self);

#ifdef __cplusplus
}
#endif
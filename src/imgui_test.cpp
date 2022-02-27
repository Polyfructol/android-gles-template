
#include <glad/gles2.h>

#include <imgui_impl_opengl3.h>
#include <imgui_impl_android.h>

#include "event.h"

#include "imgui_test.h"

struct ImGuiTest
{
    bool showDemoWindow = false;
    bool prevWantTextInput = false;

    ImGuiTestIO io;
    ImGuiTestIO prevIO;

    InputEvent lastMotionEvent;
};

ImGuiTest* test_Init()
{
    ImGuiTest* self = new ImGuiTest();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    ImFontConfig font_cfg;
    font_cfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&font_cfg);
    
    ImGui::GetStyle().ScaleAllSizes(3.0f);

    ImGui_ImplAndroid_Init();
    return self;
}

void test_Terminate(ImGuiTest* self)
{
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    delete self;
}

ImGuiTestIO* test_GetIO(ImGuiTest* self)
{
    return &self->io;
}

void test_LoadGPUData(ImGuiTest* self)
{
    ImGui_ImplOpenGL3_Init("#version 300 es");
}

void test_UnloadGPUData(ImGuiTest* self)
{
    ImGui_ImplOpenGL3_Shutdown();
}

void test_SizeChanged(float width, float height)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = { width, height };
}

void test_HandleEvent(ImGuiTest* self, const InputEvent* inputEvent)
{
    ImGui_ImplAndroid_HandleInputEvent(inputEvent);
    if (inputEvent->type == AINPUT_EVENT_TYPE_MOTION)
        self->lastMotionEvent = *inputEvent;
}

void test_InputUnicodeChar(ImGuiTest* self, int unicodeChar)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(unicodeChar);
}

void test_UpdateAndDraw(ImGuiTest* self)
{
    ImGuiIO& io = ImGui::GetIO();

    // Show/hide keyboard command
    {
        self->io.showKeyboard = ( io.WantTextInput && !self->prevWantTextInput);
        self->io.hideKeyboard = (!io.WantTextInput &&  self->prevWantTextInput);
        self->prevWantTextInput = io.WantTextInput;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    if (self->showDemoWindow)
        ImGui::ShowDemoWindow(&self->showDemoWindow);

    ImGui::Begin("Another Window");
    ImGui::Text("Hello from another window!");
    ImGui::Checkbox("Show demo window", &self->showDemoWindow);
    ImGui::Checkbox("Test motion", &self->io.disableVSYNCOnMotion);
    ImGui::End();

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    for (int i = 0; i < self->lastMotionEvent.motionEvent.pointerCount; ++i)
    {
        float x = self->lastMotionEvent.motionEvent.x[i];
        float y = self->lastMotionEvent.motionEvent.y[i];
        drawList->AddCircleFilled({ x, y }, 20.f, IM_COL32_WHITE);
        drawList->AddCircle({ x, y }, 90.f, IM_COL32_WHITE);
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    self->prevIO = self->io;
}

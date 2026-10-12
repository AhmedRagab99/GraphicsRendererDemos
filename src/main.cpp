#include "smoke_tests.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <backends/imgui_impl_metal.h>
#include <backends/imgui_impl_sdl3.h>

#include <cstdio>
#include <cstring>

namespace {

// Draws a triangle rotated with glm into the ImGui background draw list.
void drawSpinningTriangle(float time, ImVec2 center, float radius, ImU32 color) {
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 0.0f, 1.0f));
    ImVec2 points[3];
    for (int i = 0; i < 3; ++i) {
        float angle = glm::two_pi<float>() * static_cast<float>(i) / 3.0f;
        glm::vec4 p = rotation * glm::vec4(glm::cos(angle), glm::sin(angle), 0.0f, 1.0f);
        points[i] = ImVec2(center.x + p.x * radius, center.y - p.y * radius);
    }
    ImGui::GetBackgroundDrawList()->AddTriangleFilled(points[0], points[1], points[2], color);
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0) return runSmokeTests();
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Graphics Renderer Demos", 1280, 720,
                                          SDL_WINDOW_METAL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    MTL::CommandQueue* queue = device->newCommandQueue();

    SDL_MetalView view = SDL_Metal_CreateView(window);
    auto* layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(view));
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForMetal(window);
    ImGui_ImplMetal_Init(device);

    float clearColor[3] = {0.08f, 0.09f, 0.12f};
    bool showDemo = false;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        layer->setDrawableSize(CGSizeMake(width, height));

        CA::MetalDrawable* drawable = layer->nextDrawable();
        if (drawable) {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::renderPassDescriptor();
            MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
            color->setTexture(drawable->texture());
            color->setLoadAction(MTL::LoadActionClear);
            color->setStoreAction(MTL::StoreActionStore);
            color->setClearColor(MTL::ClearColor::Make(clearColor[0], clearColor[1], clearColor[2], 1.0));

            ImGui_ImplMetal_NewFrame(pass);
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;
            ImGuiIO& io = ImGui::GetIO();
            drawSpinningTriangle(time, ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), 120.0f,
                                 IM_COL32(240, 160, 60, 255));

            ImGui::Begin("Graphics Renderer Demos");
            ImGui::Text("%.1f FPS (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
            ImGui::Text("GPU: %s", device->name()->utf8String());
            ImGui::ColorEdit3("Clear color", clearColor);
            ImGui::Checkbox("ImGui demo window", &showDemo);
            ImGui::End();
            if (showDemo) ImGui::ShowDemoWindow(&showDemo);

            ImGui::Render();

            MTL::CommandBuffer* commands = queue->commandBuffer();
            MTL::RenderCommandEncoder* encoder = commands->renderCommandEncoder(pass);
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commands, encoder);
            encoder->endEncoding();
            commands->presentDrawable(drawable);
            commands->commit();
        }

        pool->release();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_Metal_DestroyView(view);
    queue->release();
    device->release();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

#include "renderer.h"
#include "smoke_tests.h"

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

const char* apiLabel(GraphicsApi api) {
    return api == GraphicsApi::Metal ? "Metal (MSL)" : "Vulkan (GLSL -> SPIR-V)";
}

const char* shaderDescription(GraphicsApi api, ShaderSource source) {
    if (source == ShaderSource::Slang)
        return api == GraphicsApi::Metal ? "triangle.slang -> MSL -> .metallib" : "triangle.slang -> SPIR-V";
    return api == GraphicsApi::Metal ? "triangle.metal -> .metallib" : "triangle.vert/.frag -> SPIR-V (glslc)";
}

// A window can only be used with one graphics API (SDL_WINDOW_METAL or SDL_WINDOW_VULKAN), so switching API
// means creating a new window with the right flag, keeping the old position and size.
SDL_Window* createWindow(GraphicsApi api, const SDL_Rect* previous) {
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    flags |= api == GraphicsApi::Metal ? SDL_WINDOW_METAL : SDL_WINDOW_VULKAN;

    SDL_Window* window = SDL_CreateWindow("Graphics Renderer Demos", previous ? previous->w : 1280,
                                          previous ? previous->h : 720, flags);
    if (window && previous) SDL_SetWindowPosition(window, previous->x, previous->y);
    return window;
}

bool initPlatformBackend(SDL_Window* window, GraphicsApi api) {
    return api == GraphicsApi::Metal ? ImGui_ImplSDL3_InitForMetal(window) : ImGui_ImplSDL3_InitForVulkan(window);
}

std::unique_ptr<Renderer> createRenderer(GraphicsApi api) {
    return api == GraphicsApi::Metal ? createMetalRenderer() : createVulkanRenderer();
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0) return runSmokeTests();
    }

    GraphicsApi api = GraphicsApi::Metal;
    ShaderSource shaderSource = ShaderSource::Native;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--vulkan") == 0) api = GraphicsApi::Vulkan;
        if (std::strcmp(argv[i], "--slang") == 0) shaderSource = ShaderSource::Slang;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    SDL_Window* window = nullptr;
    std::unique_ptr<Renderer> renderer;
    std::string statusMessage;

    // Creates the window, the SDL3 platform backend and the renderer for `target`. Everything is torn down again on failure.
    auto start = [&](GraphicsApi target, const SDL_Rect* previous) {
        window = createWindow(target, previous);
        if (!window) {
            statusMessage = std::string("Cannot create window: ") + SDL_GetError();
            return false;
        }
        initPlatformBackend(window, target);
        renderer = createRenderer(target);
        if (!renderer->init(window)) {
            statusMessage = std::string("Failed to start ") + apiLabel(target) + " (see console)";
            renderer->shutdown();
            renderer.reset();
            ImGui_ImplSDL3_Shutdown();
            SDL_DestroyWindow(window);
            window = nullptr;
            return false;
        }
        if (shaderSource != ShaderSource::Native && !renderer->setShaderSource(shaderSource)) {
            statusMessage = "Slang shaders could not be loaded, using the native ones (see console)";
            shaderSource = ShaderSource::Native;
        }
        return true;
    };

    if (!start(api, nullptr)) {
        std::fprintf(stderr, "%s\n", statusMessage.c_str());
        ImGui::DestroyContext();
        SDL_Quit();
        return 1;
    }

    SceneParams scene;
    float spinSpeed = 1.0f;
    float angle = 0.0f;
    float scale = 1.0f;
    Uint64 lastTicks = SDL_GetTicks();
    bool showDemo = false;
    bool running = true;
    bool switchRequested = false;
    GraphicsApi requestedApi = api;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        if (switchRequested) {
            switchRequested = false;
            SDL_Rect rect;
            SDL_GetWindowPosition(window, &rect.x, &rect.y);
            SDL_GetWindowSize(window, &rect.w, &rect.h);

            renderer->shutdown();
            renderer.reset();
            ImGui_ImplSDL3_Shutdown();
            SDL_DestroyWindow(window);
            window = nullptr;

            if (start(requestedApi, &rect)) {
                api = requestedApi;
                statusMessage.clear();
            } else {
                // Go back to the API that worked before; keep the error message for the UI.
                std::string error = statusMessage;
                requestedApi = api;
                if (!start(api, &rect)) {
                    std::fprintf(stderr, "Cannot restore previous graphics API: %s\n", statusMessage.c_str());
                    break;
                }
                statusMessage = error;
            }
            continue;
        }

        if (!renderer->beginFrame()) {
            SDL_Delay(10);
            continue;
        }
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Uint64 now = SDL_GetTicks();
        float deltaSeconds = static_cast<float>(now - lastTicks) / 1000.0f;
        lastTicks = now;
        angle += spinSpeed * deltaSeconds;

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
        scene.transform = glm::scale(glm::mat4(1.0f), glm::vec3(scale / aspect, scale, 1.0f)) *
                          glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(16, 16), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Graphics Renderer Demos");

        ImGui::SeparatorText("Graphics API");
        int selected = requestedApi == GraphicsApi::Metal ? 0 : 1;
        bool changed = ImGui::RadioButton("Metal (MSL)", &selected, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Vulkan (GLSL -> SPIR-V)", &selected, 1);
        if (changed) {
            requestedApi = selected == 0 ? GraphicsApi::Metal : GraphicsApi::Vulkan;
            switchRequested = requestedApi != api;
        }
        ImGui::Text("Running: %s", apiLabel(api));

        ImGui::SeparatorText("Shader source");
        int source = shaderSource == ShaderSource::Native ? 0 : 1;
        bool sourceChanged = ImGui::RadioButton("Native (MSL / GLSL)", &source, 0);
        ImGui::SameLine();
        sourceChanged |= ImGui::RadioButton("Slang", &source, 1);
        if (sourceChanged) {
            ShaderSource wanted = source == 0 ? ShaderSource::Native : ShaderSource::Slang;
            if (renderer->setShaderSource(wanted)) {
                shaderSource = wanted;
                statusMessage.clear();
            } else {
                statusMessage = "Could not load those shaders (see console)";
            }
        }
        ImGui::Text("Shaders: %s", shaderDescription(api, shaderSource));
        ImGui::Text("GPU: %s", renderer->deviceName());
        if (!statusMessage.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", statusMessage.c_str());

        ImGui::SeparatorText("Scene");
        ImGui::Text("%.1f FPS (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::SliderFloat("Spin speed", &spinSpeed, -4.0f, 4.0f);
        ImGui::SliderFloat("Scale", &scale, 0.2f, 1.6f);
        ImGui::ColorEdit3("Clear color", scene.clearColor);
        ImGui::Checkbox("ImGui demo window", &showDemo);
        ImGui::End();
        if (showDemo) ImGui::ShowDemoWindow(&showDemo);

        ImGui::Render();
        renderer->endFrame(scene, ImGui::GetDrawData());
    }

    if (renderer) renderer->shutdown();
    renderer.reset();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

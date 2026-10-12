#pragma once

#include <glm/glm.hpp>

#include <memory>

struct ImDrawData;
struct SDL_Window;

enum class GraphicsApi { Metal, Vulkan };

// Where the shaders come from. Native = hand-written MSL (Metal) / GLSL (Vulkan). Slang = one triangle.slang
// compiled to MSL and SPIR-V by slangc.
enum class ShaderSource { Native, Slang };

// Per-frame inputs for the scene (the shader-drawn triangle) that every backend renders the same way.
struct SceneParams {
    glm::mat4 transform{1.0f};
    float clearColor[3] = {0.08f, 0.09f, 0.12f};
};

// Minimal backend interface. This is the first sketch of the RHI described in plan.md.
// A frame looks like:
//   if (!renderer->beginFrame()) skip;   // acquires the next image, starts the ImGui renderer backend frame
//   ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame(); ...build UI...; ImGui::Render();
//   renderer->endFrame(scene, ImGui::GetDrawData());
class Renderer {
public:
    virtual ~Renderer() = default;

    // The window must have been created with the flag that matches the backend (SDL_WINDOW_METAL / SDL_WINDOW_VULKAN).
    virtual bool init(SDL_Window* window) = 0;
    // Also shuts down the ImGui renderer backend. The caller shuts down the SDL3 platform backend.
    virtual void shutdown() = 0;

    // Rebuilds the pipeline from the other shader set. Keeps the current pipeline if it fails.
    virtual bool setShaderSource(ShaderSource source) = 0;

    virtual bool beginFrame() = 0;
    virtual void endFrame(const SceneParams& scene, ImDrawData* drawData) = 0;

    virtual GraphicsApi api() const = 0;
    virtual const char* deviceName() const = 0;
};

std::unique_ptr<Renderer> createMetalRenderer();
std::unique_ptr<Renderer> createVulkanRenderer();

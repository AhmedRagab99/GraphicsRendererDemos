#define MINIAUDIO_IMPLEMENTATION

#include "smoke_tests.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include <assimp/Importer.hpp>
#include <assimp/version.h>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <miniaudio.h>

#include <cstdio>
#include <thread>

namespace {

int failures = 0;

void report(const char* name, bool ok, const char* detail = "") {
    std::printf("[%s] %s %s\n", ok ? " OK " : "FAIL", name, detail);
    if (!ok) ++failures;
}

void testMetal() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    report("metal-cpp", device != nullptr, device ? device->name()->utf8String() : "no device");
    if (device) device->release();
    pool->release();
}

void testSDL() {
    bool ok = SDL_Init(SDL_INIT_VIDEO);
    report("SDL3 init", ok, ok ? SDL_GetCurrentVideoDriver() : SDL_GetError());
    if (!ok) return;

    SDL_Window* window = SDL_CreateWindow("Library smoke test", 640, 360, SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN);
    report("SDL3 window", window != nullptr, window ? "" : SDL_GetError());
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void testGLM() {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
    glm::vec4 p = m * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    report("glm", p == glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
}

void testVulkanHeaders() {
    char detail[64];
    std::snprintf(detail, sizeof(detail), "headers %u.%u.%u", VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE),
                  VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE), VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE));
    report("Vulkan-Headers", VK_HEADER_VERSION > 0, detail);
}

void testImGui() {
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(640, 360);
    // No renderer backend here, so build the font atlas by hand.
    unsigned char* pixels;
    int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);

    // Windows are hidden for their first (auto-sizing) frame, so run two frames.
    for (int i = 0; i < 2; ++i) {
        ImGui::NewFrame();
        ImGui::Begin("Smoke test");
        ImGui::Text("hello");
        ImGui::End();
        ImGui::Render();
    }
    ImDrawData* draw = ImGui::GetDrawData();
    report("imgui", ctx != nullptr && draw && draw->CmdListsCount > 0, ImGui::GetVersion());
    ImGui::DestroyContext(ctx);
}

void testAssimp() {
    Assimp::Importer importer;
    char detail[64];
    std::snprintf(detail, sizeof(detail), "v%u.%u.%u", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch());
    report("assimp", importer.GetImporterCount() > 0, detail);
}

void testJolt() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    constexpr JPH::ObjectLayer kLayer = 0;

    struct ObjectToBroad final : JPH::BroadPhaseLayerInterface {
        JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override { return JPH::BroadPhaseLayer(0); }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "DEFAULT"; }
#endif
    } broadInterface;
    struct ObjectVsBroad final : JPH::ObjectVsBroadPhaseLayerFilter {
        bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
    } objectVsBroad;
    struct ObjectPair final : JPH::ObjectLayerPairFilter {
        bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
    } objectPair;

    JPH::PhysicsSystem physics;
    physics.Init(64, 0, 64, 64, broadInterface, objectVsBroad, objectPair);
    physics.SetGravity(JPH::Vec3(0, -10, 0));

    JPH::BodyInterface& bodies = physics.GetBodyInterface();
    JPH::BodyCreationSettings settings(new JPH::SphereShape(0.5f), JPH::RVec3(0, 10, 0), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Dynamic, kLayer);
    JPH::BodyID id = bodies.CreateAndAddBody(settings, JPH::EActivation::Activate);

    JPH::TempAllocatorImpl allocator(10 * 1024 * 1024);
    JPH::JobSystemThreadPool jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                  std::max(1u, std::thread::hardware_concurrency() - 1));
    for (int i = 0; i < 60; ++i) physics.Update(1.0f / 60.0f, 1, &allocator, &jobs);

    float y = bodies.GetCenterOfMassPosition(id).GetY();
    char detail[64];
    std::snprintf(detail, sizeof(detail), "sphere fell from y=10 to y=%.2f", y);
    report("Jolt Physics", y < 10.0f, detail);

    bodies.RemoveBody(id);
    bodies.DestroyBody(id);
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void testMiniaudio() {
    // Don't open a real audio device, so this also passes on machines without one.
    ma_engine_config config = ma_engine_config_init();
    config.noDevice = MA_TRUE;
    config.channels = 2;
    config.sampleRate = 48000;
    ma_engine engine;
    ma_result result = ma_engine_init(&config, &engine);
    report("miniaudio", result == MA_SUCCESS, ma_result_description(result));
    if (result == MA_SUCCESS) ma_engine_uninit(&engine);
}

} // namespace

int runSmokeTests() {
    testMetal();
    testSDL();
    testGLM();
    testVulkanHeaders();
    testImGui();
    testAssimp();
    testJolt();
    testMiniaudio();

    std::printf("\n%s\n", failures == 0 ? "All libraries working." : "Some libraries failed.");
    return failures == 0 ? 0 : 1;
}

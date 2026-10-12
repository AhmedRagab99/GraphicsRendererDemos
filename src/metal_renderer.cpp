#include "renderer.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_metal.h>

#include <cstdio>
#include <string>

namespace {

class MetalRenderer final : public Renderer {
public:
    bool init(SDL_Window* sdlWindow) override {
        window = sdlWindow;
        device = MTL::CreateSystemDefaultDevice();
        if (!device) {
            std::fprintf(stderr, "Metal: no device\n");
            return false;
        }
        queue = device->newCommandQueue();

        view = SDL_Metal_CreateView(window);
        layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(view));
        layer->setDevice(device);
        layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

        if (!createPipeline(ShaderSource::Native)) return false;
        return ImGui_ImplMetal_Init(device);
    }

    void shutdown() override {
        if (device) ImGui_ImplMetal_Shutdown();
        if (pipeline) pipeline->release();
        if (queue) queue->release();
        if (view) SDL_Metal_DestroyView(view);
        if (device) device->release();
        pipeline = nullptr;
        queue = nullptr;
        view = nullptr;
        device = nullptr;
    }

    bool setShaderSource(ShaderSource source) override { return createPipeline(source); }

    bool beginFrame() override {
        pool = NS::AutoreleasePool::alloc()->init();

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        layer->setDrawableSize(CGSizeMake(width, height));

        drawable = width > 0 && height > 0 ? layer->nextDrawable() : nullptr;
        if (!drawable) {
            pool->release();
            pool = nullptr;
            return false;
        }

        pass = MTL::RenderPassDescriptor::renderPassDescriptor();
        MTL::RenderPassColorAttachmentDescriptor* color = pass->colorAttachments()->object(0);
        color->setTexture(drawable->texture());
        color->setLoadAction(MTL::LoadActionClear);
        color->setStoreAction(MTL::StoreActionStore);

        ImGui_ImplMetal_NewFrame(pass);
        return true;
    }

    void endFrame(const SceneParams& scene, ImDrawData* drawData) override {
        pass->colorAttachments()->object(0)->setClearColor(
            MTL::ClearColor::Make(scene.clearColor[0], scene.clearColor[1], scene.clearColor[2], 1.0));

        MTL::CommandBuffer* commands = queue->commandBuffer();
        MTL::RenderCommandEncoder* encoder = commands->renderCommandEncoder(pass);

        encoder->setRenderPipelineState(pipeline);
        encoder->setVertexBytes(&scene.transform, sizeof(scene.transform), 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

        ImGui_ImplMetal_RenderDrawData(drawData, commands, encoder);
        encoder->endEncoding();
        commands->presentDrawable(drawable);
        commands->commit();

        pool->release();
        pool = nullptr;
    }

    GraphicsApi api() const override { return GraphicsApi::Metal; }
    const char* deviceName() const override { return device ? device->name()->utf8String() : "none"; }

private:
    // Native: src/shaders/triangle.metal. Slang: src/shaders/triangle.slang, translated to MSL by slangc.
    // Either way it is compiled to a .metallib at build time.
    bool createPipeline(ShaderSource source) {
        const bool slang = source == ShaderSource::Slang;
        const char* base = SDL_GetBasePath();
        std::string path = std::string(base ? base : "") + "shaders/" + (slang ? "triangle_slang.metallib" : "triangle.metallib");

        NS::Error* error = nullptr;
        MTL::Library* library = device->newLibrary(NS::String::string(path.c_str(), NS::UTF8StringEncoding), &error);
        if (!library) {
            std::fprintf(stderr, "Metal: cannot load %s: %s\n", path.c_str(),
                         error ? error->localizedDescription()->utf8String() : "unknown error");
            return false;
        }

        MTL::Function* vertex = library->newFunction(
            NS::String::string(slang ? "vertexMain" : "triangle_vertex", NS::UTF8StringEncoding));
        MTL::Function* fragment = library->newFunction(
            NS::String::string(slang ? "fragmentMain" : "triangle_fragment", NS::UTF8StringEncoding));

        MTL::RenderPipelineState* created = nullptr;
        if (vertex && fragment) {
            MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
            descriptor->setVertexFunction(vertex);
            descriptor->setFragmentFunction(fragment);
            descriptor->colorAttachments()->object(0)->setPixelFormat(layer->pixelFormat());
            created = device->newRenderPipelineState(descriptor, &error);
            if (!created) {
                std::fprintf(stderr, "Metal: pipeline creation failed: %s\n",
                             error ? error->localizedDescription()->utf8String() : "unknown error");
            }
            descriptor->release();
        } else {
            std::fprintf(stderr, "Metal: entry points not found in %s\n", path.c_str());
        }

        if (vertex) vertex->release();
        if (fragment) fragment->release();
        library->release();

        if (!created) return false;
        // Command buffers already submitted keep their own reference to the old pipeline.
        if (pipeline) pipeline->release();
        pipeline = created;
        return true;
    }

    SDL_Window* window = nullptr;
    SDL_MetalView view = nullptr;
    CA::MetalLayer* layer = nullptr;
    MTL::Device* device = nullptr;
    MTL::CommandQueue* queue = nullptr;
    MTL::RenderPipelineState* pipeline = nullptr;

    // Valid between beginFrame() and endFrame().
    NS::AutoreleasePool* pool = nullptr;
    CA::MetalDrawable* drawable = nullptr;
    MTL::RenderPassDescriptor* pass = nullptr;
};

} // namespace

std::unique_ptr<Renderer> createMetalRenderer() { return std::make_unique<MetalRenderer>(); }

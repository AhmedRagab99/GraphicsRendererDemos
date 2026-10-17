#include "renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <vulkan/vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kFramesInFlight = 2;

bool check(VkResult result, const char* what) {
    if (result == VK_SUCCESS) return true;
    std::fprintf(stderr, "Vulkan: %s failed (VkResult %d)\n", what, static_cast<int>(result));
    return false;
}

void checkImGui(VkResult result) {
    if (result != VK_SUCCESS) std::fprintf(stderr, "Vulkan (ImGui): VkResult %d\n", static_cast<int>(result));
}

bool hasExtension(const std::vector<VkExtensionProperties>& available, const char* name) {
    return std::any_of(available.begin(), available.end(),
                       [name](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
}

bool readFile(const std::string& path, std::vector<char>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    out.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(out.data(), static_cast<std::streamsize>(out.size()));
    return true;
}

class VulkanRenderer final : public Renderer {
public:
    bool init(SDL_Window* sdlWindow) override {
        window = sdlWindow;
        return createInstance() && createSurface() && pickDevice() && createDevice() && createFrames() &&
               createSwapchain() && createPipelineLayout() &&
               createPipeline(ShaderSource::Native, &pipeline) && initImGui();
    }

    void shutdown() override {
        if (device) {
            vkDeviceWaitIdle(device);
            if (imguiReady) ImGui_ImplVulkan_Shutdown();
            imguiReady = false;

            destroySwapchain();
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            for (Frame& f : frames) {
                if (f.fence) vkDestroyFence(device, f.fence, nullptr);
                if (f.imageAvailable) vkDestroySemaphore(device, f.imageAvailable, nullptr);
                if (f.pool) vkDestroyCommandPool(device, f.pool, nullptr);
                f = {};
            }
            vkDestroyDevice(device, nullptr);
        }
        if (surface) SDL_Vulkan_DestroySurface(instance, surface, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        pipeline = VK_NULL_HANDLE;
        pipelineLayout = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        surface = VK_NULL_HANDLE;
        instance = VK_NULL_HANDLE;
    }

    bool setShaderSource(ShaderSource source) override {
        VkPipeline created = VK_NULL_HANDLE;
        if (!createPipeline(source, &created)) return false;
        vkDeviceWaitIdle(device); // the old pipeline may still be used by frames in flight
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = created;
        return true;
    }

    bool beginFrame() override {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width <= 0 || height <= 0) return false;
        if (!swapchain || static_cast<uint32_t>(width) != extent.width || static_cast<uint32_t>(height) != extent.height ||
            needsRecreate) {
            if (!recreateSwapchain() || !swapchain) return false;
        }

        Frame& frame = frames[frameIndex];
        vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);

        VkResult result =
            vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            needsRecreate = true;
            return false;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            check(result, "vkAcquireNextImageKHR");
            return false;
        }
        // Only reset once we know work will be submitted, otherwise the next wait would never return.
        vkResetFences(device, 1, &frame.fence);

        ImGui_ImplVulkan_NewFrame();
        return true;
    }

    void endFrame(const SceneParams& scene, ImDrawData* drawData) override {
        Frame& frame = frames[frameIndex];
        VkCommandBuffer cmd = frame.commandBuffer;

        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        transitionImage(cmd, images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView = views[imageIndex];
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{scene.clearColor[0], scene.clearColor[1], scene.clearColor[2], 1.0f}};

        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea = {{0, 0}, extent};
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color;
        vkCmdBeginRendering(cmd, &rendering);

        // A negative viewport height flips Y so clip space matches Metal (+Y up) and the same transform works.
        VkViewport viewport{0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width),
                            -static_cast<float>(extent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(scene.transform), &scene.transform);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        vkCmdEndRendering(cmd);

        transitionImage(cmd, images[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_NONE, 0);
        vkEndCommandBuffer(cmd);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.imageAvailable;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished[imageIndex];
        check(vkQueueSubmit(queue, 1, &submit, frame.fence), "vkQueueSubmit");

        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished[imageIndex];
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &imageIndex;
        VkResult result = vkQueuePresentKHR(queue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) needsRecreate = true;

        frameIndex = (frameIndex + 1) % kFramesInFlight;
    }

    GraphicsApi api() const override { return GraphicsApi::Vulkan; }
    const char* deviceName() const override { return deviceNameStorage.c_str(); }

private:
    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };

    bool createInstance() {
        uint32_t sdlCount = 0;
        const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlCount);
        if (!sdlExtensions) {
            std::fprintf(stderr, "Vulkan: SDL_Vulkan_GetInstanceExtensions: %s\n", SDL_GetError());
            return false;
        }
        std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlCount);

        uint32_t availableCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);
        std::vector<VkExtensionProperties> available(availableCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());

        // MoltenVK is a "portability" driver and is hidden unless the loader is told to enumerate it.
        VkInstanceCreateFlags flags = 0;
        if (hasExtension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }

        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "GraphicsRendererDemos";
        app.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.flags = flags;
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        return check(vkCreateInstance(&info, nullptr, &instance), "vkCreateInstance");
    }

    bool createSurface() {
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
            std::fprintf(stderr, "Vulkan: SDL_Vulkan_CreateSurface: %s\n", SDL_GetError());
            return false;
        }
        return true;
    }

    bool pickDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        for (VkPhysicalDevice candidate : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(candidate, &props);
            if (props.apiVersion < VK_API_VERSION_1_3) continue;

            uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

            for (uint32_t i = 0; i < familyCount; ++i) {
                VkBool32 canPresent = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &canPresent);
                if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && canPresent) {
                    physicalDevice = candidate;
                    queueFamily = i;
                    deviceNameStorage = std::string(props.deviceName) + " (Vulkan)";
                    return true;
                }
            }
        }
        std::fprintf(stderr, "Vulkan: no suitable device (needs Vulkan 1.3, graphics + present)\n");
        return false;
    }

    bool createDevice() {
        uint32_t count = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, available.data());

        std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        // Required to be enabled whenever the device offers it (MoltenVK does).
        if (hasExtension(available, "VK_KHR_portability_subset")) extensions.push_back("VK_KHR_portability_subset");

        // Slang's SV_VertexID uses gl_VertexIndex - gl_BaseVertex, which needs shaderDrawParameters.
        VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        features11.shaderDrawParameters = VK_TRUE;

        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.pNext = &features11;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.pNext = &features13;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queueInfo;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        if (!check(vkCreateDevice(physicalDevice, &info, nullptr, &device), "vkCreateDevice")) return false;

        vkGetDeviceQueue(device, queueFamily, 0, &queue);
        return true;
    }

    bool createFrames() {
        for (Frame& f : frames) {
            VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamily;
            if (!check(vkCreateCommandPool(device, &poolInfo, nullptr, &f.pool), "vkCreateCommandPool")) return false;

            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool = f.pool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            if (!check(vkAllocateCommandBuffers(device, &allocInfo, &f.commandBuffer), "vkAllocateCommandBuffers"))
                return false;

            VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (!check(vkCreateSemaphore(device, &semInfo, nullptr, &f.imageAvailable), "vkCreateSemaphore")) return false;

            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (!check(vkCreateFence(device, &fenceInfo, nullptr, &f.fence), "vkCreateFence")) return false;
        }
        return true;
    }

    // Leaves `swapchain` null (and returns true) when the window has no drawable area, e.g. when minimised.
    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR caps;
        if (!check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps), "surface capabilities"))
            return false;

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        extent = caps.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::clamp(static_cast<uint32_t>(std::max(width, 0)), caps.minImageExtent.width,
                                      caps.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(std::max(height, 0)), caps.minImageExtent.height,
                                       caps.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0) return true;

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        // UNORM (not sRGB) matches the Metal layer's BGRA8Unorm so both backends show the same colours.
        VkSurfaceFormatKHR chosen = formats.at(0);
        for (const VkSurfaceFormatKHR& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) chosen = f;
        }
        format = chosen.format;

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface;
        info.minImageCount = imageCount;
        info.imageFormat = chosen.format;
        info.imageColorSpace = chosen.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        if (!check(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "vkCreateSwapchainKHR")) return false;

        uint32_t actualCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &actualCount, nullptr);
        images.resize(actualCount);
        vkGetSwapchainImagesKHR(device, swapchain, &actualCount, images.data());
        swapchainImageCount = actualCount;

        views.resize(actualCount);
        renderFinished.resize(actualCount);
        for (uint32_t i = 0; i < actualCount; ++i) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (!check(vkCreateImageView(device, &viewInfo, nullptr, &views[i]), "vkCreateImageView")) return false;

            VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (!check(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinished[i]), "vkCreateSemaphore"))
                return false;
        }
        needsRecreate = false;
        return true;
    }

    void destroySwapchain() {
        for (VkSemaphore s : renderFinished) vkDestroySemaphore(device, s, nullptr);
        for (VkImageView v : views) vkDestroyImageView(device, v, nullptr);
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        renderFinished.clear();
        views.clear();
        images.clear();
        swapchain = VK_NULL_HANDLE;
    }

    bool recreateSwapchain() {
        vkDeviceWaitIdle(device);
        destroySwapchain();
        bool ok = createSwapchain();
        if (ok && swapchain && imguiReady) ImGui_ImplVulkan_SetMinImageCount(swapchainImageCount);
        return ok;
    }

    VkShaderModule loadShader(const char* file) {
        const char* base = SDL_GetBasePath();
        std::string path = std::string(base ? base : "") + "shaders/" + file;
        std::vector<char> code;
        if (!readFile(path, code)) {
            std::fprintf(stderr, "Vulkan: cannot read %s\n", path.c_str());
            return VK_NULL_HANDLE;
        }
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &info, nullptr, &module), "vkCreateShaderModule");
        return module;
    }

    bool createPipelineLayout() {
        VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        return check(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
    }

    // Native: GLSL (src/shaders/triangle.vert/.frag) compiled to two SPIR-V files with glslc.
    // Slang: src/shaders/triangle.slang compiled by slangc to one SPIR-V file holding both entry points.
    bool createPipeline(ShaderSource source, VkPipeline* out) {
        if (!swapchain) return false;
        const bool slang = source == ShaderSource::Slang;
        VkShaderModule vert = loadShader(slang ? "triangle_slang.spv" : "triangle.vert.spv");
        VkShaderModule frag = slang ? vert : loadShader("triangle.frag.spv");
        if (!vert || !frag) return false;

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert;
        stages[0].pName = slang ? "vertexMain" : "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag;
        stages[1].pName = slang ? "fragmentMain" : "main";

        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                         VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;
        std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();

        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &format;

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.pNext = &rendering;
        info.stageCount = static_cast<uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewportState;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = pipelineLayout;
        bool ok = check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, out), "pipeline");

        vkDestroyShaderModule(device, vert, nullptr);
        if (frag != vert) vkDestroyShaderModule(device, frag, nullptr);
        return ok;
    }

    bool initImGui() {
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion = VK_API_VERSION_1_3;
        info.Instance = instance;
        info.PhysicalDevice = physicalDevice;
        info.Device = device;
        info.QueueFamily = queueFamily;
        info.Queue = queue;
        info.DescriptorPoolSize = 16;
        info.MinImageCount = std::max(2u, swapchainImageCount);
        info.ImageCount = swapchainImageCount;
        info.UseDynamicRendering = true;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format;
        info.CheckVkResultFn = checkImGui;
        imguiReady = ImGui_ImplVulkan_Init(&info);
        return imguiReady;
    }

    static void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                                VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                                VkAccessFlags2 dstAccess) {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = srcStage;
        barrier.srcAccessMask = srcAccess;
        barrier.dstStageMask = dstStage;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = from;
        barrier.newLayout = to;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(cmd, &dependency);
    }

    SDL_Window* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkQueue queue = VK_NULL_HANDLE;
    std::string deviceNameStorage = "none";

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    uint32_t swapchainImageCount = 0;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSemaphore> renderFinished; // one per swapchain image
    bool needsRecreate = false;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    bool imguiReady = false;

    std::array<Frame, kFramesInFlight> frames{};
    uint32_t frameIndex = 0;
    uint32_t imageIndex = 0;
};

} // namespace

std::unique_ptr<Renderer> createVulkanRenderer() { return std::make_unique<VulkanRenderer>(); }

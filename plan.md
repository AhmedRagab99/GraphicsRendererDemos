# Learning Plan: Real-Time Rendering with Metal 4, then Vulkan

The goal is to go from "clear the screen" to the techniques used in modern game engines. I do it first in Metal 4,
then port to Vulkan behind an RHI (render hardware interface), so the renderer code doesn't care which API is
underneath.

**How to use this file:** tick a box (`[x]`) when the item is done _and_ I can explain it without looking. Each
phase ends with a demo and a "can I explain it?" check. Don't skip the checks, because they are the actual learning.

**Rule of thumb:** write each new technique as a self-contained demo first. Only move it into the framework once
I understand it.

## Contents

- [Phase 0: Foundation](#phase-0-foundation)
- [Phase 1: Metal basics](#phase-1-metal-basics)
- [Phase 2: 3D fundamentals](#phase-2-3d-fundamentals)
- [Phase 3: Lighting and materials](#phase-3-lighting-and-materials)
- [Phase 4: Shadows](#phase-4-shadows)
- [Phase 5: Image-based lighting and HDR](#phase-5-image-based-lighting-and-hdr)
- [Phase 6: Frame structure](#phase-6-frame-structure)
- [Phase 7: Metal 4 in depth](#phase-7-metal-4-in-depth)
- [Phase 8: GPU-driven rendering](#phase-8-gpu-driven-rendering)
- [Phase 9: Advanced lighting and global illumination](#phase-9-advanced-lighting-and-global-illumination)
- [Phase 10: Post-processing and upscaling](#phase-10-post-processing-and-upscaling)
- [Phase 11: Animation, particles and world](#phase-11-animation-particles-and-world)
- [Phase 12: Ray tracing](#phase-12-ray-tracing)
- [Phase 13: Engine architecture](#phase-13-engine-architecture)
- [Phase 14: The RHI](#phase-14-the-rhi)
- [Phase 15: Vulkan](#phase-15-vulkan)
- [Phase 16: Capstone](#phase-16-capstone)
- [Reference material](#reference-material)

---

## Phase 0: Foundation

Get the toolchain and math solid before touching the GPU.

**Setup**

- [x] Repository with CMake build
- [x] Third-party libraries as submodules (SDL3, glm, ImGui, Vulkan-Headers, Assimp, Jolt, miniaudio)
- [x] metal-cpp integrated
- [x] Smoke test for every library
- [ ] Window, Metal layer and ImGui render loop
- [ ] Add a `.clang-format` and pick a naming convention
- [ ] Add validation layers / Metal API validation switch (`MTL_DEBUG_LAYER=1`, `MTL_SHADER_VALIDATION=1`)
- [ ] Learn Xcode's GPU frame capture (Metal debugger) on this app

**Math refresher** (needed for everything after)

- [ ] Vectors: dot, cross, normalize, projection, reflection, refraction
- [ ] Matrices: translation, rotation, scale, composition order, column-major vs row-major
- [ ] Coordinate systems: left/right-handed, Metal clip space (z in [0,1]) vs OpenGL ([-1,1]) vs Vulkan (y down)
- [ ] Model, view, projection pipeline and the perspective divide
- [ ] Quaternions: rotation, slerp, conversion to matrix
- [ ] Barycentric coordinates and why triangles interpolate the way they do
- [ ] Floating point: precision problems, reversed-Z and why it helps

**Check:** can I derive a perspective projection matrix on paper?

---

## Phase 1: Metal basics

The smallest working Metal programs. Use Metal 3 style APIs here to learn the concepts, then revisit with Metal 4 in
[Phase 7](#phase-7-metal-4-in-depth).

- [ ] Device, command queue, command buffer, command encoder: what each is and who owns what
- [ ] Render pass descriptor: load/store actions, clear colour
- [ ] Metal Shading Language (MSL) basics: types, address spaces, attributes
- [ ] Compile `.metal` files offline into a `.metallib` with CMake
- [ ] Render pipeline state: vertex function, fragment function, pixel formats
- [ ] **Demo 01:** hello triangle with vertex buffer
- [ ] Index buffers and indexed drawing
- [ ] Per-vertex colour and interpolation
- [ ] Uniform data: `setVertexBytes` vs buffers, alignment rules between C++ and MSL
- [ ] Triple buffering / frames in flight, with a semaphore
- [ ] **Demo 02:** animated, rotating quad with a uniform
- [ ] Textures: creation, upload, samplers, mipmaps, anisotropic filtering
- [ ] **Demo 03:** textured quad (load images with stb_image or ImageIO)
- [ ] Blending and alpha
- [ ] Resize handling: drawable size, recreating size-dependent resources
- [ ] Compute basics: compute pipeline, threadgroups, dispatch
- [ ] **Demo 04:** compute shader that writes a texture (animated procedural image)
- [ ] ImGui panel to tweak every demo's parameters live

**Check:** can I explain what happens on CPU and GPU between `commit()` and the pixel appearing on screen?

---

## Phase 2: 3D fundamentals

- [ ] Depth buffer, depth test, depth state; reversed-Z
- [ ] Back-face culling and winding order
- [ ] **Demo 05:** rotating 3D cube with perspective camera
- [ ] Fly camera (WASD + mouse) and orbit camera
- [ ] Frame timing and delta time; fixed vs variable timestep
- [ ] Vertex layouts and vertex descriptors
- [ ] Load meshes with Assimp (OBJ, then glTF)
- [ ] Mesh, submesh and material data structures
- [ ] Normals, tangents and bitangents; generating tangent space
- [ ] Texture coordinate handling and sRGB vs linear textures
- [ ] Scene graph basics: transform hierarchy, world matrices, dirty flags
- [ ] **Demo 06:** load and display a textured model
- [ ] Instancing: draw many copies with per-instance data
- [ ] **Demo 07:** thousands of instanced objects

**Check:** can I explain every transform a vertex goes through, and why normals need the inverse-transpose?

---

## Phase 3: Lighting and materials

- [ ] Lambert diffuse
- [ ] Phong and Blinn-Phong specular
- [ ] Point, directional and spot lights; attenuation
- [ ] Normal mapping (tangent space)
- [ ] Parallax mapping / parallax occlusion mapping
- [ ] **Demo 08:** Blinn-Phong with several lights and a normal map
- [ ] Radiometry basics: radiance, irradiance, the rendering equation
- [ ] Physically based shading: Cook-Torrance BRDF (GGX distribution, Smith geometry, Schlick Fresnel)
- [ ] Metallic-roughness workflow, ORM textures, glTF 2.0 materials
- [ ] Energy conservation
- [ ] Linear-space lighting, gamma correction
- [ ] Alpha testing, alpha blending, order-independent transparency (weighted blended OIT)
- [ ] **Demo 09:** PBR sphere grid (roughness vs metallic) then a glTF PBR model
- [ ] Emissive materials
- [ ] Clear coat, sheen, anisotropy, subsurface (reading + one implementation)

**Check:** can I explain each term of the Cook-Torrance BRDF and what each one looks like when removed?

---

## Phase 4: Shadows

- [ ] Shadow mapping: depth-only pass, light-space matrix, comparison sampler
- [ ] Bias, slope-scaled bias, normal-offset bias; acne and peter-panning
- [ ] Percentage-closer filtering (PCF)
- [ ] Percentage-closer soft shadows (PCSS)
- [ ] Orthographic (directional) vs perspective (spot) shadow maps
- [ ] Point-light shadows with cube maps
- [ ] **Demo 10:** directional light shadow with PCF
- [ ] Cascaded shadow maps (CSM): split selection, stabilization, blending between cascades
- [ ] Variance shadow maps / exponential variants (reading + implementation)
- [ ] Contact shadows (screen-space)
- [ ] Shadow atlas for many lights
- [ ] Ray-traced shadows (see [Phase 12](#phase-12-ray-tracing))
- [ ] **Demo 11:** cascaded shadow maps over a large scene

**Check:** can I explain why every shadow artifact (acne, peter-panning, shimmering) happens?

---

## Phase 5: Image-based lighting and HDR

- [ ] HDR render targets (`RGBA16Float`)
- [ ] Cube maps and skyboxes
- [ ] Equirectangular to cube map conversion (compute)
- [ ] Tone mapping: Reinhard, ACES, AgX, Khronos PBR Neutral
- [ ] Auto exposure (histogram-based)
- [ ] Diffuse irradiance map (spherical harmonics or convolution)
- [ ] Prefiltered specular environment map (split-sum approximation)
- [ ] BRDF integration lookup texture
- [ ] **Demo 12:** PBR with image-based lighting and a skybox
- [ ] Procedural sky (Preetham or Hosek-Wilkie) and atmospheric scattering (Bruneton / Hillaire)
- [ ] Reflection probes and box-projected cube maps

**Check:** can I derive the split-sum approximation and say what it throws away?

---

## Phase 6: Frame structure

How a frame is organised. This is where a renderer starts to turn into an engine.

- [ ] Forward rendering and its limits with many lights
- [ ] Deferred rendering: G-buffer layout, lighting pass
- [ ] Tile-based GPUs: tile memory, programmable blending, memoryless render targets (Apple GPU specifics)
- [ ] Apple-specific deferred rendering with tile shaders and imageblocks
- [ ] Forward+ (tiled forward) with a light culling compute pass
- [ ] Clustered shading (3D light clusters)
- [ ] **Demo 13:** deferred renderer with hundreds of point lights
- [ ] **Demo 14:** clustered forward with the same scene, compare cost
- [ ] Visibility buffer rendering
- [ ] Render graph (frame graph): passes, resources, automatic barriers and lifetimes
- [ ] Transient resource aliasing / memory heaps
- [ ] Debug views: G-buffer visualisation, overdraw, light heatmap
- [ ] GPU timing with counter sample buffers; per-pass profiling UI

**Check:** can I explain which architecture I would pick for a given game and why?

---

## Phase 7: Metal 4 in depth

Revisit everything with the Metal 4 API model. Check the current Apple docs and WWDC sessions for exact details.

**Core model changes**

- [ ] Why Metal 4 exists: lower CPU overhead, explicit memory, unified encoders
- [ ] `MTL4CommandQueue` and `MTL4CommandBuffer`, with command buffers reusable and decoupled from the queue
- [ ] `MTL4CommandAllocator`: ownership and reset
- [ ] Explicit synchronisation with shared events and barriers between passes
- [ ] Unified compute encoder (compute, blit and acceleration structure work in one encoder)
- [ ] Render pass encoding in Metal 4, including suspending and resuming passes
- [ ] **Argument tables** (`MTL4ArgumentTable`) replacing implicit resource binding
- [ ] **Residency sets** (`MTLResidencySet`): explicit residency for resources
- [ ] Resource views and bindless access with resource IDs in shaders
- [ ] Placement heaps and sparse resources
- [ ] `MTL4Compiler` and explicit pipeline creation; pipeline data sets and binary archives
- [ ] Flexible pipeline states (specialise pipelines without full recompiles)
- [ ] Shader function linking and visible function tables
- [ ] Port **Demo 01 to 05** to the Metal 4 model

**Metal 4 + machine learning**

- [ ] `MTLTensor` and the ML command encoder
- [ ] Running an ML network inside the frame
- [ ] Shader ML (calling inference from inside shaders)
- [ ] **Demo:** small neural network applied to a texture (e.g. denoise or style filter)

**Metal 4 + MetalFX**

- [ ] MetalFX spatial and temporal upscaling
- [ ] MetalFX frame interpolation
- [ ] MetalFX denoised upscaling for ray tracing

**Tooling**

- [ ] Metal debugger, shader debugger, GPU counters, Metal System Trace
- [ ] Shader validation and API validation, with fixes for any warnings

**Check:** can I explain argument tables, residency sets and barriers well enough to move a renderer from Metal 3 to Metal 4?

---

## Phase 8: GPU-driven rendering

- [ ] Indirect draw and indirect command buffers (ICB)
- [ ] GPU-side frustum culling in compute
- [ ] Occlusion culling with a Hi-Z depth pyramid (two-pass)
- [ ] Back-face and cone culling for meshlets
- [ ] **Demo 15:** a hundred thousand objects culled and drawn on the GPU
- [ ] Mesh shaders (object and mesh stages) and meshlet generation (meshoptimizer)
- [ ] Bindless textures and materials
- [ ] Multi-draw / merged vertex and index buffers (mega-buffers)
- [ ] Level of detail: discrete LODs, selection by screen-space error, mesh simplification
- [ ] Virtualised geometry concepts (Nanite-style clusters), reading and a small prototype
- [ ] Texture streaming and sparse textures
- [ ] Mesh optimisation: vertex cache, overdraw, quantisation, compression

**Check:** can I draw a whole scene with a handful of draw calls and no CPU per-object work?

---

## Phase 9: Advanced lighting and global illumination

- [ ] Ambient occlusion: SSAO, HBAO, GTAO
- [ ] Screen-space reflections (SSR), with hierarchical tracing
- [ ] Screen-space global illumination (SSGI)
- [ ] Light probes and irradiance volumes (DDGI-style)
- [ ] Lightmaps and baking (reading + a simple baker)
- [ ] Voxel cone tracing
- [ ] Reflective shadow maps and light propagation volumes (reading)
- [ ] Screen-space radiance caching concepts (reading)
- [ ] Area lights with linearly transformed cosines (LTC)
- [ ] Volumetric fog and light shafts (froxel-based)
- [ ] Participating media, clouds and volumetric rendering (raymarching)
- [ ] Subsurface scattering for skin (screen-space), hair and fur shading
- [ ] Water: waves, refraction, reflection, foam
- [ ] Decals

**Check:** can I name what each GI technique approximates and what artifacts it has?

---

## Phase 10: Post-processing and upscaling

- [ ] Fullscreen pass and compute-based post-processing
- [ ] Bloom (downsample/upsample chain)
- [ ] Depth of field (gather and scatter)
- [ ] Motion blur (camera and per-object)
- [ ] Colour grading with 3D LUTs
- [ ] Vignette, chromatic aberration, film grain, lens flare
- [ ] Anti-aliasing: MSAA, FXAA, SMAA
- [ ] **Temporal anti-aliasing (TAA):** jitter, velocity buffer, history reprojection, neighbourhood clamping
- [ ] MetalFX temporal upscaling integrated with the TAA inputs
- [ ] Dynamic resolution
- [ ] Variable rate shading / rasterisation rate maps
- [ ] **Demo 16:** full post-processing stack with toggles in ImGui

**Check:** can I explain why TAA ghosts and how each fix works?

---

## Phase 11: Animation, particles and world

- [ ] Skeletal animation: skinning matrices, skeleton hierarchy, GPU skinning
- [ ] Animation blending, state machines, inverse kinematics
- [ ] Morph targets / blend shapes
- [ ] GPU particle system (emit, simulate, sort, render) in compute
- [ ] Soft particles, flipbooks, particle lighting
- [ ] Terrain: heightmaps, tessellation or clipmaps, splat-map texturing
- [ ] Foliage: instancing, wind animation, alpha-to-coverage
- [ ] Procedural generation with compute (noise, erosion)
- [ ] Text rendering: bitmap fonts, signed-distance-field and MSDF text
- [ ] Debug rendering: lines, gizmos, bounding boxes

**Check:** can I animate and render a skinned character with a thousand particles around it?

---

## Phase 12: Ray tracing

- [ ] Ray tracing theory: BVH, ray-triangle intersection, traversal
- [ ] Metal acceleration structures: bottom-level (BLAS) and top-level (TLAS), building and refitting
- [ ] Intersection functions and ray tracing in compute shaders
- [ ] **Demo 17:** ray-traced hard shadows
- [ ] Ray-traced reflections
- [ ] Ray-traced ambient occlusion
- [ ] Path tracing basics: Monte Carlo integration, importance sampling, multiple importance sampling
- [ ] **Demo 18:** progressive path tracer in a compute shader
- [ ] Denoising: temporal accumulation, SVGF-style, MetalFX denoised upscaling
- [ ] Hybrid rendering: raster G-buffer with ray-traced lighting
- [ ] ReSTIR and many-light sampling (reading)

**Check:** can I explain why a path tracer converges and what denoisers trade away?

---

## Phase 13: Engine architecture

Everything around the renderer. This is where Assimp, Jolt and miniaudio come in.

- [ ] Project structure: core, platform, rhi, renderer, scene, assets, tools
- [ ] Entity component system (ECS) or another data-oriented scene model
- [ ] Asset pipeline: import with Assimp, convert to a runtime format, cache
- [ ] Resource manager with handles and reference counting
- [ ] Job system and multi-threaded command recording
- [ ] Memory allocators: linear, pool, ring buffers for per-frame data
- [ ] Shader pipeline: hot reload, permutations, include files, reflection
- [ ] Material system and material editor in ImGui
- [ ] Input system with action maps
- [ ] Physics with Jolt: rigid bodies, collision shapes, character controller, debug rendering
- [ ] Audio with miniaudio: playback, 3D positional sound, streaming
- [ ] Serialisation: scene save and load
- [ ] Logging, assertions, CPU and GPU profilers
- [ ] **Demo 19:** small scene with physics, audio and a controllable character

**Check:** can I add a new object type end-to-end (asset, component, render, physics, sound) without touching engine internals?

---

## Phase 14: The RHI

Design the abstraction after knowing Metal 4 well, and _before_ writing Vulkan, so the second backend proves the design.

**Design**

- [ ] Write down the lowest common denominator of Metal 4 and Vulkan 1.3 (and what to expose beyond it)
- [ ] Pick an object model: device, queue, command list, buffer, texture, sampler, pipeline, descriptor / argument binding
- [ ] Handle-based resources (generational indices) instead of raw pointers
- [ ] Decide how bindings work: bindless first, with Metal argument tables and Vulkan descriptor indexing underneath
- [ ] Decide the synchronisation model: render-graph-driven barriers, explicit fences and semaphores
- [ ] Decide the memory model: heaps, sub-allocation, upload and readback paths
- [ ] Pipeline state objects and a cache; dynamic state vs baked state
- [ ] Decide how errors and validation messages are reported

**Shaders**

- [ ] Choose a shader strategy: write Slang or HLSL and compile to MSL and SPIR-V, or maintain both MSL and GLSL
- [ ] Integrate the compiler (Slang, DXC + SPIRV-Cross, or glslang) into CMake
- [ ] Shader reflection to build binding layouts automatically
- [ ] Shared header for CPU/GPU struct layouts

**Implementation**

- [ ] `rhi/` interface headers with no backend types leaking
- [ ] Metal 4 backend implementing the RHI
- [ ] Port the renderer (Phases 1 to 6) onto the RHI, running on Metal
- [ ] Render graph on top of the RHI
- [ ] Unit tests for the RHI that run against every backend (for example, render a known image and compare it)
- [ ] Golden image tests for the demos

**Check:** can I read a renderer pass and tell nothing in it is backend-specific?

---

## Phase 15: Vulkan

Learn Vulkan directly, then plug it in behind the RHI. On macOS this goes through MoltenVK, so note any missing features.

**Setup**

- [ ] Install the Vulkan SDK / MoltenVK; configure `find_package(Vulkan)` or load via volk
- [ ] Instance, validation layers, debug messenger
- [ ] Physical device selection and queue families
- [ ] Logical device and queues
- [ ] Surface and swapchain with SDL3 (`SDL_Vulkan_CreateSurface`)
- [ ] Image views, render passes vs **dynamic rendering**, framebuffers
- [ ] **Demo V01:** hello triangle in Vulkan

**Core concepts**

- [ ] Command pools and command buffers; recording and submission
- [ ] Synchronisation: fences, semaphores, pipeline barriers, **synchronization2**, timeline semaphores
- [ ] Frames in flight
- [ ] Memory: allocation, **VMA** (Vulkan Memory Allocator), staging buffers
- [ ] Descriptors: sets, layouts, pools; **descriptor indexing** / bindless; buffer device address
- [ ] Push constants
- [ ] Pipelines, pipeline cache, specialisation constants, dynamic state
- [ ] Shaders: SPIR-V, compile from GLSL/HLSL/Slang
- [ ] Textures, mipmaps and image layout transitions
- [ ] Depth buffer, MSAA
- [ ] Compute pipelines and compute/graphics synchronisation
- [ ] Mesh shaders, ray tracing extensions (`VK_KHR_ray_tracing_pipeline`, `VK_KHR_acceleration_structure`), dynamic rendering local read

**Backend**

- [ ] Vulkan backend implementing the RHI
- [ ] Run **every** earlier demo on the Vulkan backend with no renderer code changes
- [ ] Compare Metal and Vulkan: frame time, CPU overhead, code size, pain points; write the findings in `notes/`
- [ ] Fix every RHI design flaw the second backend exposes

**Check:** can I explain every Vulkan synchronisation primitive and why Metal needs fewer of them?

---

## Phase 16: Capstone

Pull it all together into one demo that shows the engine off.

- [ ] A scene with PBR materials, IBL, cascaded shadows and a skinned character
- [ ] GPU-driven culling, clustered lighting, TAA and MetalFX upscaling
- [ ] Ray-traced shadows or reflections where supported
- [ ] Physics-driven props and positional audio
- [ ] ImGui editor: scene hierarchy, material editor, profiler, render graph viewer
- [ ] Runs on Metal 4 and on Vulkan from the same code, with a runtime switch
- [ ] Write-up: what I learned, what I would redo, benchmarks

---

## Reference material

**Books**

- _Real-Time Rendering_, 4th edition (Akenine-Möller, Haines, Hoffman)
- _Physically Based Rendering: From Theory to Implementation_ (pbr-book.org, free online)
- _Game Engine Architecture_ (Jason Gregory)
- _GPU Gems_ series and _GPU Zen_ (free online for some)
- _Ray Tracing Gems_ (free online)

**Metal and Apple**

- Apple developer documentation for Metal, and the _Metal Shading Language Specification_ (PDF)
- WWDC sessions on Metal 4, MetalFX, ray tracing and GPU-driven rendering
- Apple sample code: "Modern Rendering with Metal", "Deferred Lighting", "GPU-driven rendering"
- metal-cpp readme and headers in `lib/Metal-cpp`

**Vulkan**

- [Vulkan Tutorial](https://vulkan-tutorial.com) and [vkguide.dev](https://vkguide.dev)
- The Vulkan specification and Khronos Vulkan samples
- Sascha Willems' Vulkan examples
- MoltenVK documentation (feature gaps on macOS)

**Techniques**

- Learn OpenGL (learnopengl.com) for PBR, IBL, SSAO and shadow theory (the ideas map straight to Metal)
- Filament design document (PBR and real-time lighting reasoning)
- Unreal Engine 5 papers: Nanite, Lumen (SIGGRAPH advances talks)
- SIGGRAPH "Advances in Real-Time Rendering" course notes
- Frostbite, DOOM (2016/Eternal), Call of Duty and other GDC talks on frame structure
- Jendrik Illner's "Graphics Programming Weekly"

**Tools**

- Xcode Metal debugger and GPU frame capture
- RenderDoc (for the Vulkan side)
- Tracy profiler for CPU
- meshoptimizer, Slang, SPIRV-Cross, VMA, volk

---

## Progress log

Add a line when a phase or major demo is finished: date, what I built, what surprised me.

| Date       | Milestone                                                        | Notes |
| ---------- | ---------------------------------------------------------------- | ----- |
| 2026-10-12 | Project set up, all libraries linked, ImGui render loop on Metal |       |

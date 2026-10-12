# GraphicsRendererDemos

A personal learning project for real-time rendering. The goal is to learn **Metal 4** first, then **Vulkan**.
Along the way I'll build a small graphics framework with an **RHI** (render hardware interface) that hides the
backend, and use it to implement real-time rendering algorithms from the basics up to engine-level techniques.

See [plan.md](plan.md) for the detailed, checkable learning roadmap.

## Status

- [x] Project skeleton, CMake build, third-party libraries added as submodules
- [x] Smoke test for every library (`--smoke-test`)
- [x] Window + Metal layer + ImGui render loop
- [ ] First triangle, then everything in [plan.md](plan.md)

## Libraries

All libraries are git submodules in `lib/`.

| Library | Purpose |
|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | Window, input, events, Metal view / Vulkan surface |
| [metal-cpp](https://developer.apple.com/metal/cpp/) | C++ bindings for Metal (headers checked in) |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | Vulkan API headers (MoltenVK or the Vulkan SDK is needed to run on macOS) |
| [glm](https://github.com/g-truc/glm) | Vector and matrix math |
| [Dear ImGui](https://github.com/ocornut/imgui) | Debug UI, with the SDL3, Metal and Vulkan backends |
| [Assimp](https://github.com/assimp/assimp) | Model and scene loading |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Physics, for later engine-style demos |
| [miniaudio](https://github.com/mackron/miniaudio) | Audio, for later engine-style demos |

## Requirements

- macOS on Apple Silicon, with a recent Xcode (Metal 4 needs a recent macOS SDK and an Apple GPU that supports it)
- CMake 3.10 or newer
- The [LunarG Vulkan SDK](https://vulkan.lunarg.com) (or equivalent): the Vulkan loader, MoltenVK and `glslc`
- A C++23 compiler (Apple Clang)

## Building

```sh
git clone --recurse-submodules <repo-url>
cd GraphicsRendererDemos

# If you already cloned without submodules:
git submodule update --init --recursive

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j8
```

## Running

```sh
./build/GraphicsRendererDemos               # interactive demo window (starts on Metal)
./build/GraphicsRendererDemos --vulkan      # start on Vulkan instead
./build/GraphicsRendererDemos --smoke-test  # headless check that every library works
```

## Layout

```
CMakeLists.txt     Build configuration for the app and all libraries
lib/               Third-party libraries (git submodules, plus metal-cpp)
src/main.cpp       App entry point, window handling and ImGui UI (including the Metal/Vulkan switch)
src/renderer.h     Minimal backend interface (first sketch of the RHI)
src/metal_renderer.cpp   Metal backend, shaders in MSL
src/vulkan_renderer.cpp  Vulkan backend (via MoltenVK), shaders in GLSL -> SPIR-V
src/shaders/       triangle.metal, triangle.vert, triangle.frag
src/metal_impl.cpp metal-cpp implementation (must exist in exactly one source file)
src/smoke_tests.*  Headless library checks
plan.md            Learning roadmap with checkboxes
```

The layout will grow as the framework is built. The intended direction is a small `rhi/` layer (with `metal/` and
`vulkan/` backends), a `renderer/` layer on top of it, and a `demos/` folder with one self-contained demo per
technique.

## Notes

- `lib/JoltPhysics` is built with its GPU compute backends turned off. They need shader compilers that this project
  doesn't use.
- metal-cpp's implementation defines (`NS_PRIVATE_IMPLEMENTATION` and friends) must appear in exactly one source
  file. Here that is `src/metal_impl.cpp`.

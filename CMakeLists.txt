cmake_minimum_required(VERSION 3.10)


project(GraphicsRendererDemos LANGUAGES C CXX OBJC OBJCXX)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_library(METALFX_FRAMEWORK MetalFX REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)


# SDL3
set(SDL_SHARED ON CACHE BOOL "Build shared SDL library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Build static SDL library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Disable SDL test programs" FORCE)
add_subdirectory(lib/SDL3)

# GLM (header-only, provides glm::glm)
add_subdirectory(lib/glm)

# Vulkan headers (provides Vulkan::Headers)
add_subdirectory(lib/Vulkan-Headers)

# Assimp
# configure Assimp to build as a static library and disable tests and installation
# this is necessary to avoid conflicts with other versions of Assimp that may be installed on the system
# each one of these options is set to OFF to disable the corresponding feature, and FORCE is used to ensure that the option is set even if it was previously set to a different value
#explained:
# build shared libs: OFF (we want static linking)
# build tests: OFF (we don't need to run tests)
# install: OFF (we don't want to install Assimp, we just want to use it as a dependency)
# build assimp_view: OFF (we don't need the Assimp viewer)
# no export: ON (we don't want to export symbols, we just want to use Assimp as a static library)
# warnings as errors: OFF (we don't want to treat warnings as errors, we just want to build Assimp)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/assimp)


# Jolt Physics (CMake project lives in Build/)
#explained:
# Jolt Physics is a physics engine that can be built as a CMake project. The CMake project for Jolt Physics lives in the Build/ directory of the Jolt Physics source code. The following options are set to configure the build of Jolt Physics:
# Target unit tests: OFF (we don't need to run unit tests)
# Target hello world: OFF (we don't need the hello world example)
# Target performance test: OFF (we don't need the performance test)
# Target samples: OFF (we don't need the samples)

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
# GPU compute is not needed, and it requires shader compilers (glslc/dxc/metal)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/JoltPhysics/Build)

# miniaudio (single header, add the include path below)

# Dear ImGui (no upstream CMake support, build it as a static library)
add_library(imgui STATIC
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/backends/imgui_impl_sdl3.cpp
    lib/imgui/backends/imgui_impl_metal.mm
    lib/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    lib/imgui
    lib/imgui/backends
    lib/Metal-cpp
)
target_compile_definitions(imgui PUBLIC VK_NO_PROTOTYPES IMGUI_IMPL_METAL_CPP)
target_link_libraries(imgui PUBLIC
    SDL3::SDL3
    Vulkan::Headers
    ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
)


add_executable(GraphicsRendererDemos
    src/main.cpp
    src/metal_impl.cpp
    src/smoke_tests.cpp
)

# metal-cpp is header-only
target_include_directories(GraphicsRendererDemos PRIVATE lib/Metal-cpp lib/miniaudio)

target_link_libraries(GraphicsRendererDemos PRIVATE
    ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${METALFX_FRAMEWORK}
    glm::glm
    SDL3::SDL3
    Vulkan::Headers
    imgui
    assimp::assimp
    Jolt
)

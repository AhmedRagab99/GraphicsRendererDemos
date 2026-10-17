cmake_minimum_required(VERSION 3.10)


project(GraphicsRendererDemos LANGUAGES C CXX OBJC OBJCXX)


# Build type for single-config generators (Makefiles, Ninja): -DCMAKE_BUILD_TYPE=Debug|Release, defaults to Debug.
# Multi-config generators (Xcode, Visual Studio) pick it at build time: cmake --build <dir> --config Release
get_property(IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IS_MULTI_CONFIG)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
    message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_library(METALFX_FRAMEWORK MetalFX REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(APPKIT_FRAMEWORK AppKit REQUIRED)


# Tools and libraries for the Vulkan backend and shader compilation.
# The Vulkan loader + MoltenVK come from the LunarG SDK (or Homebrew). Headers come from the submodule.
find_library(VULKAN_LOADER vulkan
    HINTS /usr/local/lib /opt/homebrew/lib "$ENV{VULKAN_SDK}/lib"
    REQUIRED)
find_program(GLSLC glslc HINTS /opt/homebrew/bin /usr/local/bin "$ENV{VULKAN_SDK}/bin" REQUIRED)
find_program(XCRUN xcrun REQUIRED)
find_program(SLANGC slangc HINTS /usr/local/bin "$ENV{VULKAN_SDK}/bin" REQUIRED)

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
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_METAL_CPP)
target_link_libraries(imgui PUBLIC
    SDL3::SDL3
    Vulkan::Headers
    ${VULKAN_LOADER}
    ${METAL_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
)


add_executable(GraphicsRendererDemos
    src/main.cpp
    src/metal_impl.cpp
    src/metal_renderer.cpp
    src/vulkan_renderer.cpp
    src/smoke_tests.cpp
)

# Shaders: one program written twice. MSL -> .metallib for Metal, GLSL -> SPIR-V for Vulkan.
# They are written next to the executable, in <exe dir>/shaders/.
set(SHADER_SRC ${CMAKE_SOURCE_DIR}/src/shaders)
set(SHADER_OUT $<TARGET_FILE_DIR:GraphicsRendererDemos>/shaders)
set(SHADER_TMP ${CMAKE_BINARY_DIR}/shader_tmp)

add_custom_command(
    OUTPUT ${SHADER_TMP}/triangle.metallib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TMP}
    COMMAND ${XCRUN} -sdk macosx metal -c ${SHADER_SRC}/triangle.metal -o ${SHADER_TMP}/triangle.air
    COMMAND ${XCRUN} -sdk macosx metallib ${SHADER_TMP}/triangle.air -o ${SHADER_TMP}/triangle.metallib
    DEPENDS ${SHADER_SRC}/triangle.metal
    COMMENT "Compiling MSL: triangle.metal"
    VERBATIM)
add_custom_command(
    OUTPUT ${SHADER_TMP}/triangle.vert.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TMP}
    COMMAND ${GLSLC} ${SHADER_SRC}/triangle.vert -o ${SHADER_TMP}/triangle.vert.spv
    DEPENDS ${SHADER_SRC}/triangle.vert
    COMMENT "Compiling GLSL: triangle.vert"
    VERBATIM)
add_custom_command(
    OUTPUT ${SHADER_TMP}/triangle.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TMP}
    COMMAND ${GLSLC} ${SHADER_SRC}/triangle.frag -o ${SHADER_TMP}/triangle.frag.spv
    DEPENDS ${SHADER_SRC}/triangle.frag
    COMMENT "Compiling GLSL: triangle.frag"
    VERBATIM)
# Slang: the same shader written once, translated to MSL (then compiled like the native one) and to SPIR-V.
# Slang's default matrix layout is row-major; glm is column-major.
add_custom_command(
    OUTPUT ${SHADER_TMP}/triangle_slang.metallib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TMP}
    COMMAND ${SLANGC} ${SHADER_SRC}/triangle.slang -matrix-layout-column-major -target metal -o ${SHADER_TMP}/triangle_slang.metal
    COMMAND ${XCRUN} -sdk macosx metal -c ${SHADER_TMP}/triangle_slang.metal -o ${SHADER_TMP}/triangle_slang.air
    COMMAND ${XCRUN} -sdk macosx metallib ${SHADER_TMP}/triangle_slang.air -o ${SHADER_TMP}/triangle_slang.metallib
    DEPENDS ${SHADER_SRC}/triangle.slang
    COMMENT "Compiling Slang -> MSL: triangle.slang"
    VERBATIM)
add_custom_command(
    OUTPUT ${SHADER_TMP}/triangle_slang.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TMP}
    COMMAND ${SLANGC} ${SHADER_SRC}/triangle.slang -matrix-layout-column-major -target spirv -o ${SHADER_TMP}/triangle_slang.spv
    DEPENDS ${SHADER_SRC}/triangle.slang
    COMMENT "Compiling Slang -> SPIR-V: triangle.slang"
    VERBATIM)
add_custom_target(shaders DEPENDS
    ${SHADER_TMP}/triangle_slang.metallib
    ${SHADER_TMP}/triangle_slang.spv
    ${SHADER_TMP}/triangle.metallib
    ${SHADER_TMP}/triangle.vert.spv
    ${SHADER_TMP}/triangle.frag.spv)
add_dependencies(GraphicsRendererDemos shaders)
add_custom_command(TARGET GraphicsRendererDemos POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_TMP}/triangle.metallib ${SHADER_TMP}/triangle.vert.spv ${SHADER_TMP}/triangle.frag.spv
        ${SHADER_TMP}/triangle_slang.metallib ${SHADER_TMP}/triangle_slang.spv ${SHADER_OUT}
    COMMENT "Copying shaders next to the executable")

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

# Convenience targets. Build with: cmake --build <dir> --target <name>
#   run          build + launch the demo (from the source dir)
#   open-xcode   generate (if needed) and open the Xcode project   (macOS)
#   open-vscode  open the folder in VS Code
#   open-vs      generate (if needed) and open the Visual Studio solution (Windows)
add_custom_target(run
    COMMAND $<TARGET_FILE:GraphicsRendererDemos>
    DEPENDS GraphicsRendererDemos
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    COMMENT "Running GraphicsRendererDemos")

if(APPLE)
    set(XCODE_BUILD_DIR ${CMAKE_SOURCE_DIR}/build-xcode)
    if(CMAKE_GENERATOR STREQUAL "Xcode")
        set(XCODE_BUILD_DIR ${CMAKE_BINARY_DIR})
        set(XCODE_GENERATE_CMD ${CMAKE_COMMAND} -E echo "Xcode project already generated")
    else()
        set(XCODE_GENERATE_CMD ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${XCODE_BUILD_DIR} -G Xcode)
    endif()
    add_custom_target(open-xcode
        COMMAND ${XCODE_GENERATE_CMD}
        COMMAND open ${XCODE_BUILD_DIR}/${PROJECT_NAME}.xcodeproj
        USES_TERMINAL
        COMMENT "Opening Xcode project")
endif()

find_program(CODE_EXECUTABLE code)
if(CODE_EXECUTABLE)
    add_custom_target(open-vscode
        COMMAND ${CODE_EXECUTABLE} ${CMAKE_SOURCE_DIR}
        COMMENT "Opening VS Code")
elseif(APPLE)
    add_custom_target(open-vscode
        COMMAND open -a "Visual Studio Code" ${CMAKE_SOURCE_DIR}
        COMMENT "Opening VS Code")
endif()

if(WIN32)
    set(VS_BUILD_DIR ${CMAKE_SOURCE_DIR}/build-vs)
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        set(VS_BUILD_DIR ${CMAKE_BINARY_DIR})
        set(VS_GENERATE_CMD ${CMAKE_COMMAND} -E echo "Visual Studio solution already generated")
    else()
        set(VS_GENERATE_CMD ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR} -B ${VS_BUILD_DIR} -G "Visual Studio 17 2022")
    endif()
    add_custom_target(open-vs
        COMMAND ${VS_GENERATE_CMD}
        COMMAND ${CMAKE_COMMAND} -E env cmd /c start "" ${VS_BUILD_DIR}/${PROJECT_NAME}.sln
        USES_TERMINAL
        COMMENT "Opening Visual Studio solution")
endif()

# Xcode scheme settings. Only used when configured with the Xcode generator (cmake --preset xcode), ignored otherwise.
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(GraphicsRendererDemos PROPERTIES
        XCODE_GENERATE_SCHEME ON
        # Metal debugger: adds the GPU frame capture button to Xcode's debug bar
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
        # Same as MTL_DEBUG_LAYER=1 and MTL_SHADER_VALIDATION=1
        XCODE_SCHEME_ENABLE_GPU_API_VALIDATION ON
        XCODE_SCHEME_ENABLE_GPU_SHADER_VALIDATION ON
        XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

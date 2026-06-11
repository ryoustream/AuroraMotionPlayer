# Compile GLSL shaders to SPIR-V at build time
# Requires glslc (from Vulkan SDK) or glslangValidator

cmake_minimum_required(VERSION 3.20)

find_program(GLSLC glslc HINTS
    "$ENV{VULKAN_SDK}/bin"
    "C:/VulkanSDK/1.3.275.0/Bin"
    "/usr/bin"
)

if(NOT GLSLC)
    find_program(GLSLC glslangValidator HINTS
        "$ENV{VULKAN_SDK}/bin"
        "C:/VulkanSDK/1.3.275.0/Bin"
        "/usr/bin"
    )
    if(GLSLC)
        message(STATUS "[Shaders] Using glslangValidator: ${GLSLC}")
        set(GLSLC_CMD "${GLSLC}" -V -o)
    else()
        message(WARNING "[Shaders] glslc/glslangValidator not found — SPIR-V will not be compiled at build time")
        return()
    endif()
else()
    message(STATUS "[Shaders] Using glslc: ${GLSLC}")
    set(GLSLC_CMD "${GLSLC}" -o)
endif()

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

set(GLSL_SOURCES
    yuv_to_rgb.vert
    yuv_to_rgb.frag
    nv12_to_rgb.frag
)

set(SPIRV_OUTPUTS)
foreach(GLSL_SRC ${GLSL_SOURCES})
    set(SPIRV_OUT "${SHADER_OUTPUT_DIR}/${GLSL_SRC}.spv")
    add_custom_command(
        OUTPUT  "${SPIRV_OUT}"
        COMMAND ${GLSLC_CMD} "${SPIRV_OUT}" "${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_SRC}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${GLSL_SRC}"
        COMMENT "Compiling shader: ${GLSL_SRC} → ${GLSL_SRC}.spv"
        VERBATIM
    )
    list(APPEND SPIRV_OUTPUTS "${SPIRV_OUT}")
endforeach()

add_custom_target(aurora_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# Install compiled SPIR-V next to the executable
install(FILES ${SPIRV_OUTPUTS} DESTINATION bin/shaders)

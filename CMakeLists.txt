cmake_minimum_required(VERSION 3.20)

#---------------------------------------------------------------------------------
# devkitPro Setup (must be before project() for cross-compilation)
#---------------------------------------------------------------------------------
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITA64 ${DEVKITPRO}/devkitA64)
set(PORTLIBS ${DEVKITPRO}/portlibs/switch)
set(LIBNX ${DEVKITPRO}/libnx)

# Cross-compilation toolchain setup (must be before project())
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-gcc)
set(CMAKE_CXX_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-g++)
set(CMAKE_ASM_COMPILER ${DEVKITA64}/bin/aarch64-none-elf-gcc)
set(CMAKE_AR ${DEVKITA64}/bin/aarch64-none-elf-ar)
set(CMAKE_STRIP ${DEVKITA64}/bin/aarch64-none-elf-strip)
set(CMAKE_RANLIB ${DEVKITA64}/bin/aarch64-none-elf-ranlib)

project(NX-Shell VERSION 5.0.0 LANGUAGES C CXX)

#---------------------------------------------------------------------------------
# Project Configuration
#---------------------------------------------------------------------------------
set(APP_TITLE "NX-Shell")
set(APP_AUTHOR "Joel16")
set(APP_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(ROMFS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)

#---------------------------------------------------------------------------------
# Compiler Flags
#---------------------------------------------------------------------------------
set(ARCH_FLAGS "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE")
set(COMMON_FLAGS "-g -Wall -O3 -ffunction-sections -Wno-restrict ${ARCH_FLAGS} -D__SWITCH__")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -std=gnu++20 -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g ${ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -specs=${DEVKITPRO}/libnx/switch.specs -g ${ARCH_FLAGS}")

#---------------------------------------------------------------------------------
# Parse Dependency Versions from dependencies.json (single source of truth)
#---------------------------------------------------------------------------------
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/dependencies.json" DEPS_JSON)

# Extract versions using CMake's JSON parsing (requires CMake 3.19+)
string(JSON IMGUI_VERSION GET ${DEPS_JSON} "git_dependencies" "imgui" "version")
string(JSON LIBNSBMP_COMMIT GET ${DEPS_JSON} "git_dependencies" "libnsbmp" "commit")
string(JSON STB_COMMIT GET ${DEPS_JSON} "git_dependencies" "stb" "commit")
string(JSON USBHSFS_VERSION GET ${DEPS_JSON} "git_dependencies" "libusbhsfs" "version")
string(JSON USBHSFS_COMMIT GET ${DEPS_JSON} "git_dependencies" "libusbhsfs" "commit")

# Shorten commit hashes for display (first 7 chars)
string(SUBSTRING "${LIBNSBMP_COMMIT}" 0 7 LIBNSBMP_COMMIT_SHORT)
string(SUBSTRING "${STB_COMMIT}" 0 7 STB_COMMIT_SHORT)

message(STATUS "Dependency versions from dependencies.json:")
message(STATUS "  Dear ImGui: ${IMGUI_VERSION}")
message(STATUS "  libusbhsfs: ${USBHSFS_VERSION}")
message(STATUS "  libnsbmp: ${LIBNSBMP_COMMIT_SHORT}")
message(STATUS "  stb: ${STB_COMMIT_SHORT}")

#---------------------------------------------------------------------------------
# Fetch Dependencies
#---------------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        ${IMGUI_VERSION}
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(libnsbmp
    GIT_REPOSITORY https://github.com/netsurf-browser/libnsbmp.git
    GIT_TAG        ${LIBNSBMP_COMMIT}
)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        ${STB_COMMIT}
)

# libusbhsfs pre-built release (building from source requires complex cross-compilation)
FetchContent_Declare(usbhsfs_release
    URL https://github.com/DarkMatterCore/libusbhsfs/releases/download/v${USBHSFS_VERSION}/libusbhsfs_${USBHSFS_VERSION}-main-${USBHSFS_COMMIT}_GPLv2+.tar.bz2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(imgui libnsbmp stb usbhsfs_release)

#---------------------------------------------------------------------------------
# Find devkitPro Tools & Libraries
#---------------------------------------------------------------------------------
find_program(PKG_CONFIG_EXECUTABLE NAMES aarch64-none-elf-pkg-config
    PATHS ${DEVKITPRO}/portlibs/switch/bin NO_DEFAULT_PATH REQUIRED)

find_program(CURL_CONFIG_EXECUTABLE NAMES curl-config
    PATHS ${DEVKITPRO}/portlibs/switch/bin NO_DEFAULT_PATH)

find_program(ELF2NRO_EXECUTABLE NAMES elf2nro
    PATHS ${DEVKITPRO}/tools/bin NO_DEFAULT_PATH REQUIRED)

find_program(NACPTOOL_EXECUTABLE NAMES nacptool
    PATHS ${DEVKITPRO}/tools/bin NO_DEFAULT_PATH REQUIRED)

execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags freetype2
    OUTPUT_VARIABLE FREETYPE_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --libs freetype2
    OUTPUT_VARIABLE FREETYPE_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CURL_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${CURL_CONFIG_EXECUTABLE} --libs
        OUTPUT_VARIABLE CURL_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FREETYPE_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FREETYPE_CFLAGS}")

#---------------------------------------------------------------------------------
# Source Files
#---------------------------------------------------------------------------------
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/misc/freetype/imgui_freetype.cpp
)

file(GLOB_RECURSE PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

#---------------------------------------------------------------------------------
# Executable Target
#---------------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${IMGUI_SOURCES}
    ${PROJECT_SOURCES}
    ${libnsbmp_SOURCE_DIR}/src/libnsbmp.c
)

set_source_files_properties(${libnsbmp_SOURCE_DIR}/src/libnsbmp.c PROPERTIES LANGUAGE C)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    # Version
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_MICRO=${PROJECT_VERSION_PATCH}
    # 3rd party dependency versions (from dependencies.json)
    DEP_IMGUI_VERSION="${IMGUI_VERSION}"
    DEP_LIBUSBHSFS_VERSION="${USBHSFS_VERSION}"
    DEP_LIBNSBMP_COMMIT="${LIBNSBMP_COMMIT_SHORT}"
    DEP_STB_COMMIT="${STB_COMMIT_SHORT}"
    # ImGui configuration
    IMGUI_IMPL_OPENGL_LOADER_GLAD
    IMGUI_DEFINE_MATH_OPERATORS
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    IMGUI_DISABLE_DEMO_WINDOWS
    IMGUI_DISABLE_DEBUG_TOOLS
    IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
    IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
    IMGUI_DISABLE_WIN32_FUNCTIONS
    IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
    IMGUI_DISABLE_STB_TRUETYPE_IMPLEMENTATION
    IMGUI_ENABLE_FREETYPE
)

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/misc/freetype
    ${libnsbmp_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
    ${usbhsfs_release_SOURCE_DIR}/include
    ${PORTLIBS}/include
    ${LIBNX}/include
)

target_link_directories(${PROJECT_NAME}.elf PRIVATE
    ${usbhsfs_release_SOURCE_DIR}/lib
    ${PORTLIBS}/lib
    ${LIBNX}/lib
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    ${FREETYPE_LIBS}
    ${CURL_LIBS}
    gif turbojpeg jpeg png webp jansson
    glad EGL glapi drm_nouveau
    usbhsfs lwext4 ntfs-3g
    minizip nx m z
)

#---------------------------------------------------------------------------------
# NRO Creation
#---------------------------------------------------------------------------------
# Find icon (prefer project-named, fall back to generic)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.jpg)
    set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.jpg)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
    set(ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/icon.jpg)
else()
    set(ICON_FILE "")
endif()

set(NACP_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nacp)
set(NRO_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nro)

add_custom_command(OUTPUT ${NACP_FILE}
    COMMAND ${NACPTOOL_EXECUTABLE} --create "${APP_TITLE}" "${APP_AUTHOR}" "${APP_VERSION}" ${NACP_FILE}
    COMMENT "Creating NACP file"
)

add_custom_command(OUTPUT ${NRO_FILE}
    COMMAND ${ELF2NRO_EXECUTABLE}
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.elf ${NRO_FILE}
        --icon=${ICON_FILE} --nacp=${NACP_FILE} --romfsdir=${ROMFS_DIR}
    DEPENDS ${PROJECT_NAME}.elf ${NACP_FILE}
    COMMENT "Creating NRO file"
)

add_custom_target(nro ALL DEPENDS ${NRO_FILE})

# Copy NRO to source directory for convenience
add_custom_command(TARGET nro POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${NRO_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.nro
    COMMENT "Copying NRO to source directory"
)

#---------------------------------------------------------------------------------
# Install
#---------------------------------------------------------------------------------
install(FILES ${NRO_FILE} DESTINATION .)

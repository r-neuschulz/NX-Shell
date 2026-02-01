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

# Allow version override from CI (e.g., -DPROJECT_VERSION_OVERRIDE=5.1.0)
if(DEFINED PROJECT_VERSION_OVERRIDE)
    project(NX-Shell VERSION ${PROJECT_VERSION_OVERRIDE} LANGUAGES C CXX ASM)
    message(STATUS "Version overridden to: ${PROJECT_VERSION_OVERRIDE}")
else()
    project(NX-Shell VERSION 5.0.0 LANGUAGES C CXX ASM)
endif()

# GitHub repository for update checks (can be overridden via -DGITHUB_REPO_OWNER and -DGITHUB_REPO_NAME)
# These allow forks to automatically check their own releases when built from their CI
if(NOT DEFINED GITHUB_REPO_OWNER)
    set(GITHUB_REPO_OWNER "joel16")
endif()
if(NOT DEFINED GITHUB_REPO_NAME)
    set(GITHUB_REPO_NAME "NX-Shell")
endif()
message(STATUS "Update check repository: ${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}")

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
# Note: Exceptions enabled for nspmini compatibility (uses throw/catch for error handling)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -std=gnu++20 -fno-rtti")
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
string(JSON NSPMINI_COMMIT GET ${DEPS_JSON} "git_dependencies" "nspmini" "commit")

# Shorten nspmini commit hash for display (first 7 chars)
string(SUBSTRING "${NSPMINI_COMMIT}" 0 7 NSPMINI_COMMIT_SHORT)

# Shorten commit hashes for display (first 7 chars)
string(SUBSTRING "${LIBNSBMP_COMMIT}" 0 7 LIBNSBMP_COMMIT_SHORT)
string(SUBSTRING "${STB_COMMIT}" 0 7 STB_COMMIT_SHORT)

message(STATUS "Dependency versions from dependencies.json:")
message(STATUS "  Dear ImGui: ${IMGUI_VERSION}")
message(STATUS "  libusbhsfs: ${USBHSFS_VERSION}")
message(STATUS "  libnsbmp: ${LIBNSBMP_COMMIT_SHORT}")
message(STATUS "  stb: ${STB_COMMIT_SHORT}")
message(STATUS "  nspmini: ${NSPMINI_COMMIT_SHORT}")

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

# nspmini library for NSP installation
FetchContent_Declare(nspmini
    GIT_REPOSITORY https://github.com/StarDustCFW/nspmini.git
    GIT_TAG        ${NSPMINI_COMMIT}
)
FetchContent_MakeAvailable(nspmini)

# Build nspmini library (produces lib/libnsp.a)
set(NSPMINI_LIB "${nspmini_SOURCE_DIR}/lib/libnsp.a")
add_custom_command(
    OUTPUT ${NSPMINI_LIB}
    COMMAND make
    WORKING_DIRECTORY ${nspmini_SOURCE_DIR}
    COMMENT "Building nspmini library"
)
add_custom_target(nspmini_build DEPENDS ${NSPMINI_LIB})

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

find_program(ELF2NSO_EXECUTABLE NAMES elf2nso
    PATHS ${DEVKITPRO}/tools/bin NO_DEFAULT_PATH REQUIRED)

find_program(NPDMTOOL_EXECUTABLE NAMES npdmtool
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
# Forwarder Build (built first, then bundled into main app's romfs)
#---------------------------------------------------------------------------------
set(FORWARDER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/forwarder)
set(FORWARDER_ELF ${CMAKE_CURRENT_BINARY_DIR}/forwarder.elf)
set(FORWARDER_NSO ${CMAKE_CURRENT_BINARY_DIR}/forwarder.nso)
set(FORWARDER_NPDM ${CMAKE_CURRENT_BINARY_DIR}/forwarder.npdm)

# Compile forwarder main.c
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/forwarder_main.o
    COMMAND ${CMAKE_C_COMPILER}
        -g -Wall -O2 -ffunction-sections
        -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
        -D__SWITCH__
        -I${LIBNX}/include
        -c ${FORWARDER_DIR}/main.c
        -o ${CMAKE_CURRENT_BINARY_DIR}/forwarder_main.o
    DEPENDS ${FORWARDER_DIR}/main.c
    COMMENT "Compiling forwarder main.c"
)

# Compile forwarder trampoline.s
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/forwarder_trampoline.o
    COMMAND ${CMAKE_ASM_COMPILER}
        -g -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
        -c ${FORWARDER_DIR}/trampoline.s
        -o ${CMAKE_CURRENT_BINARY_DIR}/forwarder_trampoline.o
    DEPENDS ${FORWARDER_DIR}/trampoline.s
    COMMENT "Compiling forwarder trampoline.s"
)

# Link forwarder ELF
add_custom_command(
    OUTPUT ${FORWARDER_ELF}
    COMMAND ${CMAKE_C_COMPILER}
        -specs=${DEVKITPRO}/libnx/switch.specs
        -g -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
        -Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/forwarder.map
        ${CMAKE_CURRENT_BINARY_DIR}/forwarder_main.o
        ${CMAKE_CURRENT_BINARY_DIR}/forwarder_trampoline.o
        -L${LIBNX}/lib -lnx
        -o ${FORWARDER_ELF}
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/forwarder_main.o
        ${CMAKE_CURRENT_BINARY_DIR}/forwarder_trampoline.o
    COMMENT "Linking forwarder ELF"
)

# Convert ELF to NSO
add_custom_command(
    OUTPUT ${FORWARDER_NSO}
    COMMAND ${ELF2NSO_EXECUTABLE} ${FORWARDER_ELF} ${FORWARDER_NSO}
    DEPENDS ${FORWARDER_ELF}
    COMMENT "Creating forwarder NSO"
)

# Generate NPDM from JSON
add_custom_command(
    OUTPUT ${FORWARDER_NPDM}
    COMMAND ${NPDMTOOL_EXECUTABLE} ${FORWARDER_DIR}/forwarder.json ${FORWARDER_NPDM}
    DEPENDS ${FORWARDER_DIR}/forwarder.json
    COMMENT "Creating forwarder NPDM"
)

# Copy forwarder binaries to res/ directory for bundling into romfs
add_custom_command(
    OUTPUT ${ROMFS_DIR}/forwarder.nso ${ROMFS_DIR}/forwarder.npdm
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARDER_NSO} ${ROMFS_DIR}/forwarder.nso
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FORWARDER_NPDM} ${ROMFS_DIR}/forwarder.npdm
    DEPENDS ${FORWARDER_NSO} ${FORWARDER_NPDM}
    COMMENT "Copying forwarder binaries to romfs directory"
)

# Forwarder build target
add_custom_target(forwarder_build ALL
    DEPENDS ${ROMFS_DIR}/forwarder.nso ${ROMFS_DIR}/forwarder.npdm
)

message(STATUS "Forwarder will be built from source: ${FORWARDER_DIR}")

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

# Get all source files, excluding the forwarder directory (built separately)
file(GLOB_RECURSE PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*/forwarder/.*")

#---------------------------------------------------------------------------------
# Executable Target
#---------------------------------------------------------------------------------
add_executable(${PROJECT_NAME}.elf
    ${IMGUI_SOURCES}
    ${PROJECT_SOURCES}
    ${libnsbmp_SOURCE_DIR}/src/libnsbmp.c
)

# Ensure dependencies are built before linking
add_dependencies(${PROJECT_NAME}.elf nspmini_build forwarder_build)

set_source_files_properties(${libnsbmp_SOURCE_DIR}/src/libnsbmp.c PROPERTIES LANGUAGE C)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    # Version
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_MICRO=${PROJECT_VERSION_PATCH}
    VERSION_STRING="${APP_VERSION}"
    # GitHub repository for update checks (injected at build time)
    GITHUB_REPO_OWNER="${GITHUB_REPO_OWNER}"
    GITHUB_REPO_NAME="${GITHUB_REPO_NAME}"
    # 3rd party dependency versions (from dependencies.json)
    DEP_IMGUI_VERSION="${IMGUI_VERSION}"
    DEP_LIBUSBHSFS_VERSION="${USBHSFS_VERSION}"
    DEP_LIBNSBMP_COMMIT="${LIBNSBMP_COMMIT_SHORT}"
    DEP_STB_COMMIT="${STB_COMMIT_SHORT}"
    DEP_NSPMINI_COMMIT="${NSPMINI_COMMIT_SHORT}"
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
    ${nspmini_SOURCE_DIR}/include
    ${PORTLIBS}/include
    ${LIBNX}/include
)

target_link_directories(${PROJECT_NAME}.elf PRIVATE
    ${usbhsfs_release_SOURCE_DIR}/lib
    ${nspmini_SOURCE_DIR}/lib
    ${PORTLIBS}/lib
    ${LIBNX}/lib
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE
    ${FREETYPE_LIBS}
    ${CURL_LIBS}
    gif turbojpeg jpeg png webp jansson
    glad EGL glapi drm_nouveau
    usbhsfs lwext4 ntfs-3g
    nsp
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
    COMMAND ${NACPTOOL_EXECUTABLE} --create "${APP_TITLE}" "${APP_AUTHOR}" "${APP_VERSION}" ${NACP_FILE} --videocapture=2
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

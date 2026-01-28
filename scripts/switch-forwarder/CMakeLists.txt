cmake_minimum_required(VERSION 3.13)

# This builds the NSP forwarder - a small executable that gets installed
# to the home menu and launches NX-Shell.nro from the SD card.

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif()

set(DEVKITPRO $ENV{DEVKITPRO})
include(${DEVKITPRO}/cmake/Switch.cmake REQUIRED)

set(ARCH "-march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE")
set(CMAKE_C_FLAGS "${ARCH} -g -Wall -O2 -ffunction-sections")
set(CMAKE_ASM_FLAGS "-g ${ARCH}")
set(CMAKE_EXE_LINKER_FLAGS "-L${DEVKITPRO}/libnx/lib -specs=${DEVKITPRO}/libnx/switch.specs -g ${ARCH}")
enable_language(ASM)

#---------------------------------------------------------------------------------
# hacBrewPack - fetch and build for host system (NSP creation tool)
#---------------------------------------------------------------------------------
include(FetchContent)

# Always fetch and build hacBrewPack (version from dependencies.json)
FetchContent_Declare(hacbrewpack
    GIT_REPOSITORY https://github.com/gayhearts/hacBrewPack.git
    GIT_TAG        ${HACBREWPACK_COMMIT}
)
FetchContent_MakeAvailable(hacbrewpack)

# Apply patch to add --nostartupuser support
set(HACBREWPACK_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/hacbrewpack-nostartupuser.patch")
set(HACBREWPACK_PATCH_MARKER "${hacbrewpack_SOURCE_DIR}/.nostartupuser_patched")
if(EXISTS ${HACBREWPACK_PATCH} AND NOT EXISTS ${HACBREWPACK_PATCH_MARKER})
    message(STATUS "Applying hacbrewpack --nostartupuser patch...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy "${hacbrewpack_SOURCE_DIR}/main.c" "${hacbrewpack_SOURCE_DIR}/main.c.orig"
        WORKING_DIRECTORY ${hacbrewpack_SOURCE_DIR}
    )
    execute_process(
        COMMAND git apply --ignore-whitespace "${HACBREWPACK_PATCH}"
        WORKING_DIRECTORY ${hacbrewpack_SOURCE_DIR}
        RESULT_VARIABLE PATCH_RESULT
    )
    if(PATCH_RESULT EQUAL 0)
        file(WRITE ${HACBREWPACK_PATCH_MARKER} "Patched with --nostartupuser support")
        message(STATUS "hacbrewpack patch applied successfully")
    else()
        message(WARNING "Failed to apply hacbrewpack patch (may already be applied)")
    endif()
endif()

# Build hacBrewPack using host compiler (not cross-compiler)
set(HACBREWPACK_BUILD_DIR "${hacbrewpack_SOURCE_DIR}")
set(HACBREWPACK_EXE "${HACBREWPACK_BUILD_DIR}/hacbrewpack")

# Copy config.mk required by hacBrewPack's Makefile
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/hacbrewpack-config.mk"
    "${HACBREWPACK_BUILD_DIR}/config.mk"
    COPYONLY
)

add_custom_command(
    OUTPUT ${HACBREWPACK_EXE}
    COMMAND make
    WORKING_DIRECTORY ${HACBREWPACK_BUILD_DIR}
    COMMENT "Building hacbrewpack for host system"
)
add_custom_target(hacbrewpack_build DEPENDS ${HACBREWPACK_EXE})

project(nsp_forwarder)

# Unique Title ID for NX-Shell forwarder
set(PROJECT_TITLEID "010ff000ffff0002")

# Icon file for the forwarder (use main project's icon)
set(FORWARDER_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../../icon.jpg")

set(FORWARDER_SRC 
    "${CMAKE_CURRENT_SOURCE_DIR}/source/main.c" 
    "${CMAKE_CURRENT_SOURCE_DIR}/source/trampoline.s"
)

add_executable(${PROJECT_NAME} ${FORWARDER_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE ${DEVKITPRO}/libnx/include)
target_link_libraries(${PROJECT_NAME} PRIVATE nx)
target_compile_definitions(${PROJECT_NAME} PRIVATE VERSION="${APP_VERSION}")

# Download prod.keys if not provided (needed for NSP creation)
if(NOT PROJECT_KEYSET)
    set(PROJECT_KEYSET "${CMAKE_CURRENT_BINARY_DIR}/prod.keys")
    if(NOT EXISTS ${PROJECT_KEYSET})
        file(DOWNLOAD https://raw.githubusercontent.com/externalEmu/s/master/prod.keys ${PROJECT_KEYSET})
    endif()
endif()

# Output file paths
set(EXEFS_MAIN "${CMAKE_CURRENT_BINARY_DIR}/exefs/main")
set(EXEFS_NPDM "${CMAKE_CURRENT_BINARY_DIR}/exefs/main.npdm")
set(NSP_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.nsp")

# Create PFS0 (ExeFS) - main NSO
add_custom_command(
    OUTPUT ${EXEFS_MAIN}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/exefs"
    COMMAND ${NX_ELF2NSO_EXE} "$<TARGET_FILE:${PROJECT_NAME}>" "${EXEFS_MAIN}"
    DEPENDS ${PROJECT_NAME}
    COMMENT "Creating ExeFS main"
)

# Create PFS0 (ExeFS) - NPDM
add_custom_command(
    OUTPUT ${EXEFS_NPDM}
    COMMAND ${NX_NPDMTOOL_EXE} "${CMAKE_CURRENT_SOURCE_DIR}/nxshell.json" "${EXEFS_NPDM}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/nxshell.json"
    COMMENT "Creating ExeFS NPDM"
)

# Create final NSP (trackable output file)
add_custom_command(
    OUTPUT ${NSP_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/control" "${CMAKE_CURRENT_BINARY_DIR}/logo"
    COMMAND ${NX_NACPTOOL_EXE} --create "NX-Shell" "${APP_AUTHOR}" "${APP_VERSION}" "${CMAKE_CURRENT_BINARY_DIR}/control/control.nacp"
    COMMAND ${CMAKE_COMMAND} -E copy "${FORWARDER_ICON}" "${CMAKE_CURRENT_BINARY_DIR}/control/icon_AmericanEnglish.dat"
    COMMAND ${CMAKE_COMMAND} -E copy "${FORWARDER_ICON}" "${CMAKE_CURRENT_BINARY_DIR}/logo/NintendoLogo.png"
    COMMAND ${HACBREWPACK_EXE} -k "${PROJECT_KEYSET}" --titleid ${PROJECT_TITLEID} --titlename "NX-Shell" --noromfs --nostartupuser
    COMMAND ${CMAKE_COMMAND} -E rename "${CMAKE_CURRENT_BINARY_DIR}/hacbrewpack_nsp/${PROJECT_TITLEID}.nsp" "${NSP_OUTPUT}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${EXEFS_MAIN} ${EXEFS_NPDM} ${HACBREWPACK_EXE} ${FORWARDER_ICON}
    COMMENT "Creating NSP forwarder"
)

# Target to build the NSP
add_custom_target(${PROJECT_NAME}.nsp ALL DEPENDS ${NSP_OUTPUT})

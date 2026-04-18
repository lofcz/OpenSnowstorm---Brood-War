cmake_minimum_required(VERSION 3.10)
project(OpenSnowstorm CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Common Compile Options & Strictness
# ---------------------------------------------------------------------------
function(opensnowstorm_apply_common_compile_options target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE 
            /bigobj 
            /W4       # Warning level 4
            /MP       # Multi-processor build
            /utf-8    # UTF-8 source/execution charset
        )
    else()
        target_compile_options(${target_name} PRIVATE 
            -Wall 
            -Wextra 
            -Wno-unused-parameter
            -fvisibility=hidden
        )
    endif()
endfunction()

if(MSVC)
    add_compile_options(/bigobj)
endif()

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(OPENSNOWSTORM_BUILD_UI
    "Build the SDL2 UI library and gfxtest executable (requires SDL2)" ON)

option(OPENSNOWSTORM_BUILD_BWAPI
    "Build the mini-openbwapi BWAPI compatibility wrapper" ON)

enable_testing()
add_test(NAME DeterminismCheck COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} python scripts/regression_check.py)

# ---------------------------------------------------------------------------
# SDL2 UI + gfxtest executable
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_UI)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_mixer REQUIRED)

    add_definitions(-DOPENBW_NO_SDL_IMAGE)
    add_definitions(-DOPENBW_ENABLE_UI)

    # Rendering backend selection (SDL2 vs Headless)
    if(1 OR SDL2_FOUND)
        add_library(openbw_ui ui/sdl2.cpp)
        target_link_libraries(openbw_ui "${SDL2_LIBRARY}" "${SDL2_MIXER_LIBRARY}")
        target_include_directories(openbw_ui PUBLIC "${SDL2_INCLUDE_DIR}" "${SDL2_MIXER_INCLUDE_DIR}")
        add_definitions(-DOPENBW_ENABLE_SDL)
    else()
        add_library(openbw_ui ui/headless_backend.cpp)
        add_definitions(-DOPENBW_ENABLE_HEADLESS)
    endif()

    target_include_directories(openbw_ui PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
    )
    opensnowstorm_apply_common_compile_options(openbw_ui)
    set_property(TARGET openbw_ui PROPERTY POSITION_INDEPENDENT_CODE ON)

    # Main replay-viewer / benchmark / validate-replay executable
    add_executable(gfxtest ui/gfxtest.cpp)
    target_include_directories(gfxtest PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_MIXER_INCLUDE_DIRS}
    )
    target_link_libraries(gfxtest openbw_ui ${SDL2_LIBRARIES} ${SDL2_MIXER_LIBRARIES})
    opensnowstorm_apply_common_compile_options(gfxtest)
endif()

# ---------------------------------------------------------------------------
# mini-openbwapi (BWAPI-compatible C wrapper)
# ---------------------------------------------------------------------------
if(OPENSNOWSTORM_BUILD_BWAPI)
    add_subdirectory(mini-openbwapi)
endif()

add_executable(test_ui test_ui.cpp)


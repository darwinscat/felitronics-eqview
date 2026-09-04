cmake_minimum_required(VERSION 3.22)
project(felitronics_eqview VERSION 0.1.0 LANGUAGES C CXX)   # C: juce_graphics compiles Sheenbidi (.c) in the JUCE test tier

# ----------------------------------------------------------------------------
# felitronics-eqview — the EQ VIEW of the Darwin's Cat family: the log-frequency plot every product
# ends up drawing (a spectrum behind it, response curves on it, handles you drag), as reusable
# pieces rather than as four copies of the same code with four copies of the same bugs.
#
# Header-only. The interface target adds an include path and nothing else.
#
# TWO RULES, both deliberate:
#   • it depends on felitronics-core and NOTHING else — no appkit, no product. The look is a Theme
#     parameter, so a product's own palette is not a fight with the library;
#   • it NEVER fetches its own dependencies. The CONSUMER resolves versions (one-version policy):
#     make felitronics-core available FIRST, then this. The only exception is a top-level build —
#     its own tests and CI — where there is nobody else to do it.
#
# Consume (pinned by tag, like everything else in the family):
#   include(FetchContent)
#   FetchContent_MakeAvailable(felitronics_core)          # core FIRST — see the one-version policy
#   FetchContent_Declare(felitronics_eqview GIT_REPOSITORY <url> GIT_TAG <tag>)
#   FetchContent_MakeAvailable(felitronics_eqview)
#   target_link_libraries(my_plugin PRIVATE felitronics::eqview)
# ----------------------------------------------------------------------------

add_library(felitronics_eqview INTERFACE)
add_library(felitronics::eqview ALIAS felitronics_eqview)
target_include_directories(felitronics_eqview INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(felitronics_eqview INTERFACE cxx_std_20)

# --- the dependency, resolved by whoever owns the graph --------------------------------------------
# Top-level: this IS the owner, so it resolves core itself — a sibling checkout when one is there
# (local development), the pinned tag otherwise (which is what CI gets: it checks out this repo
# alone, so a green CI build proves the PIN, not somebody's working copy).
if(PROJECT_IS_TOP_LEVEL)
    include(FetchContent)
    set(EQVIEW_FCORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../felitronics-core" CACHE PATH
        "Local felitronics-core checkout (used if present); else the pinned release is fetched")
    set(EQVIEW_FCORE_TAG "v0.25.0" CACHE STRING
        "felitronics-core git tag to fetch when no local checkout is present")

    if(EQVIEW_FCORE_DIR AND EXISTS "${EQVIEW_FCORE_DIR}/CMakeLists.txt")
        message(STATUS "felitronics-core: local checkout — ${EQVIEW_FCORE_DIR}")
        FetchContent_Declare(felitronics_core SOURCE_DIR "${EQVIEW_FCORE_DIR}")
    else()
        message(STATUS "felitronics-core: fetching ${EQVIEW_FCORE_TAG} from GitHub")
        FetchContent_Declare(felitronics_core
            GIT_REPOSITORY https://github.com/darwinscat/felitronics-core.git
            GIT_TAG        ${EQVIEW_FCORE_TAG}
            GIT_SHALLOW    TRUE)
    endif()
    set(FELITRONICS_BUILD_TESTS OFF CACHE BOOL "" FORCE)   # core's own suites are core's business
    FetchContent_MakeAvailable(felitronics_core)
endif()

# Consumed, not top-level: the targets must already be there. This is the one-version policy with
# teeth — a nested fetch here is how a project ends up with two copies of core at two versions.
if(NOT TARGET felitronics::eq OR NOT TARGET felitronics::analysis)
    message(FATAL_ERROR
        "felitronics-eqview needs felitronics::eq and felitronics::analysis, and it does not fetch "
        "them itself. Make felitronics-core available BEFORE adding eqview (FetchContent_MakeAvailable "
        "for core first), so ONE version of core is resolved by the product that owns the graph.")
endif()

target_link_libraries(felitronics_eqview INTERFACE felitronics::eq felitronics::analysis)

# --- tests (top-level only: a consumer's FetchContent does not inherit them) ------------------------
option(FELITRONICS_EQVIEW_BUILD_TESTS "Build felitronics-eqview self-tests" "${PROJECT_IS_TOP_LEVEL}")

# Most of this library is JUCE-free and its suites run anywhere. The drawing layer (PlotSurface.h) is
# not: it needs juce_graphics, which the CONSUMER supplies. Its gate is a compile check rather than a
# unit — pixels are judged by eye against a product's own screens — and it is OFF by default so a bare
# `cmake && ctest` stays offline. CI turns it on (and fetches JUCE); point FELITRONICS_EQVIEW_JUCE_SRC
# at a local checkout to skip the network.
option(FELITRONICS_EQVIEW_TESTS_WITH_JUCE "Also compile the JUCE-facing headers against a real JUCE" OFF)
set(FELITRONICS_EQVIEW_JUCE_SRC "" CACHE PATH "Optional path to a local JUCE source tree (skips the network fetch)")
if(NOT FELITRONICS_EQVIEW_BUILD_TESTS)
    return()
endif()

enable_testing()

# The same downstream-grade strict set the family gates its headers with: these compile inside a
# JUCE product under juce_recommended_warning_flags, so warning-cleanliness is gated HERE.
set(EQVIEW_STRICT_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(EQVIEW_STRICT_FLAGS
        -Werror -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion -Wfloat-equal -Wshadow -Wcast-align
        -Wzero-as-null-pointer-constant -Wunreachable-code -Wswitch-enum -Wextra-semi
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wshorten-64-to-32;-Wimplicit-float-conversion;-Wconditional-uninitialized;-Wconstant-conversion;-Wbool-conversion>)
endif()

# Theory-first, like the rest of the family: the expectations come from what the thing IS (the ruler
# is the set k*10^n; a decade is a constant width on a log axis; a matched design is exact at its
# centre), not from what the code currently returns.
foreach(unit PlotGrid HandleMath TraceSet)
    string(TOLOWER "eqview_${unit}_tests" _target)
    add_executable(${_target} tests/${unit}Tests.cpp)
    target_link_libraries(${_target} PRIVATE felitronics::eqview)
    target_compile_features(${_target} PRIVATE cxx_std_20)
    target_compile_options(${_target} PRIVATE ${EQVIEW_STRICT_FLAGS})
    add_test(NAME ${_target} COMMAND ${_target})
endforeach()

# --- the JUCE tier: the drawing layer, through a real consumer's flags ------------------------------
if(FELITRONICS_EQVIEW_TESTS_WITH_JUCE)
    include(FetchContent)
    if(FELITRONICS_EQVIEW_JUCE_SRC)
        FetchContent_Declare(juce SOURCE_DIR "${FELITRONICS_EQVIEW_JUCE_SRC}")
    else()
        FetchContent_Declare(juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        8.0.14)   # the family's pinned JUCE
    endif()
    FetchContent_MakeAvailable(juce)

    add_executable(eqview_juce_compile_check tests/JuceConsumerCompileCheck.cpp)
    target_link_libraries(eqview_juce_compile_check PRIVATE
        felitronics::eqview
        juce::juce_graphics
        juce::juce_recommended_warning_flags)
    target_compile_definitions(eqview_juce_compile_check PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)
    target_compile_features(eqview_juce_compile_check PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(eqview_juce_compile_check PRIVATE -Werror)   # the recommended flags must be CLEAN
    endif()
    add_test(NAME eqview_juce_compile_check COMMAND eqview_juce_compile_check)
endif()

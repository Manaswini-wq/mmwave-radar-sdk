cmake_minimum_required(VERSION 3.16)
project(mmwave_radar_sdk VERSION 2.4.0 LANGUAGES C ASM)

# Platform selection
set(PLATFORM "awr2944" CACHE STRING "Target platform: awr2944, iwr6843, awr1843")
set_property(CACHE PLATFORM PROPERTY STRINGS awr2944 iwr6843 awr1843)

string(TOUPPER ${PLATFORM} PLATFORM_UPPER)
add_compile_definitions(PLATFORM_${PLATFORM_UPPER})
add_compile_definitions(SDK_VERSION="${PROJECT_VERSION}")

# Common include paths
set(SDK_INCLUDES
    ${CMAKE_SOURCE_DIR}/sdk/include
    ${CMAKE_SOURCE_DIR}/sdk/hal
    ${CMAKE_SOURCE_DIR}/sdk/middleware
    ${CMAKE_SOURCE_DIR}/sdk/app_api
    ${CMAKE_SOURCE_DIR}/sdk/dsp
    ${CMAKE_SOURCE_DIR}/sdk/error
    ${CMAKE_SOURCE_DIR}/platform/common
    ${CMAKE_SOURCE_DIR}/platform/${PLATFORM}
)

# SDK source files
file(GLOB HAL_SRCS     "sdk/hal/*.c")
file(GLOB MW_SRCS      "sdk/middleware/*.c")
file(GLOB API_SRCS     "sdk/app_api/*.c")
file(GLOB DSP_SRCS     "sdk/dsp/*.c")
file(GLOB ERR_SRCS     "sdk/error/*.c")
file(GLOB PLAT_SRCS    "platform/${PLATFORM}/*.c")

# SDK static library
add_library(mmwave_sdk STATIC
    ${HAL_SRCS} ${MW_SRCS} ${API_SRCS} ${DSP_SRCS} ${ERR_SRCS} ${PLAT_SRCS}
)
target_include_directories(mmwave_sdk PUBLIC ${SDK_INCLUDES})
target_compile_options(mmwave_sdk PRIVATE -Wall -Wextra -Werror -std=c11)

# Examples
option(BUILD_EXAMPLES "Build example applications" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples/basic_radar)
endif()

# Unit tests (host build with GCC)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SRCS "tests/unit/*.c")
    add_executable(sdk_unit_tests ${TEST_SRCS})
    target_link_libraries(sdk_unit_tests PRIVATE mmwave_sdk m)
    target_compile_definitions(sdk_unit_tests PRIVATE UNIT_TEST)
    add_test(NAME unit_tests COMMAND sdk_unit_tests)
endif()

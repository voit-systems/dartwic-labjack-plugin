cmake_minimum_required(VERSION 3.30)
project(labjack_t7)

set(CMAKE_CXX_STANDARD 20)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif()

set(ENGINE_DIR "${PROJECT_SOURCE_DIR}/engine")
set(PLUGIN_MANIFEST_FILE "${PROJECT_SOURCE_DIR}/plugin.json")
set(BUILD_CONFIGURATION_FILE "${PROJECT_SOURCE_DIR}/build-configuration.json")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${PLUGIN_MANIFEST_FILE}"
        "${BUILD_CONFIGURATION_FILE}")

file(READ "${PLUGIN_MANIFEST_FILE}" PLUGIN_MANIFEST_JSON)
string(JSON ENGINE_PLUGIN_ID GET "${PLUGIN_MANIFEST_JSON}" id)
string(JSON ENGINE_PLUGINS_API_VERSION GET "${PLUGIN_MANIFEST_JSON}" engine_plugins_api_version)
string(JSON CONTAINS_ENGINE_PLUGIN GET "${PLUGIN_MANIFEST_JSON}" contains_engine_plugin)
if(NOT ENGINE_PLUGINS_API_VERSION MATCHES "^[0-9]+$")
    message(FATAL_ERROR "plugin.json engine_plugins_api_version must be an integer.")
endif()
if(NOT CONTAINS_ENGINE_PLUGIN)
    message(FATAL_ERROR "plugin.json must set contains_engine_plugin to true.")
endif()

file(READ "${BUILD_CONFIGURATION_FILE}" BUILD_CONFIGURATION_JSON)
string(JSON COPY_PLUGIN_VALUE ERROR_VARIABLE COPY_PLUGIN_ERROR GET "${BUILD_CONFIGURATION_JSON}" copy_plugin)
if(COPY_PLUGIN_ERROR)
    set(COPY_PLUGIN_VALUE true)
endif()

string(JSON EXTERNAL_ENGINE_ROOT ERROR_VARIABLE ENGINE_DIR_ERROR GET "${BUILD_CONFIGURATION_JSON}" engine_dir)
if(ENGINE_DIR_ERROR)
    set(EXTERNAL_ENGINE_ROOT "")
endif()

set(SHOULD_COPY_PLUGIN OFF)
if(COPY_PLUGIN_VALUE)
    set(SHOULD_COPY_PLUGIN ON)
endif()

set(ENGINE_PLUGIN_DIR "${PROJECT_SOURCE_DIR}/plugin/engine/${ENGINE_PLUGIN_ID}")

file(GLOB_RECURSE SRC_SOURCES "${ENGINE_DIR}/src/*.cpp")
add_library(labjack_t7 SHARED ${SRC_SOURCES})

file(GLOB ENGINE_INCLUDE_DIRS
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES true
        "${ENGINE_DIR}/include/*")

target_include_directories(labjack_t7 PRIVATE
        "${ENGINE_DIR}/include"
        "${ENGINE_DIR}/libs/concurrent_queue"
        "${ENGINE_DIR}/libs/LabJack/Drivers"
        ${ENGINE_INCLUDE_DIRS})

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(labjack_t7 PRIVATE
        nlohmann_json::nlohmann_json
        "${ENGINE_DIR}/libs/LabJack/Drivers/64bit/LabJackM.lib"
)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set_target_properties(labjack_t7 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${ENGINE_PLUGIN_DIR}/bin")

add_custom_command(TARGET labjack_t7 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ENGINE_PLUGIN_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PLUGIN_MANIFEST_FILE}"
        "${ENGINE_PLUGIN_DIR}/plugin.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ENGINE_DIR}/src/module_config.json"
        "${ENGINE_PLUGIN_DIR}/module_config.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ENGINE_DIR}/src/default_parameters.json"
        "${ENGINE_PLUGIN_DIR}/default_parameters.json"
        COMMENT "Copying engine plugin metadata into plugin output"
)

if(SHOULD_COPY_PLUGIN AND NOT "${EXTERNAL_ENGINE_ROOT}" STREQUAL "")
    set(EXTERNAL_ENGINE_PLUGIN_DIR "${EXTERNAL_ENGINE_ROOT}/plugins/${ENGINE_PLUGIN_ID}")
    add_custom_command(TARGET labjack_t7 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${EXTERNAL_ENGINE_ROOT}/plugins"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${EXTERNAL_ENGINE_PLUGIN_DIR}/bin"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ENGINE_PLUGIN_DIR}/plugin.json" "${EXTERNAL_ENGINE_PLUGIN_DIR}/plugin.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ENGINE_PLUGIN_DIR}/module_config.json" "${EXTERNAL_ENGINE_PLUGIN_DIR}/module_config.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ENGINE_PLUGIN_DIR}/default_parameters.json" "${EXTERNAL_ENGINE_PLUGIN_DIR}/default_parameters.json"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:labjack_t7>" "${EXTERNAL_ENGINE_PLUGIN_DIR}/bin/$<TARGET_FILE_NAME:labjack_t7>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:labjack_t7>" "${EXTERNAL_ENGINE_PLUGIN_DIR}/bin/$<TARGET_PDB_FILE_NAME:labjack_t7>"
            COMMENT "Copying engine plugin to external DARTWIC engine plugins directory")
endif()

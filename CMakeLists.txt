cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

find_path(FFmpeg_INCLUDE_DIR NAMES libavformat/avformat.h)
find_library(FFmpeg_avformat_LIBRARY NAMES avformat)
find_library(FFmpeg_avcodec_LIBRARY NAMES avcodec)
find_library(FFmpeg_avutil_LIBRARY NAMES avutil)
if(NOT FFmpeg_INCLUDE_DIR OR NOT FFmpeg_avformat_LIBRARY OR NOT FFmpeg_avcodec_LIBRARY OR NOT FFmpeg_avutil_LIBRARY)
  message(FATAL_ERROR "FFmpeg (avformat/avcodec/avutil) not found in CMAKE_PREFIX_PATH")
endif()
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${FFmpeg_INCLUDE_DIR}")
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE "${FFmpeg_avformat_LIBRARY}" "${FFmpeg_avcodec_LIBRARY}" "${FFmpeg_avutil_LIBRARY}"
)

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/config-store.cpp
    src/config-store.hpp
    src/hearthis-output.cpp
    src/hearthis-output.hpp
    src/stream-controller.cpp
    src/stream-controller.hpp
    src/ui/hearthis-dock.cpp
    src/ui/hearthis-dock.hpp
    src/ui/settings-dialog.cpp
    src/ui/settings-dialog.hpp
    src/ui/volume-meter.cpp
    src/ui/volume-meter.hpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

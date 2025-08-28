CMAKE_MINIMUM_REQUIRED(VERSION 3.15)
PROJECT(loki_media_player)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")

FIND_PACKAGE(PkgConfig REQUIRED)

# FFmpeg, PortAudio
PKG_CHECK_MODULES(AVFORMAT REQUIRED libavformat)
PKG_CHECK_MODULES(AVCODEC  REQUIRED libavcodec)
PKG_CHECK_MODULES(AVUTIL   REQUIRED libavutil)
PKG_CHECK_MODULES(SWSCALE  REQUIRED libswscale)
PKG_CHECK_MODULES(SWRESAMPLE REQUIRED libswresample)
PKG_CHECK_MODULES(PORTAUDIO REQUIRED portaudio-2.0)

# CUDA
FIND_PACKAGE(CUDA REQUIRED)

# CUDA DRIVER
FIND_LIBRARY(CUDA_DRIVER_LIBRARY
    NAMES cuda 
    PATHS ${CUDA_TOOLKIT_ROOT_DIR}/lib64 
          ${CUDA_TOOLKIT_ROOT_DIR}/lib
          /usr/local/cuda/lib64
          /usr/local/cuda/lib
          /usr/lib/x86_64-linux-gnu/
    DOC "CUDA driver library"
)

SET(CUDA_LIBRARIES
        ${CUDA_LIBRARIES}
        ${CUDA_cudart_static_LIBRARY}
        ${CUDA_rt_LIBRARY}
        ${CUDA_DRIVER_LIBRARY}
)

# Add CUDA include directories
include_directories(${CUDA_INCLUDE_DIRS})

# Add CUDA driver API header path
include_directories(/usr/local/cuda/include)

# OpenGL, GLFW
FIND_PACKAGE(OpenGL REQUIRED)
FIND_PACKAGE(glfw3 REQUIRED)
FIND_PACKAGE(GLEW REQUIRED)

# Paho MQTT C/C++
SET(PAHO_MQTT_C_INCLUDE_DIRS "/usr/include")
SET(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3as paho-mqtt3c)
SET(PAHO_MQTT_CPP_LIBRARIES paho-mqttpp3)

INCLUDE_DIRECTORIES(
        ${PAHO_MQTT_C_INCLUDE_DIRS}
        "/usr/local/include"
)

ADD_DEFINITIONS(-DPAHO_MQTT_PP_IMPL)
ADD_DEFINITIONS(-DPAHO_MQTT_HEADER)
ADD_DEFINITIONS(-DOPENSSL_SUPPORTED=1)

FIND_PACKAGE(PahoMqttCpp QUIET)
SET(PAHO_MQTT_CPP_FOUND TRUE)

FIND_LIBRARY(PAHO_MQTT_CPP_LIB paho-mqttpp3)
FIND_LIBRARY(PAHO_MQTT_C_LIB paho-mqtt3a)

# IMGUI
SET(IMGUI_DIR ${CMAKE_SOURCE_DIR}/3rdparty/imgui)
# cpprest-http-client-sdk
SET(CPPREST_HTTP_CLIENT_SDK_DIR ${CMAKE_SOURCE_DIR}/3rdparty/cpprest-http-client-sdk)

SET(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
SET(BUILD_TESTS OFF CACHE BOOL "" FORCE)
SET(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)

ADD_SUBDIRECTORY(${CPPREST_HTTP_CLIENT_SDK_DIR})

# SOURCE
ADD_EXECUTABLE(loki_media_player
        src/main.cpp
        src/core/Application.cpp
        src/core/Utils.cpp
        src/core/MediaPlayer.cpp
        src/ui/ControlPanel.cpp
        src/ui/FileSelector.cpp
        src/ui/UIManager.cpp
        src/ui/OSDRenderer.cpp
        src/rendering/ShaderProgram.cpp
        src/rendering/RenderContext.cpp
        src/rendering/VideoFBO.cpp
        src/report/MqttReportSource.cpp
        src/report/HttpReportSource.cpp
        src/sensors/CsvSensorSource.cpp
        src/threads/AudioThread.cpp
        src/media/Decoder.cpp
        src/media/Encoder.cpp
        src/media/VideoRenderer.cpp
        src/media/FileVideoSource.cpp
        src/media/NetworkStreamVideoSource.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# INCLUDE
TARGET_INCLUDE_DIRECTORIES(loki_media_player PRIVATE
        src
        ${AVFORMAT_INCLUDE_DIRS}
        ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS}
        ${SWSCALE_INCLUDE_DIRS}
        ${SWRESAMPLE_INCLUDE_DIRS}
        ${PORTAUDIO_INCLUDE_DIRS}
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${PAHO_MQTT_CPP_INCLUDES}
        ${PAHO_MQTT_CPP_INCLUDE_DIRS}
        ${PAHO_MQTT_C_INCLUDE_DIRS}
)

# RPATH
SET(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/3rdparty/cpprest-http-client-sdk:$ORIGIN/3rdparty/cpprest-http-client-sdk/3rdparty/loki-secure-sdk/3rdparty/spdlog")
SET(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/3rdparty/cpprest-http-client-sdk:${CMAKE_BINARY_DIR}/3rdparty/cpprest-http-client-sdk/3rdparty/loki-secure-sdk/3rdparty/spdlog")
SET_TARGET_PROPERTIES(loki_media_player PROPERTIES
        INSTALL_RPATH "$ORIGIN:$ORIGIN/3rdparty/cpprest-http-client-sdk:$ORIGIN/3rdparty/cpprest-http-client-sdk/3rdparty/loki-secure-sdk/3rdparty/spdlog"
        BUILD_WITH_INSTALL_RPATH TRUE
        BUILD_RPATH "${CMAKE_BINARY_DIR}/3rdparty/cpprest-http-client-sdk:${CMAKE_BINARY_DIR}/3rdparty/cpprest-http-client-sdk/3rdparty/loki-secure-sdk/3rdparty/spdlog"
        INSTALL_RPATH_USE_LINK_PATH TRUE
)

# COMPILE
TARGET_COMPILE_OPTIONS(loki_media_player PRIVATE
        ${AVFORMAT_CFLAGS_OTHER}
        ${AVCODEC_CFLAGS_OTHER}
        ${AVUTIL_CFLAGS_OTHER}
        ${SWSCALE_CFLAGS_OTHER}
        ${SWRESAMPLE_CFLAGS_OTHER}
        ${PORTAUDIO_CFLAGS_OTHER}
)

# LINK
TARGET_LINK_LIBRARIES(loki_media_player
        ${AVFORMAT_LIBRARIES}
        ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES}
        ${SWSCALE_LIBRARIES}
        ${SWRESAMPLE_LIBRARIES}
        ${PORTAUDIO_LIBRARIES}
        OpenGL::GL
        glfw
        GLEW::GLEW
        cpprest-http-client-sdk
        ${PAHO_MQTT_CPP_LIB}
        ${PAHO_MQTT_C_LIB}
        ${CUDA_LIBRARIES}
        ssl
        crypto
        pthread
        dl
        rt
        stdc++fs
)

TARGET_LINK_OPTIONS(loki_media_player PRIVATE
        ${AVFORMAT_LDFLAGS_OTHER}
        ${AVCODEC_LDFLAGS_OTHER}
        ${AVUTIL_LDFLAGS_OTHER}
        ${SWSCALE_LDFLAGS_OTHER}
        ${SWRESAMPLE_LDFLAGS_OTHER}
        ${PORTAUDIO_LDFLAGS_OTHER}
)
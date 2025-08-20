CMAKE_MINIMUM_REQUIRED(VERSION 3.14)
PROJECT(loki_media_player)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

FIND_PACKAGE(PkgConfig REQUIRED)

# FFmpeg, PortAudio
PKG_CHECK_MODULES(AVFORMAT REQUIRED libavformat)
PKG_CHECK_MODULES(AVCODEC  REQUIRED libavcodec)
PKG_CHECK_MODULES(AVUTIL   REQUIRED libavutil)
PKG_CHECK_MODULES(SWSCALE  REQUIRED libswscale)
PKG_CHECK_MODULES(SWRESAMPLE REQUIRED libswresample)
PKG_CHECK_MODULES(PORTAUDIO REQUIRED portaudio-2.0)

# OpenGL, GLFW
FIND_PACKAGE(OpenGL REQUIRED)
FIND_PACKAGE(glfw3 REQUIRED)
FIND_PACKAGE(GLEW REQUIRED)

# IMGUI
SET(IMGUI_DIR ${CMAKE_SOURCE_DIR}/3rdparty/imgui)

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
        src/threads/AudioThread.cpp
        src/media/Decoder.cpp
        src/media/VideoRenderer.cpp
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

TARGET_LINK_OPTIONS(loki_media_player PRIVATE
        ${AVFORMAT_LDFLAGS_OTHER}
        ${AVCODEC_LDFLAGS_OTHER}
        ${AVUTIL_LDFLAGS_OTHER}
        ${SWSCALE_LDFLAGS_OTHER}
        ${SWRESAMPLE_LDFLAGS_OTHER}
        ${PORTAUDIO_LDFLAGS_OTHER}
)
# loki-media-player

---

- Modern C++ multimedia player powered by FFmpeg and OpenGL
- Cross-platform video and audio playback with hardware acceleration
- Real-time audio/video synchronization with advanced timing control
- Designed for MVP

---

## Screenshot
![Screenshot1](/assets/Screenshot1.png)

---

## Features
- Multi-format Support**: Supports all major video/audio formats via FFmpeg (MP4, MOV, AVI, MKV, etc.)
- Hardware Acceleration**: OpenGL-based video rendering for smooth performance
- Real-time Synchronization**: Advanced audio/video sync with adaptive timing control
- Multi-threaded Architecture**: Separate threads for decoding, audio, and video rendering
- Memory Management**: Smart buffering with queue size limits to prevent memory overflow
- Cross-platform Audio**: PortAudio-based audio playback with callback-driven architecture
- Frame Rate Control**: Adaptive frame rate limiting to reduce CPU usage
- Thread-safe Queues**: Lock-free communication between decoder and players

---

## Getting Started

### Prerequisites
- Linux (Ubuntu 20.04 or later recommended)
- Requires CMake 3.14 or later
- Requires C++17 or later compiler
- [FFmpeg](https://ffmpeg.org/) development libraries
- [GLFW3](https://www.glfw.org/) for _window management
- [PortAudio](http://www.portaudio.com/) for cross-platform audio
- [OpenGL](https://www.opengl.org/) for hardware-accelerated rendering

---

## Build Instructions

### Setup and Installation
```bash
# Update package lists
sudo apt-get update

# Install dependencies
sudo apt install build-essential cmake pkg-config

# FFmpeg development libraries
sudo apt install libavformat-dev libavcodec-dev libswscale-dev libswresample-dev libavutil-dev

# Graphics and audio libraries
sudo apt install libglfw3-dev libgl1-mesa-dev portaudio19-dev

# Build the project
. build_project.sh
```

---

## Project Structure
```
loki-media-player/
├── assets/                                 # Resource files (csv, screenshot, media)
│   └── sensor_data.csv                     # Sample sensor data for simulation
├── src/            
│   ├── core/                               # Core application logic
│   │   ├── Application.cpp                 # Main application class and entry point
│   │   ├── Application.h                   # Application header with main loop
│   │   ├── MediaPlayer.cpp                 # Media playback core logic
│   │   ├── MediaPlayer.h                   # MediaPlayer interface and implementation
│   │   ├── MediaState.h                    # Playback state management
│   │   ├── Utils.cpp                       # Utility functions
│   │   └── Utils.h                         # Utility functions and macros
│   │
│   ├── media/                              # Media processing components
│   │   ├── AudioFrame.h                    # Audio frame data structure
│   │   ├── AudioPlayer.h                   # Audio playback interface
│   │   ├── CodecInfo.h                     # Media codec information
│   │   ├── Decoder.cpp                     # FFmpeg decoder implementation
│   │   ├── Decoder.h                       # FFmpeg decoder wrapper
│   │   ├── FileVideoSource.cpp             # File-based video source
│   │   ├── FileVideoSource.h               # File source interface
│   │   ├── NetworkStreamVideoSource.cpp    # Network stream source
│   │   ├── NetworkStreamVideoSource.h      # Network source interface
│   │   ├── SyncManager.h                   # Audio/video synchronization
│   │   ├── ThreadSafeQueue.h               # Thread-safe queue implementation
│   │   ├── VideoFrame.h                    # Video frame data structure
│   │   ├── VideoRenderer.cpp               # OpenGL video renderer
│   │   ├── VideoRenderer.h                 # Video rendering interface
│   │   ├── WebcamVideoSource.h             # Webcam source interface
│   │   └── interface/
│   │       └── IVideoSource.h              # Video source interface
│   │           
│   ├── rendering/                          # OpenGL rendering system
│   │   ├── RenderContext.cpp               # Rendering context management
│   │   ├── RenderContext.h                 # Context interface
│   │   ├── ShaderProgram.cpp               # Shader program management
│   │   ├── ShaderProgram.h                 # Shader program wrapper
│   │   ├── VideoFBO.cpp                    # Framebuffer object
│   │   └── VideoFBO.h                      # FBO interface
│   │           
│   ├── sensors/                            # Sensor data handling
│   │   ├── CsvSensorSource.cpp             # CSV-based sensor data source
│   │   ├── CsvSensorSource.h               # CSV sensor source interface
│   │   ├── SensorData.h                    # Sensor data structure
│   │   └── interface/
│   │       └── ISensorSource.h             # Base sensor source interface
│   │
│   ├── threads/                            # Multi-threading support
│   │   ├── AudioThread.cpp                 # Audio thread implementation
│   │   └── AudioThread.h                   # Audio thread management
│   │
│   └── ui/                                 # User interface components
│       ├── ControlPanel.cpp                # Media control panel
│       ├── ControlPanel.h                  # Control panel interface
│       ├── FileSelector.cpp                # File selection dialog
│       ├── FileSelector.h                  # File selector interface
│       ├── OSDRenderer.cpp                 # On-Screen Display renderer
│       ├── OSDRenderer.h                   # OSD rendering interface
│       ├── OSDState.h                      # OSD state management
│       ├── UIManager.cpp                   # UI management and rendering
│       └── UIManager.h                     # UI manager interface
│
├── gl_common.h                             # OpenGL common headers
└── main.cpp                                # Program entry point
```

---

### Thread Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                             Main Thread                          │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐  │
│  │  Media      │    │  Video Queue │    │  Video Renderer     │  │
│  │  Player     │───▶│  (Thread-    │───▶│  (OpenGL Context)   │  │
│  │  + Decoder  │    │   Safe)      │    │  + UI Rendering     │  │
│  └─────────────┘    └──────────────┘    └─────────────────────┘  │
│         │                   │                      ▲             │
│         │                   │                      │             │
│         ▼                   ▼                      │             │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐  │
│  │  Sensor     │    │  Sensor Data │    │  OSD Update         │  │
│  │  Manager    │───▶│  Queue       │───▶│  (Displays sensor   │  │
│  └─────────────┘    │  (Thread-    │    │   data in UI)       │  │
│                     │   Safe)      │    └─────────────────────┘  │
│                     └──────────────┘                             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
         │                   │
         │                   │
         ▼                   ▼
  ┌─────────────┐    ┌──────────────┐
  │  Sensor     │    │  Audio       │
  │  Thread     │    │  Thread      │
  │  (Reads     │    │  (Plays back │
  │   sensor    │    │   audio      │
  │   data)     │    │   frames)    │
  └─────────────┘    └──────────────┘
```
---

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

---
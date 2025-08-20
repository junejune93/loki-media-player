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
loki_media_player/
├── CMakeLists.txt              # CMake build configuration
├── src/                        # Source files
│   ├── core/                   # Core application logic
│   │   ├── Application.cpp     # Main application class
│   │   ├── Application.h       # Application header
│   │   ├── MediaPlayer.cpp     # Media playback core logic
│   │   ├── MediaPlayer.h       # MediaPlayer header
│   │   ├── MediaState.h        # Playback state management
│   │   ├── Utils.cpp           # Utility functions
│   │   └── Utils.h             # Utility header
│   ├── media/                  # Media processing
│   │   ├── AudioFrame.h        # Audio frame structure
│   │   ├── AudioPlayer.h       # Audio player interface
│   │   ├── Decoder.cpp         # FFmpeg decoder implementation
│   │   ├── Decoder.h           # FFmpeg decoder wrapper
│   │   ├── SyncManager.h       # Audio/video synchronization
│   │   ├── ThreadSafeQueue.h   # Thread-safe queue template
│   │   ├── VideoFrame.h        # Video frame structure
│   │   ├── VideoRenderer.cpp   # OpenGL video renderer implementation
│   │   └── VideoRenderer.h     # OpenGL video renderer
│   ├── rendering/              # OpenGL rendering system
│   │   ├── RenderContext.cpp   # Rendering context implementation
│   │   ├── RenderContext.h     # Rendering context
│   │   ├── ShaderProgram.cpp   # Shader management implementation
│   │   ├── ShaderProgram.h     # Shader program wrapper
│   │   ├── VideoFBO.cpp        # Framebuffer object implementation
│   │   └── VideoFBO.h          # Framebuffer object
│   ├── threads/                # Multi-threading support
│   │   ├── AudioThread.cpp     # Audio thread implementation
│   │   └── AudioThread.h       # Audio thread management
│   ├── ui/                     # User interface
│   │   ├── ControlPanel.cpp    # Media control panel implementation
│   │   ├── ControlPanel.h      # Control panel interface
│   │   ├── FileSelector.cpp    # File selection dialog implementation
│   │   ├── FileSelector.h      # File selector interface
│   │   ├── OSDRenderer.cpp     # OSD Rendering implementation
│   │   ├── OSDRenderer.h       # OSD Rendering
│   │   ├── OSDState.h          # OSD state management
│   │   ├── UIManager.cpp       # UI management implementation
│   │   └── UIManager.h         # UI manager
│   ├── gl_common.h             # OpenGL common headers
│   └── main.cpp                # Program entry point
└── README.md                   # Project documentation
```

---

### Thread Architecture
```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│    Main     │───▶│ Video Queue  │───▶│   Video     │
│   Thread    │    │              │    │  Renderer   │
│ (MediaPlayer│    ┌──────────────┐    │ (Main Thread│
│  + Decoder) │───▶│ Audio Queue  │    │   OpenGL)   │
└─────────────┘    └──────────────┘    └─────────────┘
                           │
                           ▼
                   ┌─────────────┐
                   │             │
                   │    Audio    │
                   │   Thread    │
                   │(AudioThread)│
                   │             │
                   └─────────────┘
```

---

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

---
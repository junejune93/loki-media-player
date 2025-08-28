# loki-media-player

---

- Modern C++ multimedia player powered by FFmpeg and OpenGL
- Cross-platform video and audio playback with hardware acceleration
- Real-time audio/video synchronization with advanced timing control
- Designed for end-to-end solution
- Designed for Docker-based execution (easy deployment and isolation)
- <span style="color:deepskyblue; font-weight:bold">Requires a sample media file (`assets/sample.mp4`) to be present for execution</span>
- Compatible with Ubuntu 22.04 or later  

---

## Screenshot
- loki Media Player
![Screenshot1](/assets/Screenshot1.png)
- loki Media Player OpenAPI Server Doc.
![Screenshot1](/assets/Screenshot2.png)

---

## Features
- Multi-format Support: Supports all major video/audio formats via <span style="color:deepskyblue; font-weight:bold">FFmpeg (MP4, MOV, AVI, MKV, etc.)</span>
- Hardware Acceleration: <span style="color:deepskyblue; font-weight:bold">OpenGL-based</span> video rendering for smooth performance
- Cross-platform Windowing: <span style="color:deepskyblue; font-weight:bold">GLFW + GLEW</span> for high-performance graphics context management
- UI/UX: ImGui-based UI with control panel, file selector, and <span style="color:deepskyblue; font-weight:bold">OSD</span> (on-screen display)
- Real-time Synchronization: Advanced audio/video sync with <span style="color:deepskyblue; font-weight:bold">adaptive timing control</span>
- Multi-threaded Architecture: Separate threads for <span style="color:deepskyblue; font-weight:bold">decoding, audio, and video rendering</span>
- Memory Management: Smart buffering with <span style="color:deepskyblue; font-weight:bold">queue size limits</span> to prevent memory overflow
- Cross-platform Audio: <span style="color:deepskyblue; font-weight:bold">PortAudio-based</span> audio playback with callback-driven architecture
- Frame Rate Control: <span style="color:deepskyblue; font-weight:bold">Adaptive frame rate limiting</span> to reduce CPU usage
- Thread-safe Queues: <span style="color:deepskyblue; font-weight:bold">Lock-free communication</span> between decoder and players
- Video Effects & Shaders: <span style="color:deepskyblue; font-weight:bold">Custom shader support</span> for video post-processing
- Sensor Integration: <span style="color:deepskyblue; font-weight:bold">CSV-based sensor data input</span> for adaptive behavior or analytics
- Playback Control: <span style="color:deepskyblue; font-weight:bold">SEEK functionality</span> with precise frame-level navigation and timeline
- H.264 Encoding & Storage: libx264-based H.264 stream encoding with frame timestamp synchronization verification, 3-minute interval file segmentation using std::filesystem for path/file management
- Screen Recording: <span style="color:deepskyblue; font-weight:bold">MP4-based recording</span> with real-time screen capture and compression
- Status Reporting: HTTP-based reporting with connection pool for real-time channel, sensor, synchronization monitoring <span style="color:deepskyblue; font-weight:bold">at 20-second intervals</span>
- Frame Markers: I-Frame/P-Frame visual indicators with color-coded timeline markers for GOP structure analysis
- Hardware Acceleration: CUDA-based hardware decoder support with NVDEC integration for accelerated H.264/H.265 decoding and GPU memory optimization

---

## Getting Started

### Prerequisites
- Linux (Ubuntu 22.04 or later)
- Requires CMake 3.15 or later
- Requires C++20
- [FFmpeg](https://ffmpeg.org/) development libraries:
  - libavformat
  - libavcodec
  - libavutil
  - libswscale
  - libswresample
- [GLFW3](https://www.glfw.org/) for window management
- [GLEW](http://glew.sourceforge.net/) for OpenGL extension handling
- [PortAudio](http://www.portaudio.com/) for cross-platform audio
- [OpenGL](https://www.opengl.org/) for hardware-accelerated rendering
- [ImGui](https://github.com/ocornut/imgui) for GUI rendering (included as third-party)
- [cpprest-http-client-sdk](https://github.com/loki2001-dev/cpprest-http-client-sdk) for HTTP Client SDK (included as third-party)

---

## Build Instructions

### Setup and Installation (OpenAPI Sever)
```bash
# Update package lists
sudo apt-get update

# Install dependencies
./installation_dependencies.sh

# Activate Service
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
sudo systemctl status mosquitto

# Activate the virtual environment
source venv/bin/activate

pip install paho-mqtt

# Run the server
python3 openapi/openapi_server.py

```

### Setup and Installation (Docker)
```bash
# Update package lists
sudo apt-get update

# Install Docker (Base)
./build_loki_media_player_docker_base.sh

# Install Docker (App)
./build_loki_media_player_docker.sh

# Excute Docker (App)
./docker-run-loki-media-player.sh
```

### Setup and Installation (Local)
```bash
# Update package lists
sudo apt-get update

# Install dependencies
./installation_dependencies.sh

# Build the project
./build_project.sh
```

---

## Project Structure
```
loki-media-player/
├── assets/                                 # Resource files (csv, screenshot, media)
│   └── sensor_data.csv                     # Sample sensor data for simulation
├── src/            
│   ├── openapi/                            # opsenapi server
│   │   └── openapi_server.py               # opsenapi server implementation
│   │
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
│   │   ├── Encoder.cpp                     # FFmpeg Encoder implementation
│   │   ├── Encoder.h                       # FFmpeg Encoder wrapper
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
│   ├── report/                             # Report system
│   │   ├── HttpReportSource.cpp            # Http Client source
│   │   ├── HttpReportSource.h              # Http Client interface
│   │   ├── MqttReportSource.cpp            # Mqtt Client source
│   │   ├── MqttReportSource.h              # Mqtt Client interface
│   │   └── interface/
│   │       └── IReportSource.h             # Report source interface
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
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                                      Main Thread                                            │
│     ┌─────────────┐    ┌─────────────────┐    ┌─────────────────────┐  ┌────────────────┐   │
│     │  Media      │    │   Video Queue   │    │  Video Renderer     │  │  UI Manager    │   │
│     │  Player     │───▶│  (Thread-Safe)  │───▶│  (OpenGL Context)   │  │  (ImGui)       │   │
│     │  + Decoder  │    │                 │    │  + Shader Effects   │  │  + Controls    │   │
│     └─────────────┘    └─────────────────┘    └──────────┬──────────┘  └────────┬───────┘   │
│            │                   │                         │                      │           │
│            │                   │                         │                      │           │
│            ▼                   ▼                         ▼                      │           │
│     ┌─────────────┐    ┌─────────────────┐    ┌─────────────────────┐           │           │
│     │  Sync       │    │  Audio Queue    │    │  OSD Renderer       │           │           │
│     │  Manager    │◀──▶│  (Thread-Safe)  │    │  (Frame Timing      │           │           │
│     └─────────────┘    │                 │    │   + Statistics)     │           │           │
│                        └─────────────────┘    └─────────────────────┘           │           │
│                                                                                 │           │
│                                       ┌─────────────────────────────────────────┘           │
│                                       │                                                     │
│                                       ▼                                                     │
│     ┌────────────────────────────────────────────────────────────────────────┐              │
│     │  State Management (Play/Pause/Seek)                                    │              │
│     └────────────────────────────────────────────────────────────────────────┘              │
│                                                                                             │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
                       │                   │                           │
                       │                   │                           │
                       ▼                   ▼                           ▼
                ┌─────────────┐    ┌──────────────┐    ┌─────────────────────────────┐
                │  Decoder    │    │  Audio       │    │  Recording Thread           │
                │  Thread     │    │  Thread      │    │  (Handles screen recording  │
                │  (FFmpeg    │    │  (PortAudio  │    │   and encoding)             │
                │   demuxing &│    │   Callback)  │    │                             │
                │   decoding) │    │              │    │                             │
                └─────────────┘    └──────────────┘    └─────────────────────────────┘
                       │                   │
                       │                   │
                       ▼                   ▼
                ┌─────────────┐    ┌─────────────────┐
                │  File/      │    │  Audio Device   │
                │  Network    │    │  (Hardware      │
                │  I/O        │    │   Interface)    │
                └─────────────┘    └─────────────────┘

Key Components:
1. Main Thread: Handles UI rendering, window events, and coordinates between components
2. Decoder Thread: FFmpeg-based demuxing and decoding of media streams
3. Audio Thread: Manages audio playback using PortAudio with callback-driven architecture
4. Video Rendering: OpenGL-based rendering with shader support for effects
5. Sync Manager: Ensures A/V synchronization using a master clock
6. Thread-Safe Queues: Enable lock-free communication between threads
7. Recording Thread: Optional thread for screen recording and encoding

Thread Communication:
- Video Frames: Decoder → Video Queue → Render Thread
- Audio Frames: Decoder → Audio Queue → Audio Thread
- State Changes: Main Thread → All Threads (via atomic variables)
- Seek Requests: Main Thread → Decoder Thread (with queue flushing)
```
---

## License
This project is licensed under the GNU LGPL v2.1 or later - see the [LICENSE](LICENSE) file for details.

---

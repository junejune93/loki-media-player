# loki-media-player

---

- Modern C++ multimedia player powered by FFmpeg and OpenGL
- Cross-platform video and audio playback with hardware acceleration
- Real-time audio/video synchronization with advanced timing control
- Designed for MVP

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
- [GLFW3](https://www.glfw.org/) for window management
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
├── CMakeLists.txt              # CMake build configuration
├── include/                    # Header files
│   ├── AudioFrame.h            # Audio frame structure
│   ├── VideoFrame.h            # Video frame structure
│   ├── AudioPlayer.h           # PortAudio-based audio player
│   ├── VideoRenderer.h         # OpenGL video renderer
│   ├── Decoder.h               # FFmpeg decoder wrapper
│   ├── SyncManager.h           # Audio/video synchronization
│   └── ThreadSafeQueue.h       # Thread-safe queue template
├── src/                        # Source files
│   ├── main.cpp                # Main application
│   ├── Decoder.cpp             # FFmpeg decoder implementation
│   └── VideoRenderer.cpp       # OpenGL rendering implementation
├── assets/                     # Test media files
│   └── tears_of_steel_1080p.mov
└── README.md
```

---

### Thread Architecture
```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│   Decoder   │───▶│ Video Queue  │───▶│   Video     │
│   Thread    │    │              │    │  Renderer   │
│             │    ┌──────────────┐    │   Thread    │
│             │───▶│ Audio Queue  │───▶│             │
└─────────────┘    └──────────────┘    └─────────────┘
                           │
                           ▼
                   ┌─────────────┐
                   │   Audio     │
                   │   Player    │
                   │   Thread    │
                   └─────────────┘
```

---

## Scrrenshot
![image1](/assets/Screenshot from 2025-08-18 23-46-48.png)

---

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

---

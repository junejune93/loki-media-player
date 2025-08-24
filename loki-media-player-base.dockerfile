# ==========================================
# Loki Media Player Base Image
# ==========================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git wget \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev \
    pulseaudio pulseaudio-utils \
    alsa-utils \
    portaudio19-dev \
    libglfw3-dev libglew-dev libgl1-mesa-dev libgl1-mesa-glx libglu1-mesa mesa-utils \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxrender1 \
    xorg-dev x11-apps \
    libpaho-mqtt-dev libpaho-mqttpp-dev \
    libssl-dev \
    libcrypto++-dev \
    libstdc++6 \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

CMD ["./build/loki_media_player"]
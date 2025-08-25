# ==========================================
# Loki Media Player Base Image
# ==========================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y \
    build-essential pkg-config git wget curl \
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
    libboost-all-dev \
    libcpprest-dev \
    mosquitto \
    mosquitto-clients \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null \
    && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list >/dev/null \
    && apt-get update \
    && apt-get install -y cmake \
    && rm -rf /var/lib/apt/lists/*

RUN cmake --version

WORKDIR /app

COPY . /app

RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

CMD ["./build/loki_media_player"]
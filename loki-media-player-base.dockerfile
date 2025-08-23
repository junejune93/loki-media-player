# ==========================================
# Loki Media Player Base Image
# ==========================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Seoul

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev \
    pulseaudio pulseaudio-utils \
    alsa-utils \
    portaudio19-dev \
    libglfw3-dev libglew-dev libgl1-mesa-dev libgl1-mesa-glx libglu1-mesa mesa-utils \
    libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxrender1 \
    xorg-dev x11-apps \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)

CMD ["./build/loki_media_player"]
#!/bin/bash

# ==========================================
# Installation dependencies for Loki Media Player
# ==========================================

echo "Updating package lists..."
sudo apt update

echo "Installing required packages..."
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    portaudio19-dev \
    libglew-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxrandr-dev \
    libxext-dev \
    libx11-dev \
    xorg-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    libssl-dev \
    libcrypto++-dev \
    git \
    wget \
    libstdc++6 \
    libc6-dev \
    mosquitto \
    mosquitto-clients \
    nvidia-cuda-toolkit \
    nvidia-cuda-dev \
    libcuda1 \
    cuda-driver-dev \
    cuda-runtime-11-8 \
    libcpprest-dev \
    libboost-all-dev \
    librt-dev \
    libdl-dev
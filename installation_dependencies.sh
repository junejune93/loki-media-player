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
    xorg-dev

echo "All dependencies installed successfully!"
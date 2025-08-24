# ==========================================
# Loki Media Player Application Image
# ==========================================
FROM loki-media-player-base:latest

RUN apt-get update && apt-get install -y \
    xvfb \
    x11-apps \
    mesa-utils \
    libgl1-mesa-glx \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN if [ ! -d "3rdparty/imgui" ]; then \
        echo "ImGui not found, cloning from repository..."; \
        mkdir -p 3rdparty && \
        cd 3rdparty && \
        git clone https://github.com/ocornut/imgui.git; \
    fi

RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

RUN mkdir -p /app/media /app/logs

RUN chmod +x build/loki_media_player

ENV DISPLAY=:0
ENV LIBGL_ALWAYS_INDIRECT=1

VOLUME ["/app/media", "/app/logs"]

WORKDIR /app/build

CMD ["./loki_media_player"]
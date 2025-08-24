#!/bin/bash

echo "=== Building loki-media-player application image ==="

sudo docker build -f loki-media-player-app.dockerfile -t loki-media-player:latest .

echo "=== Build complete: loki-media-player:latest ==="

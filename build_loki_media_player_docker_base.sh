#!/bin/bash

echo "=== Building loki-media-player-base image ==="

sudo docker build -f loki-media-player-base.dockerfile -t loki-media-player-base:latest .

echo "=== Build complete: loki-media-player-base:latest ==="
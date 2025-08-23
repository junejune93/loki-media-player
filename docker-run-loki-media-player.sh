#!/bin/bash

xhost +si:localuser:$(whoami)

PULSE_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}

MEDIA_DIR="/path/to/media"
LOGS_DIR="/path/to/logs"
IMAGE_NAME="loki-media-player:latest"

sudo docker run -it --rm \
    --net=host \
    --privileged \
    -e DISPLAY=$DISPLAY \
    -e LIBGL_ALWAYS_INDIRECT=0 \
    -e PULSE_SERVER=unix:$PULSE_RUNTIME_DIR/pulse/native \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v $PULSE_RUNTIME_DIR/pulse/native:$PULSE_RUNTIME_DIR/pulse/native \
    -v $MEDIA_DIR:/app/media \
    -v $LOGS_DIR:/app/logs \
    $IMAGE_NAME
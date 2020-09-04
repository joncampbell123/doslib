#!/bin/bash
# for 1000x720 video to squash for 320x200
ffmpeg -i "$1" -vf format=rgba,scale=width=334:height=200 -y "$1.%03d.png"

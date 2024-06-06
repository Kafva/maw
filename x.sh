#!/usr/bin/env bash
# convert current_cover -gravity east -chop 280x0 -gravity west -chop 280x0 cropped_cover


# https://video.stackexchange.com/a/4571
ffmpeg -y -i esc.m4a -an -filter:v "crop=w=720:h=720:x=280:y=0" output.png

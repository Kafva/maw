#!/usr/bin/env bash
# With imagemagick
# convert current_cover -gravity east -chop 280x0 -gravity west -chop 280x0 cropped_cover

# https://trac.ffmpeg.org/wiki/Map
# https://video.stackexchange.com/a/4571
# Extract cropped cover
#   ffmpeg -y -i esc.m4a -an -frames:v 1 -filter:v "crop=w=720:h=720:x=280:y=0" output.png

# Set cropped cover in-place
ffmpeg \
    -y \
    -i esc.m4a \
    -map 0:0 \
    -c:a copy \
    -map 0:1 \
    -filter:v "crop=w=720:h=720:x=280:y=0,format=rgb24" \
    -disposition:1 attached_pic \
    -frames:v 1 \
    -c:v png \
    output.m4a

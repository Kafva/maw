ffmpeg \
    -y \
    -f lavfi \
    -i anullsrc=duration=30 \
    -f lavfi \
    -i color=c=white:s=1280x720 \
    -map 0:a \
    -c:a aac \
    -map 1:v \
    -frames:v 1 \
    -c:v png \
    -disposition:v:0 attached_pic \
    output.m4a

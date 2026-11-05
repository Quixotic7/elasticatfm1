#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Records the RemoteFM1 app's window (screen 0, cropped to the window's pixel rect) with the Mac's audio from the
# BlackHole 2ch device, with ffmpeg's avfoundation input. Needs Screen Recording permission for the calling app.
#   record.sh OUT.mov X Y W H SECONDS      (X Y W H in screen pixels)
set -e
OUT=$1; X=$2; Y=$3; W=$4; H=$5; SECS=$6
date +%s.%N > "$OUT.start"
exec ffmpeg -y -loglevel warning -f avfoundation -framerate 30 -capture_cursor 0 -pixel_format uyvy422 \
    -i "1:BlackHole 2ch" -t "$SECS" \
    -vf "crop=$W:$H:$X:$Y" -c:v libx264 -preset ultrafast -crf 16 -pix_fmt yuv420p -c:a pcm_s16le "$OUT"

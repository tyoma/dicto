recorder | ffmpeg -f s16le -ar 44100 -ac 1 -i pipe: -q:a 9 -f segment -segment_time 3600 "%~n1-%%03d%~x1"

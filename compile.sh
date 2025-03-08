# On Ubuntu apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
g++ -Ofast ffmpeg.cpp -lavcodec -lavutil -lswscale -lavformat -o caffeine -I .
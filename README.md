## AudioDemo
用于演示在Android native下AudioTrack和AudioRecord的使用

* 播放pcm文件
audiodemo -m 0 -d 3 -c 1 -b 16 -r 44100 -t 10 /data/sine_44100_s16_mono.pcm

* 从内置MIC录制声音
audiodemo -m 1 -d 1 -c 2 -b 16 -r 44100 -t 10 /data/t1.pcm

* 从内置MIC录制声音并立即播放
audiodemo -m 2 -d 1,3 -c 1,2 -b 16,16 -r 44100,48000 -t 10


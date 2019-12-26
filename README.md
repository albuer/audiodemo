## AudioDemo
用于演示在Android native下AudioTrack和AudioRecord的使用

* 播放wav文件

  ```
  audiodemo --in=/sdcard/demo.wav
  ```

* 播放pcm文件

  ```
  audiodemo --in=/sdcard/sine_44100_s16_mono.pcm --in-channel=1 --in-rate=44100 --in-bits=16 
  ```

* 使用内置MIC录制声音并保存文件

  ```
  audiodemo --out=/sdcard/demo.pcm --duration=10
  ```

* 从内置MIC录制声音并立即播放

  ```
  audiodemo --duration=5
  ```

  


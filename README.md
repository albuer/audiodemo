## AudioDemo
������ʾ��Android native��AudioTrack��AudioRecord��ʹ��

* ����wav�ļ�

  ```
  audiodemo --in=/sdcard/demo.wav
  ```

* ����pcm�ļ�

  ```
  audiodemo --in=/sdcard/sine_44100_s16_mono.pcm --in-channel=1 --in-rate=44100 --in-bits=16 
  ```

* ʹ������MIC¼�������������ļ�

  ```
  audiodemo --out=/sdcard/demo.pcm --duration=10
  ```

* ������MIC¼����������������

  ```
  audiodemo --duration=5
  ```

 * �ز���

    ```
    audiodemo --in=96000_2ch.pcm --in-channel=2 --in-rate=96000 --out=44100_2ch.pcm --out-channel=2 --out-rate=44100 --resample
    ```

## AudioDemo
������ʾ��Android native��AudioTrack��AudioRecord��ʹ��

* ����pcm�ļ�
audiodemo -m 0 -d 3 -c 1 -b 16 -r 44100 -t 10 /data/sine_44100_s16_mono.pcm

* ������MIC¼������
audiodemo -m 1 -d 1 -c 2 -b 16 -r 44100 -t 10 /data/t1.pcm

* ������MIC¼����������������
audiodemo -m 2 -d 1,3 -c 1,2 -b 16,16 -r 44100,48000 -t 10


// Copyright 2008, The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>
#include <getopt.h>

#include <media/AudioSystem.h>
#include <media/AudioTrack.h>
#include <media/AudioRecord.h>

#include <audio_utils/resampler.h>

namespace android {

/************************************************************
*
*    Constructor
*
************************************************************/

audio_source_t      gInDevice = AUDIO_SOURCE_MIC;
audio_stream_type_t gOutDevice = AUDIO_STREAM_MUSIC;

#define         CHANNEL_NUM     2
#define         SAMPLE_RATE     44100
#define         SAMPLE_BITS     16
#define         SINE_FREQ       500
#define         SINE_TIME_SEC   60

int             gInChannelNum = -1;
int             gOutChannelNum = -1;

int             gInSampleRate = -1;
int             gOutSampleRate = -1;

int             gInBits = -1;
int             gOutBits = -1;

char            gInFile[512] = "";
char            gOutFile[512] = "";

bool            isPlaying = false;
bool            isRecording = false;

int             gResample = -1;
int             gSineFreq = -1;
int             gSineTime = SINE_TIME_SEC;

#undef RAMP_VOLUME

int CheckPlaybackParams()
{
    if (gOutDevice<AUDIO_STREAM_DEFAULT || gOutDevice>AUDIO_STREAM_PUBLIC_CNT)
        return -1;

    if (gOutChannelNum!=1 && gOutChannelNum!=2)
        return -2;

    if (gOutBits!=8 && gOutBits!=16 && gOutBits!=32)
        return -3;

    return 0;
}

int CheckRecordParams()
{
    if (gInDevice<AUDIO_SOURCE_DEFAULT || gInDevice>AUDIO_SOURCE_CNT)
        return -1;

    if (gInChannelNum!=1 && gInChannelNum!=2)
        return -2;

    if (gInBits!=8 && gInBits!=16 && gInBits!=32)
        return -3;

    return 0;
}

sp<AudioTrack> allocAudioTrack(void)
{
    if (gOutChannelNum < 0) gOutChannelNum = CHANNEL_NUM;
    if (gOutSampleRate < 0) gOutSampleRate = SAMPLE_RATE;
    if (gOutBits < 0) gOutBits = SAMPLE_BITS;

    if (CheckPlaybackParams() != 0) {
        printf("Invalid playback params!\n");
        return NULL;
    }

    int sampleRate = gOutSampleRate;
    size_t frameCount = 0;
    int channel = (gOutChannelNum<=1)?AUDIO_CHANNEL_OUT_MONO:AUDIO_CHANNEL_OUT_STEREO;
    audio_format_t aFormat = AUDIO_FORMAT_PCM_16_BIT;

    switch(gOutBits) {
    case 8:
        aFormat = AUDIO_FORMAT_PCM_8_BIT;
        break;
    case 16:
        aFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case 32:
        aFormat = AUDIO_FORMAT_PCM_32_BIT;
        break;
    }

    if (AudioTrack::getMinFrameCount(&frameCount, gOutDevice, sampleRate) != NO_ERROR) {
        fprintf(stderr, "cannot compute frame count\n");
        return NULL;
    }
    printf("for stream(%d): rate %d, channel %d, bits %d, frameCount %zu\n",
            gOutDevice, sampleRate, gOutChannelNum, gOutBits, frameCount);

    sp<AudioTrack> track = new AudioTrack();
    if (track->set(gOutDevice, sampleRate, aFormat,
            channel, frameCount, AUDIO_OUTPUT_FLAG_NONE, NULL,
            NULL, 0, 0, false, AUDIO_SESSION_ALLOCATE,
            AudioTrack::TRANSFER_OBTAIN) != NO_ERROR) {
        fprintf(stderr, "cannot initialize audio device\n");
        return NULL;
    }
    printf("alloc AudioTrack success. latency: %d ms\n", track->latency());

    return track;
}

sp<AudioRecord> allocAudioRecord(void)
{
    if (gInChannelNum < 0) gInChannelNum = CHANNEL_NUM;
    if (gInSampleRate < 0) gInSampleRate = SAMPLE_RATE;
    if (gInBits < 0) gInBits = SAMPLE_BITS;

    if (CheckRecordParams() != 0) {
        printf("Invalid record params!\n");
        return NULL;
    }

    int sampleRate = gInSampleRate;
    size_t frameCount = 0;
    audio_format_t aFormat = AUDIO_FORMAT_PCM_16_BIT;
    int channel = (gInChannelNum<=1)?AUDIO_CHANNEL_IN_MONO:AUDIO_CHANNEL_IN_STEREO;

    switch(gInBits) {
    case 8:
        aFormat = AUDIO_FORMAT_PCM_8_BIT;
        break;
    case 16:
        aFormat = AUDIO_FORMAT_PCM_16_BIT;
        break;
    case 32:
        aFormat = AUDIO_FORMAT_PCM_32_BIT;
        break;
    }

    if (AudioRecord::getMinFrameCount(&frameCount, sampleRate,
            aFormat, channel) != NO_ERROR) {
        fprintf(stderr, "cannot compute frame count\n");
        return NULL;
    }
    printf("for source(%d): rate %d, channel %d, bits %d, frameCount %zu\n",
            gInDevice, sampleRate, gInChannelNum, gInBits, frameCount);

    sp<AudioRecord> record = new AudioRecord(String16("AudioDemo"));
    if (record->set(gInDevice, sampleRate, aFormat,
            channel, frameCount, NULL, NULL, 0, false,
            AUDIO_SESSION_ALLOCATE, AudioRecord::TRANSFER_OBTAIN) != NO_ERROR) {
        fprintf(stderr, "cannot initialize audio device\n");
        return NULL;
    }
    printf("alloc AudioRecord success. latency: %d ms\n", record->latency());

    return record;
}

int ParseWav(FILE* fp)
{
    typedef struct WAVE_FMT{
        char  fccID[4];
        unsigned int size;
        unsigned short format_tag;
        unsigned short channels;
        unsigned int sample_rate;
        unsigned int bytes_per_sec;
        unsigned short block_align;
        unsigned short bits_per_sample;
    }WAVE_FMT;

    typedef struct WAVE_DATA{
        char fccID[4];
        unsigned int size;
    }WAVE_DATA;

    typedef struct WAVE_HEADER{
        char fccID[4];
        unsigned int size;
        char fccType[4];
        WAVE_FMT fmt_header;
        WAVE_DATA data_header;
    }WAVE_HEADER;

    WAVE_HEADER wav_header;
    size_t readCount = fread(&wav_header, 1, sizeof(WAVE_HEADER), fp);
    if (readCount != sizeof(WAVE_HEADER)) {
        return -1;
    }

    if (strncasecmp(wav_header.fccID, "RIFF", 4) != 0 || strncasecmp(wav_header.fccType, "WAVE", 4) != 0) {
        printf("invalid fcc %s %s\n", wav_header.fccID, wav_header.fccType);
        return -1;
    }
    if (wav_header.fmt_header.format_tag != 1) { // not PCM
        return -2;
    }
    gInChannelNum = wav_header.fmt_header.channels;
    gInSampleRate = wav_header.fmt_header.sample_rate;
    gInBits = wav_header.fmt_header.bits_per_sample;

    printf("WAV file: channels=%d, rate=%d, bits=%d\n", gInChannelNum, gInSampleRate, gInBits);

    return 0;
}

int readAudio(sp<AudioRecord> record, char* data, int sampleCount, int frameSize)
{
    int toRead = sampleCount;

    printf("read sample count %d\n", sampleCount);
    while (toRead > 0) {
        AudioRecord::Buffer buffer;
        buffer.frameCount = toRead;
        status_t status = record->obtainBuffer(&buffer, 1);
        if (status == NO_ERROR) {
            int offset = sampleCount - toRead;
            memcpy(&data[offset*frameSize], buffer.i8, buffer.size);
            toRead -= buffer.frameCount;
            record->releaseBuffer(&buffer);
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            fprintf(stderr, "cannot read from AudioRecord, remind: %d\n", toRead);
            break;
        }
    }

    return sampleCount-toRead;
}

int witreAudio(sp<AudioTrack> track, char* data, int sampleCount, int frameSize)
{
    int toWrite = sampleCount;

    printf("write sample count %d\n", sampleCount);
    while (toWrite > 0) {
        AudioTrack::Buffer buffer;
        buffer.frameCount = toWrite;
        status_t status = track->obtainBuffer(&buffer, 1);
        if (status == NO_ERROR) {
            int offset = sampleCount - toWrite;
            memcpy(buffer.i8, &data[offset*frameSize], buffer.size);
            toWrite -= buffer.frameCount;
            track->releaseBuffer(&buffer);
        } else if (status != TIMED_OUT && status != WOULD_BLOCK) {
            fprintf(stderr, "cannot write to AudioTrack, remind %d\n", toWrite);
            break;
        }
    }

    return toWrite;
}

int RecordAndPlayback() {
    sp<AudioRecord> record = allocAudioRecord();
    if (record == NULL) {
        printf("Setup audio record fail!\n");
        return -1;
    }

    printf("start recording.\n");
    isRecording = true;
    if (record->start() != NO_ERROR) {
        fprintf(stderr, "record start failed, now exiting\n");
        return -1;
    }
    int32_t one;
    record->read(&one, sizeof(one));

    sp<AudioTrack> track = allocAudioTrack();
    if(track == NULL) {
        printf("Setup audio track fail!\n");
        return -1;
    }

    printf("start playing.\n");
    isPlaying = true;
    if (track->start() != NO_ERROR) {
        fprintf(stderr, "playback start failed, now exiting\n");
        record->stop();
        return -1;
    }

    int inSampleCount = gInSampleRate/10;
    int inFrameSize = gInChannelNum*gInBits/8;
    int outFrameSize = gOutChannelNum*gOutBits/8;

    char* input = new char[inFrameSize*inSampleCount];
    while (isRecording && isPlaying) {
        int readCount = readAudio(record, input, inSampleCount, inFrameSize);
        int wirteCount = (readCount*inFrameSize)/outFrameSize;
        witreAudio(track, input, wirteCount, outFrameSize);
    }
    delete []input;

    printf("playback stop\n");
    track->stop();

    printf("record stop\n");
    record->stop();

    delete []input;

    return 0;
}

int Record() {
    sp<AudioRecord> record = allocAudioRecord();
    if (record == NULL) {
        printf("Setup audio record fail!\n");
        return -1;
    }

    FILE *fp = NULL;
    fp = fopen(gOutFile, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to create file: %s\n", gOutFile);
        return -1;
    }

    printf("start record");
    isRecording = true;
    if (record->start() != NO_ERROR) {
        fprintf(stderr, "record start failed, now exiting\n");
        fclose(fp);
        return -1;
    }
    int32_t one;
    record->read(&one, sizeof(one));

    int sampleCount = gInSampleRate/10;
    int frameSize = gInChannelNum*gInBits/8;
    char* buffer = new char[frameSize*sampleCount];
    while (isRecording) {
        int readCount = readAudio(record, buffer, sampleCount, frameSize);
        fwrite(buffer, frameSize, readCount, fp);
        printf("write sample count %d\n", readCount);
        fflush(fp);
    }

    printf("record stop\n");
    record->stop();
    delete []buffer;
    fclose(fp);

    return 0;
}

#ifdef RAMP_VOLUME
void rampVolume(int16_t* data, int frameCount, bool up)
{
    printf("ramp volume %d\n", frameCount);
    int16_t *in = data;
    int16_t* out = data;
    float vl = up?0.0f:1.0f;
    const float vlInc = (up?1.0f:-1.0f)/frameCount;

    do {
        float a = vl*(float)*in;
        *out++ = (int16_t)a;
        *out++ = (int16_t)a;
        in += 2;
        vl += vlInc;
    } while (--frameCount);
}
#endif

int Playback() {
    FILE *fp = NULL;
    fp = fopen(gInFile, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", gInFile);
        return -1;
    }
    if (strstr(gInFile, ".wav")) {
        if (ParseWav(fp) < 0) {
            fprintf(stderr, "Invalid wav file!\n");
            fclose(fp);
            return -1;
        }
        if (gOutChannelNum < 0) gOutChannelNum = gInChannelNum;
        if (gOutSampleRate < 0) gOutSampleRate = gInSampleRate;
        if (gOutBits < 0) gOutBits = gInBits;
    } else {
        if (gInChannelNum < 0) gInChannelNum = CHANNEL_NUM;
        if (gInSampleRate < 0) gInSampleRate = SAMPLE_RATE;
        if (gInBits < 0) gInBits = SAMPLE_BITS;
        printf("PCM file: channels=%d, rate=%d, bits=%d\n", gInChannelNum, gInSampleRate, gInBits);
    }

    sp<AudioTrack> track = allocAudioTrack();
    if(track == NULL) {
        printf("Setup audio track fail!\n");
        fclose(fp);
        return -1;
    }

    printf("start playing.\n");
//    printf("setVolume 0\n");
//    track->setVolume(0.0f);
    isPlaying = true;
    if (track->start() != NO_ERROR) {
        fprintf(stderr, "playback start failed, now exiting\n");
        fclose(fp);
        return -1;
    }
//    nsecs_t start_tm = systemTime();
    int sampleCount = gInSampleRate/10;
    int frameSize = gInChannelNum*gInBits/8;
    char* buffer = new char[frameSize*sampleCount];
#ifdef RAMP_VOLUME
    int isFirst = 1;
#endif
    while (!feof(fp) && isPlaying) {
        size_t readCount = fread(buffer, frameSize, sampleCount, fp);
        printf("read sample count %zu\n", readCount);
#ifdef RAMP_VOLUME
        if (isFirst) {
            isFirst = 0;
            rampVolume((int16_t*)buffer, readCount, true);
        }
#endif
        witreAudio(track, buffer, readCount, frameSize);
    }

#ifdef RAMP_VOLUME
    printf("setVolume 0\n");
    track->setVolume(0.0f);
    printf("pause\n");
    track->pause();
    usleep(50*1000);
#endif

    printf("playback stop\n");
    track->stop();
    delete []buffer;
    fclose(fp);

    return 0;
}

int Resample()
{
    int ret;
    struct resampler_itfe *ri;

    if (gInChannelNum < 0) gInChannelNum = CHANNEL_NUM;
    if (gInSampleRate < 0) gInSampleRate = SAMPLE_RATE;
    if (gInBits < 0) gInBits = SAMPLE_BITS;

    if (gOutSampleRate < 0) gOutSampleRate = SAMPLE_RATE;

    ret = create_resampler(gInSampleRate, gOutSampleRate, gInChannelNum,
                            gResample, NULL, &ri);
    printf("resampler rate: %d -> %d, channels=%d, quality=%d\n",
            gInSampleRate, gOutSampleRate, gInChannelNum, gResample);
    if (ret != 0) {
        fprintf(stderr, "create_resampler fail %d\n", ret);
        return -1;
    }

    FILE* fpin = fopen(gInFile, "rb");
    if (fpin == NULL) {
        printf("fail open %s\n", gInFile);
        release_resampler(ri);
        return -1;
    }
    FILE* fpout = fopen(gOutFile, "wb");
    if (fpout == NULL) {
        printf("fail open %s\n", gOutFile);
        release_resampler(ri);
        fclose(fpin);
        return -1;
    }

    size_t frame_size = gInChannelNum * (gInBits>>3);
    int8_t* inbuf = new int8_t[gInSampleRate*frame_size];
    int8_t* outbuf = new int8_t[gOutSampleRate*frame_size];
    size_t inFrameCount, outFrameCount;

    while (1) {
        int in_bytes = fread(inbuf, 1, gInSampleRate*frame_size, fpin);
        if (in_bytes <= 0) {
            printf("EOF\n");
            break;
        }
        inFrameCount = in_bytes/frame_size;
        outFrameCount = inFrameCount*gOutSampleRate/gInSampleRate;

        ret = ri->resample_from_input(ri, (int16_t*)inbuf, &inFrameCount, (int16_t*)outbuf, &outFrameCount);
        printf("resampler: in %zu, out %zu\n", inFrameCount, outFrameCount);
        if (ret < 0) {
            printf("resample failed %d\n", ret);
            break;
        }
        fwrite(outbuf, 1, outFrameCount*frame_size, fpout);
    }

    release_resampler(ri);
    fclose(fpin);
    fclose(fpout);
    delete []inbuf;
    delete []outbuf;

    return 0;
}

static void createSine(void *vbuffer, size_t frames,
        size_t channels, double sampleRate, double freq)
{
    double tscale = 1.0 / sampleRate;
    int16_t* buffer = (int16_t*)vbuffer;
    for (size_t i = 0; i < frames; ++i) {
        double t = i * tscale;
        double y = sin(2.0 * M_PI * freq * t);
        int16_t yt = 0.8*floor(y * 32767.0 + 0.5);

        for (size_t j = 0; j < channels; ++j) {
            buffer[i*channels + j] = yt;
        }
    }
}

int CreateSineFile()
{
    int16_t* buffer = NULL;
    if (gOutChannelNum < 0) gOutChannelNum = CHANNEL_NUM;
    if (gOutSampleRate < 0) gOutSampleRate = SAMPLE_RATE;
    if (gOutBits != 16) {
        printf("MakeSine: only support 16bits!\n");
        gOutBits = SAMPLE_BITS;
    }

    FILE *fp = NULL;
    fp = fopen(gOutFile, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to create file: %s\n", gOutFile);
        return -1;
    }

    buffer = new int16_t[gOutSampleRate*gOutChannelNum];
    int i=0;
    for (i=0; i<gSineTime; i++) {
        createSine(buffer, gOutSampleRate, gOutChannelNum, gOutSampleRate, gSineFreq);
        fwrite(buffer, gOutChannelNum*sizeof(int16_t), gOutSampleRate, fp);
    }

    delete []buffer;
    fclose(fp);

    printf("CreateSineFile leave\n");

    return 0;
}

/************************************************************
*
*    main in name space
*
************************************************************/
int exectue() {
    if (gResample >= 0) {
        if (gInFile[0] == 0 || gOutFile[0] == 0) {
            printf("Resample: invalid parameter!\n");
            return -1;
        }
        printf("Resample from file %s to file %s\n", gInFile, gOutFile);
        Resample();
    } else if (gSineFreq >= 0) {
        if (gOutFile[0] == 0) {
            printf("MakeSine: invalid parameter!\n");
            return -1;
        }
        printf("MakeSine: write to file %s \n", gOutFile);
        CreateSineFile();
    } else if (gInFile[0] != 0 && gOutDevice >= 0) {
        printf("Playback from file %s to stream %d\n", gInFile, gOutDevice);
        Playback();
    } else if (gInDevice >= 0 && gOutFile[0] != 0) {
        printf("Record from source %d to file %s\n", gInDevice, gOutFile);
        Record();
    } else if (gInDevice >= 0 && gOutDevice >= 0){
        printf("from source %d to stream %d\n", gInDevice, gOutDevice);
        RecordAndPlayback();
    }

    return 0;
}

}

void exitsig(int x) {
    if (android::isPlaying || android::isRecording) {
        android::isPlaying = false;
        android::isRecording = false;
        printf("Stopping playback or record\n");
    } else {
        exit(x);
    }
}

static void showhelp(const char* cmd)
{
    fprintf(stderr, "%s [in options] [out options] [options]\n", cmd);
    fprintf(stderr, "  --in=<source or file>: read from audio source or file, support source:\n");
    fprintf(stderr, "       1 - build-in mic (default)\n");
    fprintf(stderr, "       8 - remote submix\n");
    fprintf(stderr, "  --out=<device or file>: write to audio device or file, support device:\n");
    fprintf(stderr, "       1 - system\n");
    fprintf(stderr, "       3 - music (default)\n");
    fprintf(stderr, "  --[in or out]-channel=<channels>:\n");
    fprintf(stderr, "       1 - mono\n");
    fprintf(stderr, "       2 - stereo (default)\n");
    fprintf(stderr, "  --[in or out]-rate=<rate>:\n");
    fprintf(stderr, "       44100 (default)\n");
    fprintf(stderr, "       48000\n");
    fprintf(stderr, "  --[in or out]-bits=<bits>:\n");
    fprintf(stderr, "       8\n");
    fprintf(stderr, "       16 (default)\n");
    fprintf(stderr, "       32\n");
    fprintf(stderr, "  --resample[=quality]:\n");
    fprintf(stderr, "        0 - min\n");
    fprintf(stderr, "       10 - max\n");
    fprintf(stderr, "        4 (default)\n");
    fprintf(stderr, "  --sine[=freq]\n");
    fprintf(stderr, "  --duration=<duration ms>\n");
    fprintf(stderr, "  --help: print this help.\n");
}

/************************************************************
*
*    global main
*
************************************************************/
int main(int argc, char** argv)
{
    signal(SIGINT,exitsig);
    signal(SIGQUIT,exitsig);
    signal(SIGHUP,exitsig);
    signal(SIGPIPE,exitsig);
    signal(SIGTERM,exitsig);
    signal(SIGSEGV,exitsig);
    signal(SIGBUS,exitsig);
    signal(SIGILL,exitsig);
    signal(SIGALRM,exitsig);

	while (1) {
        int ret;
        int option_index;
        static const struct option long_options[] = {
          { "in",            required_argument, NULL,   'i' },
          { "in-channel",    required_argument, NULL,   'c' },
          { "in-rate",       required_argument, NULL,   'r' },
          { "in-bits",       required_argument, NULL,   'b' },

          { "out",           required_argument, NULL,   'o' },
          { "out-channel",   required_argument, NULL,   'C' },
          { "out-rate",      required_argument, NULL,   'R' },
          { "out-bits",      required_argument, NULL,   'B' },

          { "duration",      required_argument, NULL,   't' },
          { "resample",      optional_argument, NULL,   'q' },
          { "sine",          optional_argument, NULL,   's' },
          { "help",          no_argument,       NULL,   'h' },
          { NULL,            0,                 NULL,    0  }
        };

        ret = getopt_long(argc, argv, "s:c:r:b:d:C:R:B:t:h",
                          long_options, &option_index);
        if (ret < 0) {
            break;
        }
        switch (ret) {
            case 'i':
                if (optarg[0] >= '0' && optarg[0]<='9')
                    android::gInDevice = (audio_source_t)atoi(optarg);
                else
                    sprintf(android::gInFile, "%s", optarg);
                break;
            case 'o':
                if (optarg[0] >= '0' && optarg[0]<='9')
                    android::gOutDevice = (audio_stream_type_t)atoi(optarg);
                else
                    sprintf(android::gOutFile, "%s", optarg);
                break;
            case 'c': android::gInChannelNum = atoi(optarg); break;
            case 'C': android::gOutChannelNum = atoi(optarg); break;
            case 'r': android::gInSampleRate = atoi(optarg); break;
            case 'R': android::gOutSampleRate = atoi(optarg); break;
            case 'b': android::gInBits = atoi(optarg); break;
            case 'B': android::gOutBits = atoi(optarg); break;
            case 't': alarm(atoi(optarg)); break;
            case 'q': android::gResample = optarg?atoi(optarg):RESAMPLER_QUALITY_DEFAULT; break;
            case 's': android::gSineFreq = optarg?atoi(optarg):SINE_FREQ; break;
            case 'h': default: showhelp(argv[0]); exit(-1); break;
        }
    }

    return android::exectue();
}
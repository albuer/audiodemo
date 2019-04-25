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

//
#define LOG_TAG "AudioDemo"
#include <utils/Log.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <fcntl.h>

#include <media/AudioSystem.h>
#include <media/AudioTrack.h>
#include <media/AudioRecord.h>

namespace android {

/************************************************************
*
*    Constructor
*
************************************************************/

int             gDevice = -2;
audio_source_t      gInDevice = AUDIO_SOURCE_MIC;
audio_stream_type_t gOutDevice = AUDIO_STREAM_MUSIC;

int             gChannelNum = 2;
int             gInChannelNum = 2;
int             gOutChannelNum = 2;

int             gSampleRate = 44100;
int             gInSampleRate = 44100;
int             gOutSampleRate = 44100;

int             gBits = 16;
int             gInBits = 16;
int             gOutBits = 16;

int             gMode = 0; // 0 - playback, 1 - record, 2 - record and playback
char            gFile[512] = "";

bool            isPlaying = false;
bool            isRecording = false;

sp<AudioTrack> allocAudioTrack(void)
{
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
    printf("for stream(%d): rate %d, frameCount %zu, channel 0x%x, pcmFormat 0x%x\n",
            gOutDevice, sampleRate, frameCount, channel, aFormat);

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
    printf("for source(%d): rate %d, frameCount %zu, channel 0x%x, pcmFormat 0x%x\n",
            gInDevice, sampleRate, frameCount, channel, aFormat);

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
    fp = fopen(gFile, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to create file: %s\n", gFile);
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
    char* input = new char[frameSize*sampleCount];
    while (isRecording) {
        int readCount = readAudio(record, input, sampleCount, frameSize);
        fwrite(input, frameSize, readCount, fp);
        printf("write sample count %d\n", readCount);
        fflush(fp);
    }

    printf("record stop\n");
    record->stop();
    delete []input;
    fclose(fp);

    return 0;
}

int Playback() {
    sp<AudioTrack> track = allocAudioTrack();
    if(track == NULL) {
        printf("Setup audio track fail!\n");
        return -1;
    }

    FILE *fp = NULL;
    fp = fopen(gFile, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", gFile);
        return -1;
    }

    printf("start playing.\n");
    ALOGD("setVolume 0\n");
    track->setVolume(0.0f);
    isPlaying = true;
    if (track->start() != NO_ERROR) {
        fprintf(stderr, "playback start failed, now exiting\n");
        fclose(fp);
        return -1;
    }
    nsecs_t start_tm = systemTime();
    int sampleCount = gOutSampleRate/10;
    int frameSize = gOutChannelNum*gOutBits/8;
    char* output = new char[frameSize*sampleCount];
    while (!feof(fp) && isPlaying) {
        size_t readCount = fread(output, frameSize, sampleCount, fp);
        printf("read sample count %zu\n", readCount);
        witreAudio(track, output, readCount, frameSize);
        if ((systemTime() - start_tm) >= 50*1000*1000) {
            ALOGD("setVolume 1\n");
            track->setVolume(1.0f);
        }
    }

    ALOGD("setVolume 0\n");
    track->setVolume(0.0f);
    ALOGD("pause\n");
    track->pause();
    usleep(50*1000);
    ALOGD("stop\n");
    track->stop();
    delete []output;
    fclose(fp);

    return 0;
}

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

#include <audio_utils/resampler.h>
// ./audiodemo -m 3 -c 2 -b 16 -r 32000,44100
int Resample()
{
    int ret;
    struct resampler_itfe *ri;
    ret = create_resampler(gInSampleRate, gOutSampleRate, gChannelNum,
                            RESAMPLER_QUALITY_DEFAULT, NULL, &ri);
    printf("create_resampler return %d\n", ret);
    if (ret != 0) {
        return -1;
    }

    FILE* fpin = fopen("32000.pcm", "rb");
    if (fpin == NULL) {
        printf("fail open 32000.pcm\n");
        release_resampler(ri);
        return -1;
    }
    FILE* fpout = fopen("44100.pcm", "wb");
    if (fpout == NULL) {
        printf("fail open 44100.pcm\n");
        release_resampler(ri);
        fclose(fpin);
        return -1;
    }

    size_t frame_size = gChannelNum * (gBits>>3);
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

/************************************************************
*
*    main in name space
*
************************************************************/
int exectue() {
    if (gMode == 0){
        gOutDevice = (audio_stream_type_t)gDevice;
        gOutSampleRate = gSampleRate;
        gOutChannelNum = gChannelNum;
        gOutBits = gBits;

        if (CheckPlaybackParams() != 0) {
            printf("Invalid playback params!\n");
            return -1;
        }

        printf("Playback from file %s to stream %d\n", gFile, gOutDevice);
        Playback();
    } else if (gMode == 1){
        gInDevice = (audio_source_t)gDevice;
        gInSampleRate = gSampleRate;
        gInChannelNum = gChannelNum;
        gInBits = gBits;

        if (CheckRecordParams() != 0) {
            printf("Invalid playback params!\n");
            return -1;
        }

        printf("Record from source %d to file %s\n", gInDevice, gFile);
        Record();
    } else if (gMode == 2){
        if (gDevice != -2) {
            gInDevice = (audio_source_t)gDevice;
            gOutDevice = (audio_stream_type_t)gDevice;
        }
        if (gSampleRate != 44100) gInSampleRate = gOutSampleRate = gSampleRate;
        if (gChannelNum != 2) gInChannelNum = gOutChannelNum = gChannelNum;
        if (gBits != 16) gInBits = gOutBits = gBits;

        if (CheckPlaybackParams() != 0 || CheckRecordParams() != 0) {
            printf("Invalid playback or record params!\n");
            return -1;
        }

        printf("from source %d to stream %d\n", gInDevice, gOutDevice);
        RecordAndPlayback();
    } else if (gMode == 3) {
        // resampler
        Resample();
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
    fprintf(stderr, "%s [-m mode] [-d source/stream] [-c channels] [-r rate] [-b bits] [-t timer] [file]\n", cmd);
    fprintf(stderr, "  mode:\n");
    fprintf(stderr, "       0 - playback\n");
    fprintf(stderr, "       1 - record\n");
    fprintf(stderr, "       2 - record & playback\n");
    fprintf(stderr, "  source:\n");
    fprintf(stderr, "       1 - build-in mic (default)\n");
    fprintf(stderr, "       8 - remote submix\n");
    fprintf(stderr, "  stream:\n");
    fprintf(stderr, "       1 - system\n");
    fprintf(stderr, "       3 - music (default)\n");
    fprintf(stderr, "  channels:\n");
    fprintf(stderr, "       1 - mono\n");
    fprintf(stderr, "       2 - stereo (default)\n");
    fprintf(stderr, "  rate:\n");
    fprintf(stderr, "       44100 (default)\n");
    fprintf(stderr, "       48000\n");
    fprintf(stderr, "  bits:\n");
    fprintf(stderr, "       8\n");
    fprintf(stderr, "       16 (default)\n");
    fprintf(stderr, "       32\n");
    fprintf(stderr, "  timer: duration time\n");
    fprintf(stderr, "  file: for read(playback mode) or for write(record mode)\n");
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

	int ch;

    int i_arg = 1;
	while ((ch = getopt(argc, argv, "d:c:r:b:m:t:h")) != -1) {
        char* p = NULL;
        ++i_arg;
		switch(ch) {
        case 'd':
            if ((p=strchr(optarg, ','))!=NULL) {
                android::gInDevice = (audio_source_t)atoi(optarg);
                android::gOutDevice = (audio_stream_type_t)atoi(p+1);
            } else
                android::gDevice = atoi(optarg);
            ++i_arg;
            break;
		case 'c':
            if ((p=strchr(optarg, ','))!=NULL) {
                android::gInChannelNum = atoi(optarg);
                android::gOutChannelNum = atoi(p+1);
            } else
                android::gChannelNum = atoi(optarg);
            ++i_arg;
			break;
		case 'r':
            if ((p=strchr(optarg, ','))!=NULL) {
                android::gInSampleRate = atoi(optarg);
                android::gOutSampleRate = atoi(p+1);
            } else
    			android::gSampleRate = atoi(optarg);
            ++i_arg;
			break;
		case 'b':
            if ((p=strchr(optarg, ','))!=NULL) {
                android::gInBits = atoi(optarg);
                android::gOutBits = atoi(p+1);
            } else
                android::gBits = atoi(optarg);
            ++i_arg;
			break;
        case 'm':
            android::gMode = atoi(optarg);
            ++i_arg;
            break;
        case 't':
        {
            int timer = atoi(optarg);
            printf("set timer %d\n", timer);
            alarm(timer);
            ++i_arg;
            break;
        }
        case 'h':
		default:
			showhelp(argv[0]);
			exit(-1);
			break;
		}
	}

    if (i_arg<argc) {
        sprintf(android::gFile, "%s", argv[i_arg]);
        ++i_arg;
    }

    if (i_arg != argc) {
        showhelp(argv[0]);
        exit(-1);
    }

    return android::exectue();
}


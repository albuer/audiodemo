// Copyright 2008 The Android Open Source Project

#ifndef AUDIODEMO_H_
#define AUDIODEMO_H_

namespace android {

class AudioDemo{
public:
    AudioDemo(void);

    void Execute(void);
    int Playback();
    int Record();
    int RecordAndPlayback();
};

};


#endif /*AUDIODEMO_H_*/

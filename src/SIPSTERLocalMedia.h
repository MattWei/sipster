#ifndef SIPSTERLOCALMEDIA_H_
#define SIPSTERLOCALMEDIA_H_

#include <node.h>
#include <nan.h>
#include <string.h>
//#include <strings.h>
//#include <regex.h>
#include <pjsua2.hpp>

using namespace std;
using namespace node;
using namespace v8;
using namespace pj;

class SIPSTERLocalMedia : public Nan::ObjectWrap, public AudioMedia
{
private:
    AudioMediaPlayer *mPlayer;
    AudioMediaRecorder *mRecorder;

  public:
    SIPSTERLocalMedia();
    ~SIPSTERLocalMedia();

    bool startRecord(std::string fileName);
    bool stopRecord();
    bool startPlay(std::string fileName);
    bool stopPlay();
};

#endif
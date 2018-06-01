#include "SIPSTERLocalMedia.h"

SIPSTERLocalMedia::SIPSTERLocalMedia()
    : mPlayer(NULL)
    , mRecorder(NULL)
{
}

SIPSTERLocalMedia::~SIPSTERLocalMedia()
{
    stopRecord();
    stopPlay();
}

bool SIPSTERLocalMedia::startRecord(std::string fileName)
{
    stopRecord();

    mRecorder = new AudioMediaRecorder();
    AudioMedia& cap_med = Endpoint::instance().audDevManager().getCaptureDevMedia();
    try {
        mRecorder->createRecorder(fileName);
        cap_med.startTransmit(*mRecorder);
        return true;
    } catch(Error& err) {
        return false;
    }
}

bool SIPSTERLocalMedia::stopRecord()
{
    if (!mRecorder) {
        return true;
    } 

    AudioMedia& cap_med = Endpoint::instance().audDevManager().getCaptureDevMedia();
    try {
        cap_med.stopTransmit(*mRecorder);
        delete mRecorder;
        mRecorder = NULL;
        return true;
    } catch(Error& err) {
        return false;
    }
}

bool SIPSTERLocalMedia::startPlay(std::string fileName)
{
    stopPlay();

    mPlayer = new AudioMediaPlayer();
    AudioMedia& play_med = Endpoint::instance().audDevManager().getPlaybackDevMedia();
    try {
        mPlayer->createPlayer(fileName, PJMEDIA_FILE_NO_LOOP);
        mPlayer->startTransmit(play_med);

        return true;
    } catch(Error& err) {
        return false;
    }
}

bool SIPSTERLocalMedia::stopPlay()
{
    if (!mPlayer) {
        return true;
    }

    AudioMedia& play_med = Endpoint::instance().audDevManager().getPlaybackDevMedia();
    try {
        mPlayer->stopTransmit(play_med);
        delete mPlayer;
        mPlayer = NULL;
        return true;
    } catch(Error& err) {
        return false;
    }
}
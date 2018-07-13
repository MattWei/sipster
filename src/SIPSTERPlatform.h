#ifndef __SIPSTER_PLATFORM_H__
#define __SIPSTER_PLATFORM_H__


#include <node.h>
#include <nan.h>

#include <string>
#include <thread>

using namespace std;
using namespace node;
using namespace v8;

#ifdef _WIN32
#define NORMAL_PRIORITY_CLASS 0x00000020
#endif

class SIPSTERPlatform : public Nan::ObjectWrap {
private:
    std::thread *mDogVerifyThread = NULL;
    bool mIsRunning = false;
    bool mSystemHaveInit = false;
    bool runCMD(std::string csExe, std::string csParam = "", bool bWait = false, unsigned long dwPriority = NORMAL_PRIORITY_CLASS);
    void sendState(std::string state);
    bool verifyDog();

public:
    Nan::Callback* emit;

    SIPSTERPlatform() {}
    ~SIPSTERPlatform();

    bool initSystem();
    void allowPassFireWall();

    void startDogVerifyLoop();
    bool tryToVerifyDog();

    static NAN_METHOD(Init);
    static NAN_METHOD(New);

    static NAN_GETTER(VerifyDog);

    static void Initialize(Handle<Object> target);
};

#endif
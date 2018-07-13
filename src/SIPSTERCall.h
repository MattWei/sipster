#ifndef SIPSTERCALL_H_
#define SIPSTERCALL_H_

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

class TonePlayer;

enum PlayerType {START_TONE, STOP_TONE, MUSIC};

class SIPSTERCall : public Call, public Nan::ObjectWrap {
private:
  enum MicCallState { PLAY_START_TONE, MIC_CALLING, PLAY_STOP_TONE, STOPED };

  bool mIsAutoConnect;
  std::string mStartTone;
  std::string mStopTone;
  MicCallState mCallState;

  std::string mCurrentSong = "";
  TonePlayer *mTonePlayer;

  void micCallMediaStateChanged();
  void connectCaptureDevice();
  void playTone(std::string tonePath);

  void playMusic();

  void sendPlayState(std::string song, int type, int param);
public:
  Nan::Callback* emit;

  SIPSTERCall(Account &acc, int call_id=PJSUA_INVALID_ID);
  ~SIPSTERCall();

  void createPlayer(std::string song);
  void onPlayEof(PlayerType type);

  void setAudoConnect(bool isAutoConnect);
  void setTones(std::string startTone, std::string stopTone);

  void onCallMediaState(OnCallMediaStateParam &prm);
  virtual void onCallState(OnCallStateParam &prm);
  virtual void onDtmfDigit(OnDtmfDigitParam &prm);
  virtual void hangup(const CallOpParam &prm);
  virtual void changeMusic(std::string musicPath);

  static NAN_METHOD(New);
  static NAN_METHOD(Answer);
  static NAN_METHOD(Hangup);
  static NAN_METHOD(SetHold);
  static NAN_METHOD(Reinvite);
  static NAN_METHOD(Update);
  static NAN_METHOD(DialDtmf);
  static NAN_METHOD(Transfer);
  static NAN_METHOD(DoRef);
  static NAN_METHOD(DoUnref);
  static NAN_METHOD(GetStats);
  static NAN_METHOD(PlayMusic);
  static NAN_GETTER(ConDurationGetter);
  static NAN_GETTER(TotDurationGetter);
  static NAN_GETTER(HasMediaGetter);
  static NAN_GETTER(IsActiveGetter);
  static NAN_GETTER(CallInfoGetter);

  static void Initialize(Handle<Object> target);
};

class TonePlayer : public AudioMediaPlayer
{
public:
  TonePlayer(SIPSTERCall* call, PlayerType type) : mCall(call), mType(type) {}
  ~TonePlayer() {}

  virtual bool onEof()
  {
    if (mCall) {
      mCall->onPlayEof(mType);
    }

    return true;
  }

private:
  SIPSTERCall *mCall;
  PlayerType mType;
};
#endif

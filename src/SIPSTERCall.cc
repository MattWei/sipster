#include "SIPSTERCall.h"
#include "SIPSTERAccount.h"
#include "common.h"

#include <iostream>

enum PlayStateType { EXIT, START, PLAYING, END };

Nan::Persistent<FunctionTemplate> SIPSTERCall_constructor;

SIPSTERCall::SIPSTERCall(Account &acc, int call_id) : Call(acc, call_id)
{
  mCallState = PLAY_START_TONE;
  mTonePlayer = NULL;
  mIsAutoConnect = false;
  emit = NULL;
  uv_mutex_lock(&async_mutex);
  uv_ref(reinterpret_cast<uv_handle_t *>(&dumb));
  uv_mutex_unlock(&async_mutex);
}

SIPSTERCall::~SIPSTERCall()
{
  if (emit)
    delete emit;

  uv_mutex_lock(&async_mutex);
  uv_unref(reinterpret_cast<uv_handle_t *>(&dumb));
  uv_mutex_unlock(&async_mutex);
}

void SIPSTERCall::setAudoConnect(bool isAutoConnect)
{
  mIsAutoConnect = isAutoConnect;
}

void SIPSTERCall::onPlayEof(PlayerType type)
{
  if (type == START_TONE)
  {
    std::cout << "#######Start Tone play eof" << std::endl;

    mCallState = MIC_CALLING;
    delete mTonePlayer;
    mTonePlayer = NULL;
    connectCaptureDevice();
  }
  else if (type == STOP_TONE)
  {
    std::cout << "#######Stop Tone play eof" << std::endl;

    delete mTonePlayer;
    mTonePlayer = NULL;
    mCallState = STOPED;

    CallOpParam prm(true);
    Call::hangup(prm);
  }
  else if (type == MUSIC)
  {
    std::cout << "#######Music play eof" << std::endl;
    sendPlayState(mCurrentSong, END, 1000);
  }
}

void SIPSTERCall::sendPlayState(std::string song, int type, int param)
{
    SETUP_EVENT(PLAYERSTATUS);
    std::cout << "Play eof:" << song << ", type:" << type << std::endl;
    ev.call = this;
    args->songPath = song;
    args->type = type;
    args->param = param;

    ENQUEUE_EVENT(ev);
}

void SIPSTERCall::setTones(std::string startTone, std::string stopTone)
{
  mStartTone = startTone;
  mStopTone = stopTone;
  std::cout << "Set start tone:" << mStartTone << ",stop tone:" << mStopTone << std::endl;
}

void SIPSTERCall::connectCaptureDevice()
{
  mCallState = MIC_CALLING;

  CallInfo ci = getInfo();
  AudDevManager &mgr = Endpoint::instance().audDevManager();
  for (unsigned i = 0; i < ci.media.size(); i++)
  {
    CallMediaInfo mediaInfo = ci.media[i];
    AudioMedia *aud_med = (AudioMedia *)getMedia(i);
    if (mediaInfo.type == PJMEDIA_TYPE_AUDIO && aud_med)
    {
      if (mediaInfo.status == PJSUA_CALL_MEDIA_ACTIVE)
      {
        mgr.getCaptureDevMedia().startTransmit(*aud_med);
      }
      else if (mediaInfo.status == PJSUA_CALL_MEDIA_LOCAL_HOLD ||
               mediaInfo.status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
      {
        mgr.getCaptureDevMedia().stopTransmit(*aud_med);
      }
    }
  }
}

void SIPSTERCall::playTone(std::string tonePath)
{
  if (!mTonePlayer)
  {
    PlayerType type = START_TONE;
    if (mCallState == PLAY_STOP_TONE) {
      type = STOP_TONE;
    }
    mTonePlayer = new TonePlayer(this, type);
    mTonePlayer->createPlayer(tonePath, PJMEDIA_FILE_NO_LOOP);
  }

  AudDevManager &mgr = Endpoint::instance().audDevManager();
  CallInfo ci = getInfo();
  for (unsigned i = 0; i < ci.media.size(); i++)
  {
    CallMediaInfo mediaInfo = ci.media[i];
    AudioMedia *aud_med = (AudioMedia *)getMedia(i);
    if (mediaInfo.type == PJMEDIA_TYPE_AUDIO && aud_med)
    {
      mgr.getCaptureDevMedia().stopTransmit(*aud_med);
      if (mediaInfo.status == PJSUA_CALL_MEDIA_ACTIVE)
      {
        mTonePlayer->startTransmit(*aud_med);
      }
      else if (mediaInfo.status == PJSUA_CALL_MEDIA_LOCAL_HOLD ||
               mediaInfo.status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
      {
        mTonePlayer->stopTransmit(*aud_med);
      }
    }
  }
}

void SIPSTERCall::micCallMediaStateChanged()
{
  if (mCallState == START_TONE && mStartTone != "")
  {
    playTone(mStartTone);
    return;
  }

  if (mCallState == STOP_TONE && mStopTone != "")
  {
    playTone(mStopTone);
    return;
  }

  connectCaptureDevice();
}

void SIPSTERCall::onCallMediaState(OnCallMediaStateParam &prm)
{
  if (mIsAutoConnect)
  {
    micCallMediaStateChanged();
  }
  else
  {
    playMusic();
  }
}

void SIPSTERCall::playMusic()
{
  if (mTonePlayer == NULL) {
    return;
  }

  std::cout << "########playMusic:" << mCurrentSong << std::endl;
  CallInfo ci = getInfo();
  for (unsigned i = 0; i < ci.media.size(); i++)
  {
    CallMediaInfo mediaInfo = ci.media[i];
    AudioMedia *aud_med = (AudioMedia *)getMedia(i);
    if (mediaInfo.type == PJMEDIA_TYPE_AUDIO && aud_med)
    {
      if (mediaInfo.status == PJSUA_CALL_MEDIA_ACTIVE)
      {
        mTonePlayer->startTransmit(*aud_med);
      }
      else if (mediaInfo.status == PJSUA_CALL_MEDIA_LOCAL_HOLD ||
               mediaInfo.status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
      {
        mTonePlayer->stopTransmit(*aud_med);
      }
    }
  }
}

void SIPSTERCall::changeMusic(std::string musicPath)
{
  if (mIsAutoConnect) {
    return;
  }

  mCurrentSong = musicPath;
  TonePlayer *newPlayer = new TonePlayer(this, MUSIC);
  newPlayer->createPlayer(musicPath, PJMEDIA_FILE_NO_LOOP);

  std::cout << "########change to play music:" << mCurrentSong << std::endl;
  CallInfo ci = getInfo();
  for (unsigned i = 0; i < ci.media.size(); i++)
  {
    CallMediaInfo mediaInfo = ci.media[i];
    AudioMedia *aud_med = (AudioMedia *)getMedia(i);
    if (mediaInfo.type == PJMEDIA_TYPE_AUDIO && aud_med)
    {
      if (mTonePlayer) {
        mTonePlayer->stopTransmit(*aud_med);
      }
      if (mediaInfo.status == PJSUA_CALL_MEDIA_ACTIVE)
      {
        newPlayer->startTransmit(*aud_med);
      }
    }
  }

  delete mTonePlayer;
  mTonePlayer = newPlayer;
  sendPlayState(mCurrentSong, START, 0);
}

void SIPSTERCall::hangup(const CallOpParam &prm)
{
  if (mCallState == PLAY_START_TONE && mTonePlayer)
  {
    delete mTonePlayer;
    mTonePlayer = NULL;
  }

  mCallState = PLAY_STOP_TONE;
  if (mStopTone != "")
  {
    playTone(mStopTone);
  }
  else
  {
    mCallState = STOPED;
    Call::hangup(prm);
  }
}

void SIPSTERCall::onCallState(OnCallStateParam &prm)
{
  CallInfo ci = getInfo();

  SETUP_EVENT(CALLSTATE);
  ev.call = this;

  args->_state = ci.state;
  args->_lastStatuscode = ci.lastStatusCode;

  ENQUEUE_EVENT(ev);
}

void SIPSTERCall::onDtmfDigit(OnDtmfDigitParam &prm)
{
  CallInfo ci = getInfo();

  SETUP_EVENT(CALLDTMF);
  ev.call = this;

  args->digit = prm.digit[0];

  ENQUEUE_EVENT(ev);
}

void SIPSTERCall::createPlayer(std::string song)
{
  mCurrentSong = song;
  mTonePlayer = new TonePlayer(this, MUSIC);
  mTonePlayer->createPlayer(mCurrentSong, PJMEDIA_FILE_NO_LOOP);
  std::cout << "#######Create player" << mCurrentSong << std::endl;
}

NAN_METHOD(SIPSTERCall::New)
{
  Nan::HandleScope scope;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Use `new` to create instances of this object.");

  SIPSTERCall *call = NULL;
  if (info.Length() > 0)
  {
    if (Nan::New(SIPSTERAccount_constructor)->HasInstance(info[0]))
    {
      Account *acct =
          Nan::ObjectWrap::Unwrap<SIPSTERAccount>(Local<Object>::Cast(info[0]));
      call = new SIPSTERCall(*acct);
    }
    else
    {
      Local<External> extCall = Local<External>::Cast(info[0]);
      call = static_cast<SIPSTERCall *>(extCall->Value());
    }
  }
  else
    return Nan::ThrowError("Expected callId or Account argument");

  call->Wrap(info.This());

  call->emit = new Nan::Callback(
      Local<Function>::Cast(call->handle()->Get(Nan::New(emit_symbol))));

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(SIPSTERCall::Answer)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsUint32())
  {
    prm.statusCode = static_cast<pjsip_status_code>(info[0]->Int32Value());
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String reason_str(info[1]);
      prm.reason = string(*reason_str);
    }
  }
  else
    prm.statusCode = PJSIP_SC_OK;

  try
  {
    call->answer(prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.answer() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::Hangup)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsUint32())
  {
    prm.statusCode = static_cast<pjsip_status_code>(info[0]->Int32Value());
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String reason_str(info[1]);
      prm.reason = string(*reason_str);
    }
  }

  try
  {
    call->hangup(prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.hangup() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::SetHold)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsUint32())
  {
    prm.statusCode = static_cast<pjsip_status_code>(info[0]->Int32Value());
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String reason_str(info[1]);
      prm.reason = string(*reason_str);
    }
  }

  try
  {
    call->setHold(prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.setHold() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::Reinvite)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsUint32())
  {
    prm.statusCode = static_cast<pjsip_status_code>(info[0]->Int32Value());
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String reason_str(info[1]);
      prm.reason = string(*reason_str);
    }
  }

  try
  {
    call->reinvite(prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.reinvite() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::Update)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsUint32())
  {
    prm.statusCode = static_cast<pjsip_status_code>(info[0]->Int32Value());
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String reason_str(info[1]);
      prm.reason = string(*reason_str);
    }
  }

  try
  {
    call->update(prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.update() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::DialDtmf)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  if (info.Length() > 0 && info[0]->IsString())
  {
    try
    {
      Nan::Utf8String dtmf_str(info[0]);
      call->dialDtmf(string(*dtmf_str));
    }
    catch (Error &err)
    {
      string errstr = "Call.dialDtmf() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }
  else
    return Nan::ThrowTypeError("Missing DTMF string");

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::PlayMusic)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  if (info.Length() > 0 && info[0]->IsString())
  {
    try
    {
      Nan::Utf8String songPath(info[0]);
      call->changeMusic(string(*songPath));
    }
    catch (Error &err)
    {
      string errstr = "Call.play() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }
  else
    return Nan::ThrowTypeError("Missing music path");

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::Transfer)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  string dest;
  CallOpParam prm;
  if (info.Length() > 0 && info[0]->IsString())
  {
    Nan::Utf8String dest_str(info[0]);
    dest = string(*dest_str);
    if (info.Length() > 1)
    {
      prm.statusCode = static_cast<pjsip_status_code>(info[1]->Int32Value());
      if (info.Length() > 2 && info[2]->IsString())
      {
        Nan::Utf8String reason_str(info[2]);
        prm.reason = string(*reason_str);
      }
    }
  }
  else
    return Nan::ThrowTypeError("Missing transfer destination");

  try
  {
    call->xfer(dest, prm);
  }
  catch (Error &err)
  {
    string errstr = "Call.xfer() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::DoRef)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  call->Ref();

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::DoUnref)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  call->Unref();

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERCall::GetStats)
{
  Nan::HandleScope scope;
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  bool with_media = true;
  string indent = "  ";
  if (info.Length() > 0 && info[0]->IsBoolean())
  {
    with_media = info[0]->BooleanValue();
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String indent_str(info[1]);
      indent = string(*indent_str);
    }
  }

  string stats_info;
  try
  {
    stats_info = call->dump(with_media, indent);
  }
  catch (Error &err)
  {
    string errstr = "Call.dump() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().Set(Nan::New(stats_info.c_str()).ToLocalChecked());
}

NAN_GETTER(SIPSTERCall::ConDurationGetter)
{
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallInfo ci;

  try
  {
    ci = call->getInfo();
  }
  catch (Error &err)
  {
    string errstr = "Call.getInfo() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  double duration = ci.connectDuration.sec + (ci.connectDuration.msec / 1000);

  info.GetReturnValue().Set(Nan::New<Number>(duration));
}

NAN_GETTER(SIPSTERCall::TotDurationGetter)
{
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallInfo ci;

  try
  {
    ci = call->getInfo();
  }
  catch (Error &err)
  {
    string errstr = "Call.getInfo() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  double duration = ci.totalDuration.sec + (ci.totalDuration.msec / 1000);

  info.GetReturnValue().Set(Nan::New<Number>(duration));
}

NAN_GETTER(SIPSTERCall::HasMediaGetter)
{
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  info.GetReturnValue().Set(Nan::New(call->hasMedia()));
}

NAN_GETTER(SIPSTERCall::IsActiveGetter)
{
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  info.GetReturnValue().Set(Nan::New(call->isActive()));
}

NAN_GETTER(SIPSTERCall::CallInfoGetter)
{
  SIPSTERCall *call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(info.This());

  CallInfo ci;

  try
  {
    ci = call->getInfo();
  }
  catch (Error &err)
  {
    string errstr = "Call.getInfo() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  Local<Object> callInfo = Nan::New<Object>();
  Nan::Set(callInfo,
           Nan::New("srcAddress").ToLocalChecked(),
           Nan::New("localhost").ToLocalChecked());
  Nan::Set(callInfo,
           Nan::New("localUri").ToLocalChecked(),
           Nan::New(ci.localUri.c_str()).ToLocalChecked());
  Nan::Set(callInfo,
           Nan::New("remoteUri").ToLocalChecked(),
           Nan::New(ci.remoteUri.c_str()).ToLocalChecked());
  Nan::Set(callInfo,
           Nan::New("localContact").ToLocalChecked(),
           Nan::New(ci.localContact.c_str()).ToLocalChecked());
  Nan::Set(callInfo,
           Nan::New("remoteContact").ToLocalChecked(),
           Nan::New(ci.remoteContact.c_str()).ToLocalChecked());
  Nan::Set(callInfo,
           Nan::New("callId").ToLocalChecked(),
           Nan::New(ci.callIdString.c_str()).ToLocalChecked());

  info.GetReturnValue().Set(callInfo);
}

void SIPSTERCall::Initialize(Handle<Object> target)
{
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  Local<String> name = Nan::New("Call").ToLocalChecked();

  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(name);

  Nan::SetPrototypeMethod(tpl, "answer", Answer);
  Nan::SetPrototypeMethod(tpl, "hangup", Hangup);
  Nan::SetPrototypeMethod(tpl, "hold", SetHold);
  Nan::SetPrototypeMethod(tpl, "reinvite", Reinvite);
  Nan::SetPrototypeMethod(tpl, "update", Update);
  Nan::SetPrototypeMethod(tpl, "dtmf", DialDtmf);
  Nan::SetPrototypeMethod(tpl, "transfer", Transfer);
  Nan::SetPrototypeMethod(tpl, "ref", DoRef);
  Nan::SetPrototypeMethod(tpl, "unref", DoUnref);
  Nan::SetPrototypeMethod(tpl, "getStatsDump", GetStats);

  Nan::SetPrototypeMethod(tpl, "play", PlayMusic);

  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("connDuration").ToLocalChecked(),
                   ConDurationGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("totalDuration").ToLocalChecked(),
                   TotDurationGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("hasMedia").ToLocalChecked(),
                   HasMediaGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("isActive").ToLocalChecked(),
                   IsActiveGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("callInfo").ToLocalChecked(),
                   CallInfoGetter);

  Nan::Set(target, name, tpl->GetFunction());

  SIPSTERCall_constructor.Reset(tpl);
}

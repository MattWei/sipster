#include "SIPSTERMedia.h"
#include "common.h"

#include <iostream>
#include "hplayer.hpp"

Nan::Persistent<FunctionTemplate> SIPSTERMedia_constructor;

static Nan::Persistent<String> media_dir_none_symbol;
static Nan::Persistent<String> media_dir_outbound_symbol;
static Nan::Persistent<String> media_dir_inbound_symbol;
static Nan::Persistent<String> media_dir_bidi_symbol;
static Nan::Persistent<String> media_dir_unknown_symbol;

static Nan::Persistent<String> media_status_none_symbol;
static Nan::Persistent<String> media_status_active_symbol;
static Nan::Persistent<String> media_status_local_hold_symbol;
static Nan::Persistent<String> media_status_remote_hold_symbol;
static Nan::Persistent<String> media_status_error_symbol;
static Nan::Persistent<String> media_status_unknown_symbol;

SIPSTERMedia::SIPSTERMedia() : emit(NULL), media(NULL), is_media_new(false) {}
SIPSTERMedia::~SIPSTERMedia()
{
  std::cout << "SIPSTERMedia::~SIPSTERMedia()" << std::endl;
  if (media && is_media_new)
  {
    delete media;
  }
  if (emit)
  {
    delete emit;
  }
  is_media_new = false;
}

NAN_METHOD(SIPSTERMedia::New)
{
  Nan::HandleScope scope;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Use `new` to create instances of this object.");

  SIPSTERMedia *med = new SIPSTERMedia();

  med->Wrap(info.This());

  med->emit = new Nan::Callback(
      Local<Function>::Cast(med->handle()->Get(Nan::New(emit_symbol))));

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(SIPSTERMedia::StartTransmit)
{
  Nan::HandleScope scope;
  SIPSTERMedia *src = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (info.Length() == 0 || !Nan::New(SIPSTERMedia_constructor)->HasInstance(info[0]))
    return Nan::ThrowTypeError("Expected Media object");

  SIPSTERMedia *dest =
      Nan::ObjectWrap::Unwrap<SIPSTERMedia>(Local<Object>::Cast(info[0]));

  if (!src->media)
    return Nan::ThrowError("Invalid source");
  else if (!dest->media)
    return Nan::ThrowError("Invalid destination");

  try
  {
    src->media->startTransmit(*dest->media);
  }
  catch (Error &err)
  {
    string errstr = "AudioMedia.startTransmit() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::StopTransmit)
{
  Nan::HandleScope scope;
  SIPSTERMedia *src = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (info.Length() == 0 || !Nan::New(SIPSTERMedia_constructor)->HasInstance(info[0]))
    return Nan::ThrowTypeError("Expected Media object");

  SIPSTERMedia *dest =
      Nan::ObjectWrap::Unwrap<SIPSTERMedia>(Local<Object>::Cast(info[0]));

  if (!src->media)
    return Nan::ThrowError("Invalid source");
  else if (!dest->media)
    return Nan::ThrowError("Invalid destination");

  try
  {
    src->media->stopTransmit(*dest->media);
  }
  catch (Error &err)
  {
    string errstr = "AudioMedia.stopTransmit() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::AdjustRxLevel)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    if (info.Length() > 0 && info[0]->IsNumber())
    {
      try
      {
        med->media->adjustRxLevel(static_cast<float>(info[0]->NumberValue()));
      }
      catch (Error &err)
      {
        string errstr = "AudioMedia.adjustRxLevel() error: " + err.info();
        return Nan::ThrowError(errstr.c_str());
      }
    }
    else
      return Nan::ThrowTypeError("Missing signal level");
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::AdjustTxLevel)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    if (info.Length() > 0 && info[0]->IsNumber())
    {
      try
      {
        med->media->adjustTxLevel(static_cast<float>(info[0]->NumberValue()));
      }
      catch (Error &err)
      {
        string errstr = "AudioMedia.adjustTxLevel() error: " + err.info();
        return Nan::ThrowError(errstr.c_str());
      }
    }
    else
      return Nan::ThrowTypeError("Missing signal level");
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::Close)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media && med->is_media_new)
  {
    delete med->media;
    med->media = NULL;
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::PlaySong)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    if (info.Length() > 0 && info[0]->IsString())
    {
      Nan::Utf8String song_str(info[0]);
      string song = string(*song_str);

      std::cout << "SIPSTERMedia::PlaySong:" << song << std::endl;
      ((HPlayer *)(med->media))->play(song);
    }
    else
      return Nan::ThrowTypeError("Missing song path");
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::StartLocalPlay)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    AudioMedia &play_med = Endpoint::instance().audDevManager().getPlaybackDevMedia();
    med->media->startTransmit(play_med);
  }
  else
    return Nan::ThrowTypeError("Missing song path");

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::StopLocalPlay)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    AudioMedia &play_med = Endpoint::instance().audDevManager().getPlaybackDevMedia();
    med->media->stopTransmit(play_med);

    delete med->media;
    med->media = NULL;
    med->is_media_new = false;
  }
  else
    return Nan::ThrowTypeError("Player not started");

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::StartLocalRecord)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    AudioMedia &cap_med = Endpoint::instance().audDevManager().getCaptureDevMedia();
    cap_med.startTransmit(*med->media);
  }
  else
    return Nan::ThrowTypeError("Missing Recorder");

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERMedia::StopLocalRecord)
{
  Nan::HandleScope scope;
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  if (med->media)
  {
    AudioMedia &cap_med = Endpoint::instance().audDevManager().getCaptureDevMedia();
    cap_med.stopTransmit(*med->media);

    delete med->media;
    med->media = NULL;
    med->is_media_new = false;
  }

  info.GetReturnValue().SetUndefined();
}

NAN_GETTER(SIPSTERMedia::RxLevelGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  unsigned level = 0;

  if (med->media)
  {
    try
    {
      level = med->media->getRxLevel();
    }
    catch (Error &err)
    {
      string errstr = "AudioMedia.getRxLevel() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }

  info.GetReturnValue().Set(Nan::New(level));
}

NAN_GETTER(SIPSTERMedia::TxLevelGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  unsigned level = 0;

  if (med->media)
  {
    try
    {
      level = med->media->getTxLevel();
    }
    catch (Error &err)
    {
      string errstr = "AudioMedia.getTxLevel() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }

  info.GetReturnValue().Set(Nan::New(level));
}

NAN_GETTER(SIPSTERMedia::DirGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());
  Local<String> str;
  switch (med->dir)
  {
  case PJMEDIA_DIR_NONE:
    str = Nan::New(media_dir_none_symbol);
    break;
  case PJMEDIA_DIR_ENCODING:
    str = Nan::New(media_dir_outbound_symbol);
    break;
  case PJMEDIA_DIR_DECODING:
    str = Nan::New(media_dir_inbound_symbol);
    break;
  case PJMEDIA_DIR_ENCODING_DECODING:
    str = Nan::New(media_dir_bidi_symbol);
    break;
  default:
    str = Nan::New(media_dir_unknown_symbol);
  }
  info.GetReturnValue().Set(str);
}

NAN_GETTER(SIPSTERMedia::StatusGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());
  std::cout << "#############Media status getter:" << med->status << std::endl;

  Local<String> str;
  switch (med->status)
  {
  case PJSUA_CALL_MEDIA_NONE:
    str = Nan::New(media_status_none_symbol);
    break;
  case PJSUA_CALL_MEDIA_ACTIVE:
    str = Nan::New(media_status_active_symbol);
    break;
  case PJSUA_CALL_MEDIA_LOCAL_HOLD:
    str = Nan::New(media_status_local_hold_symbol);
    break;
  case PJSUA_CALL_MEDIA_REMOTE_HOLD:
    str = Nan::New(media_status_remote_hold_symbol);
    break;
  case PJSUA_CALL_MEDIA_ERROR:
    str = Nan::New(media_status_error_symbol);
  default:
    str = Nan::New(media_status_unknown_symbol);
  }
  info.GetReturnValue().Set(str);
}

NAN_GETTER(SIPSTERMedia::SrcRTPGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  info.GetReturnValue().Set(Nan::New(med->srcRTP.c_str()).ToLocalChecked());
}

NAN_GETTER(SIPSTERMedia::SrcRTCPGetter)
{
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(info.This());

  info.GetReturnValue().Set(Nan::New(med->srcRTCP.c_str()).ToLocalChecked());
}

void SIPSTERMedia::Initialize(Handle<Object> target)
{
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  Local<String> name = Nan::New("Media").ToLocalChecked();

  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(name);

  media_dir_none_symbol.Reset(Nan::New("none").ToLocalChecked());
  media_dir_outbound_symbol.Reset(Nan::New("outbound").ToLocalChecked());
  media_dir_inbound_symbol.Reset(Nan::New("inbound").ToLocalChecked());
  media_dir_bidi_symbol.Reset(Nan::New("bidirectional").ToLocalChecked());
  media_dir_unknown_symbol.Reset(Nan::New("unknown").ToLocalChecked());

  media_status_none_symbol.Reset(Nan::New("none").ToLocalChecked());
  media_status_active_symbol.Reset(Nan::New("active").ToLocalChecked());
  media_status_local_hold_symbol.Reset(Nan::New("localHold").ToLocalChecked());
  media_status_remote_hold_symbol.Reset(Nan::New("remoteHold").ToLocalChecked());
  media_status_error_symbol.Reset(Nan::New("error").ToLocalChecked());
  media_status_unknown_symbol.Reset(Nan::New("unknown").ToLocalChecked());

  Nan::SetPrototypeMethod(tpl, "startTransmitTo", StartTransmit);
  Nan::SetPrototypeMethod(tpl, "stopTransmitTo", StopTransmit);
  Nan::SetPrototypeMethod(tpl, "adjustRxLevel", AdjustRxLevel);
  Nan::SetPrototypeMethod(tpl, "adjustTxLevel", AdjustTxLevel);
  Nan::SetPrototypeMethod(tpl, "close", Close);
  Nan::SetPrototypeMethod(tpl, "playSong", PlaySong);

  Nan::SetPrototypeMethod(tpl, "startLocalPlay", StartLocalPlay);
  Nan::SetPrototypeMethod(tpl, "stopLocalPlay", StopLocalPlay);
  Nan::SetPrototypeMethod(tpl, "startLocalRecord", StartLocalRecord);
  Nan::SetPrototypeMethod(tpl, "stopLocalRecord", StopLocalRecord);

  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("dir").ToLocalChecked(),
                   DirGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("status").ToLocalChecked(),
                   StatusGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("rtpAddr").ToLocalChecked(),
                   SrcRTPGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("rtcpAddr").ToLocalChecked(),
                   SrcRTCPGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("rxLevel").ToLocalChecked(),
                   RxLevelGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("txLevel").ToLocalChecked(),
                   TxLevelGetter);

  Nan::Set(target, name, tpl->GetFunction());

  SIPSTERMedia_constructor.Reset(tpl);
}

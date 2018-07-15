#include <node.h>
#include <nan.h>
#include <string.h>
//#include <strings.h>
//#include <regex.h>
#include <pjsua2.hpp>
#include <iostream>
#include <locale>
#include <codecvt>
#include <cstdint>

#include "common.h"
#include "SIPSTERTransport.h"
//#include "hplayer.hpp"

#include "SIPSTERLocalMedia.h"
//#include "SIPSTERAudioDevInfo.h"

#include "SIPSTERPlatform.h"

#define X(kind, ctype, name, v8type, valconv) \
  Nan::Persistent<String> kind##_##name##_symbol;
INCALL_FIELDS
CALLDTMF_FIELDS
REGSTATE_FIELDS
REGSTARTING_FIELDS
INSTANTMESSAGE_FIELDS
PLAYERSTATUS_FIELDS
BUDDYSTATUS_FIELDS
SYSTEMSTATUS_FIELDS
#undef X

#define X(kind, literal) \
  Nan::Persistent<String> ev_##kind##_##literal##_symbol;
EVENT_SYMBOLS
#undef X

using namespace std;
using namespace node;
using namespace v8;
using namespace pj;

class SIPSTERLogWriter;

SIPSTERLogWriter *logger = NULL;
//SIPSTERLocalMedia *localMedia = NULL;

SIPSTERPlatform *sipPlatform = NULL;

struct CustomLogEntry
{
  int level;
  string msg;
  double threadId;
  string threadName;
};
list<CustomLogEntry> log_queue;
uv_mutex_t log_mutex;
uv_async_t logging;
bool ep_init = false;
bool ep_create = false;
bool ep_start = false;
EpConfig ep_cfg;

list<SIPEventInfo> event_queue;
uv_mutex_t event_mutex;
uv_mutex_t async_mutex;
uv_async_t dumb;
Nan::Persistent<String> emit_symbol;
Endpoint *ep = new Endpoint;

bool system_init = false;
uv_mutex_t local_media_mutex;

// DTMF event ==================================================================
Nan::Persistent<String> CALLDTMF_DTMF0_symbol;
Nan::Persistent<String> CALLDTMF_DTMF1_symbol;
Nan::Persistent<String> CALLDTMF_DTMF2_symbol;
Nan::Persistent<String> CALLDTMF_DTMF3_symbol;
Nan::Persistent<String> CALLDTMF_DTMF4_symbol;
Nan::Persistent<String> CALLDTMF_DTMF5_symbol;
Nan::Persistent<String> CALLDTMF_DTMF6_symbol;
Nan::Persistent<String> CALLDTMF_DTMF7_symbol;
Nan::Persistent<String> CALLDTMF_DTMF8_symbol;
Nan::Persistent<String> CALLDTMF_DTMF9_symbol;
Nan::Persistent<String> CALLDTMF_DTMFSTAR_symbol;
Nan::Persistent<String> CALLDTMF_DTMFPOUND_symbol;
Nan::Persistent<String> CALLDTMF_DTMFA_symbol;
Nan::Persistent<String> CALLDTMF_DTMFB_symbol;
Nan::Persistent<String> CALLDTMF_DTMFC_symbol;
Nan::Persistent<String> CALLDTMF_DTMFD_symbol;
// =============================================================================

class SIPSTERPlayer : public AudioMediaPlayer
{
public:
  SIPSTERMedia *media;
  unsigned options;
  bool skip;

  SIPSTERPlayer() : skip(false) {}
  ~SIPSTERPlayer() {}

  virtual bool onEof()
  {
    if (skip)
      return false;
    SETUP_EVENT_NOARGS(PLAYEREOF);
    ev.media = media;

    ENQUEUE_EVENT(ev);
    if (options & PJMEDIA_FILE_NO_LOOP)
    {
      skip = true;
      return false;
    }
    return true;
  }
};

class SIPSTERLogWriter : public LogWriter
{
public:
  Nan::Callback *func;

  SIPSTERLogWriter() {}
  ~SIPSTERLogWriter()
  {
    if (func)
      delete func;
  }

  virtual void write(const LogEntry &entry)
  {
    CustomLogEntry log;
    log.level = entry.level;
    log.msg = entry.msg;
    log.threadId = static_cast<double>(entry.threadId);
    log.threadName = entry.threadName;

    uv_mutex_lock(&log_mutex);
    log_queue.push_back(log);
    uv_mutex_unlock(&log_mutex);
    uv_async_send(&logging);
  }
};

// start event processing-related definitions ==================================
#if NAUV_UVVERSION < 0x000b17
void dumb_cb(uv_async_t *handle, int status)
{
  assert(status == 0);
#else
void dumb_cb(uv_async_t *handle)
{
#endif
  Nan::HandleScope scope;
  while (true)
  {
    uv_mutex_lock(&event_mutex);
    if (event_queue.empty())
      break;
    const SIPEventInfo ev = event_queue.front();
    event_queue.pop_front();
    uv_mutex_unlock(&event_mutex);

    switch (ev.type)
    {
    case EVENT_INCALL:
    {
      Local<Object> obj = Nan::New<Object>();
      EV_ARGS_INCALL *args = reinterpret_cast<EV_ARGS_INCALL *>(ev.args);
#define X(kind, ctype, name, v8type, valconv) \
  Nan::Set(obj,                               \
           Nan::New(kind##_##name##_symbol),  \
           Nan::New<v8type>(args->valconv).ToLocalChecked());
      INCALL_FIELDS
#undef X
      SIPSTERAccount *acct = ev.acct;
      SIPSTERCall *call = ev.call;
      Local<Value> new_call_args[1] = {Nan::New<External>(call)};
      Local<Object> call_obj;
      call_obj = Nan::New(SIPSTERCall_constructor)
                     ->GetFunction()
                     ->NewInstance(1, new_call_args);
      Local<Value> emit_argv[3] = {
          Nan::New(ev_INCALL_call_symbol),
          obj,
          call_obj};
      call->emit->Call(acct->handle(), 3, emit_argv);
      delete args;
    }
    break;
    case EVENT_REGSTATE:
    {
      EV_ARGS_REGSTATE *args = reinterpret_cast<EV_ARGS_REGSTATE *>(ev.args);
      SIPSTERAccount *acct = ev.acct;
      Local<Value> emit_argv[1] = {
          args->active
              ? Nan::New(ev_REGSTATE_registered_symbol)
              : Nan::New(ev_REGSTATE_unregistered_symbol)};
      acct->emit->Call(acct->handle(), 1, emit_argv);
      Local<Value> emit_catchall_argv[N_REGSTATE_FIELDS + 1] = {
          Nan::New(ev_CALLSTATE_state_symbol),
#define X(kind, ctype, name, v8type, valconv) \
  Nan::New<v8type>(args->valconv),
          REGSTATE_FIELDS
#undef X
      };
      acct->emit->Call(acct->handle(),
                       N_REGSTATE_FIELDS + 1,
                       emit_catchall_argv);
      delete args;
    }
    break;
    case EVENT_CALLSTATE:
    {
      EV_ARGS_CALLSTATE *args = reinterpret_cast<EV_ARGS_CALLSTATE *>(ev.args);
      Local<Value> ev_name;

      switch (args->_state)
      {
      case PJSIP_INV_STATE_CALLING:
        ev_name = Nan::New(ev_CALLSTATE_calling_symbol);
        break;
      case PJSIP_INV_STATE_INCOMING:
        ev_name = Nan::New(ev_CALLSTATE_incoming_symbol);
        break;
      case PJSIP_INV_STATE_EARLY:
        ev_name = Nan::New(ev_CALLSTATE_early_symbol);
        break;
      case PJSIP_INV_STATE_CONNECTING:
        ev_name = Nan::New(ev_CALLSTATE_connecting_symbol);
        break;
      case PJSIP_INV_STATE_CONFIRMED:
        //std::cout << "EVENT_CALLSTATE:" << args->_state << "," << PJSIP_INV_STATE_CONFIRMED << std::endl;
        ev_name = Nan::New(ev_CALLSTATE_confirmed_symbol);
        break;
      case PJSIP_INV_STATE_DISCONNECTED:
        //std::cout << "EVENT_CALLSTATE:" << args->_state << "," << PJSIP_INV_STATE_DISCONNECTED << std::endl;
        ev_name = Nan::New(ev_CALLSTATE_disconnected_symbol);
        break;
      default:
        break;
      }
      if (!ev_name.IsEmpty())
      {
        SIPSTERCall *call = ev.call;
        Local<Value> emit_argv[1] = {ev_name};

        if (call->emit && !call->persistent().IsEmpty())
        {
          call->emit->Call(call->handle(), 1, emit_argv);
          Local<Value> emit_catchall_argv[3] = {
              Nan::New(ev_CALLSTATE_state_symbol),
              ev_name,
              Nan::New(args->_lastStatuscode)};
          call->emit->Call(call->handle(), 3, emit_catchall_argv);
        }
      }
      delete args;
    }
    break;
    case EVENT_CALLMEDIA:
    {
      SIPSTERCall *call = ev.call;

      Local<Array> medias = Nan::New<Array>();
      CallInfo ci;
      try
      {
        ci = call->getInfo();
        AudioMedia *media = NULL;
        // TODO: update srcRTP/srcRTCP on call CONFIRMED status?
        for (unsigned i = 0, m = 0; i < ci.media.size(); ++i)
        {
          if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && (media = static_cast<AudioMedia *>(call->getMedia(i))))
          {
            Local<Object> med_obj;
            med_obj = Nan::New(SIPSTERMedia_constructor)
                          ->GetFunction()
                          ->NewInstance(0, NULL);
            SIPSTERMedia *med =
                Nan::ObjectWrap::Unwrap<SIPSTERMedia>(med_obj);
            med->media = media;
            med->dir = ci.media[i].dir;
            med->status = ci.media[i].status;

            std::cout << "media " << i << " status " << ci.media[i].status << std::endl;

            /*if (ci.media[i].status == PJSUA_CALL_MEDIA_ACTIVE) {
                try {
                  MediaTransportInfo mti = call->getMedTransportInfo(i);
                  med->srcRTP = mti.srcRtpName;
                  med->srcRTCP = mti.srcRtcpName;
                } catch (Error& err) {}
              }*/
            Nan::Set(medias, m++, med_obj);
          }
        }
        if (medias->Length() > 0)
        {
          Local<Value> emit_argv[2] = {
              Nan::New(ev_CALLMEDIA_media_symbol),
              medias};
          if (call->emit && !call->persistent().IsEmpty())
            call->emit->Call(call->handle(), 2, emit_argv);
        }
      }
      catch (Error &err)
      {
      }
    }
    break;
    case EVENT_CALLDTMF:
    {
      EV_ARGS_CALLDTMF *args = reinterpret_cast<EV_ARGS_CALLDTMF *>(ev.args);
      SIPSTERCall *call = ev.call;
      Local<Value> dtmf_char;
      switch (args->digit)
      {
      case '0':
        dtmf_char = Nan::New(CALLDTMF_DTMF0_symbol);
        break;
      case '1':
        dtmf_char = Nan::New(CALLDTMF_DTMF1_symbol);
        break;
      case '2':
        dtmf_char = Nan::New(CALLDTMF_DTMF2_symbol);
        break;
      case '3':
        dtmf_char = Nan::New(CALLDTMF_DTMF3_symbol);
        break;
      case '4':
        dtmf_char = Nan::New(CALLDTMF_DTMF4_symbol);
        break;
      case '5':
        dtmf_char = Nan::New(CALLDTMF_DTMF5_symbol);
        break;
      case '6':
        dtmf_char = Nan::New(CALLDTMF_DTMF6_symbol);
        break;
      case '7':
        dtmf_char = Nan::New(CALLDTMF_DTMF7_symbol);
        break;
      case '8':
        dtmf_char = Nan::New(CALLDTMF_DTMF8_symbol);
        break;
      case '9':
        dtmf_char = Nan::New(CALLDTMF_DTMF9_symbol);
        break;
      case '*':
        dtmf_char = Nan::New(CALLDTMF_DTMFSTAR_symbol);
        break;
      case '#':
        dtmf_char = Nan::New(CALLDTMF_DTMFPOUND_symbol);
        break;
      case 'A':
        dtmf_char = Nan::New(CALLDTMF_DTMFA_symbol);
        break;
      case 'B':
        dtmf_char = Nan::New(CALLDTMF_DTMFB_symbol);
        break;
      case 'C':
        dtmf_char = Nan::New(CALLDTMF_DTMFC_symbol);
        break;
      case 'D':
        dtmf_char = Nan::New(CALLDTMF_DTMFD_symbol);
        break;
      default:
        const char digit[1] = {args->digit};
        dtmf_char = Nan::New(digit, 1).ToLocalChecked();
      }
      Local<Value> emit_argv[2] = {
          Nan::New(ev_CALLDTMF_dtmf_symbol),
          dtmf_char};
      if (call->emit && !call->persistent().IsEmpty())
        call->emit->Call(call->handle(), 2, emit_argv);
      delete args;
    }
    break;
    case EVENT_PLAYEREOF:
    {
      SIPSTERMedia *media = ev.media;
      Local<Value> emit_argv[1] = {
          Nan::New(ev_PLAYEREOF_eof_symbol)};
      if (media->emit && !media->persistent().IsEmpty())
        media->emit->Call(media->handle(), 1, emit_argv);
    }
    break;
    case EVENT_REGSTARTING:
    {
      EV_ARGS_REGSTARTING *args =
          reinterpret_cast<EV_ARGS_REGSTARTING *>(ev.args);
      SIPSTERAccount *acct = ev.acct;

      Local<Value> emit_argv[1] = {
          (args->renew
               ? Nan::New(ev_REGSTARTING_registering_symbol)
               : Nan::New(ev_REGSTARTING_unregistering_symbol))};
      acct->emit->Call(acct->handle(), 1, emit_argv);
      delete args;
    }
    break;
    case EVENT_INSTANTMESSAGE:
    {
      std::cout << "dumb_cb EVENT_INSTANTMESSAGE" << std::endl;

      EV_ARGS_INSTANTMESSAGE *args =
          reinterpret_cast<EV_ARGS_INSTANTMESSAGE *>(ev.args);
      SIPSTERAccount *acct = ev.acct;

      Local<Value> emit_argv[N_INSTANTMESSAGE_FIELDS] = {
          Nan::New(ev_INSTANTMESSAGE_instantMessage_symbol),
#define X(kind, ctype, name, v8type, valconv) \
  Nan::New<v8type>(args->valconv).ToLocalChecked(),
          INSTANTMESSAGE_FIELDS
#undef X
      };

      //std::cout << "dumb_cb EVENT_INSTANTMESSAGE, field size:"
      //          << N_INSTANTMESSAGE_FIELDS << "," << args->fromUri << "," << args->msg << std::endl;
      acct->emit->Call(acct->handle(), N_INSTANTMESSAGE_FIELDS, emit_argv);
      delete args;
    }
    break;
    case EVENT_PLAYERSTATUS:
    {
      //std::cout << "dumb_cb EVENT_PLAYERSTATUS" << std::endl;

      EV_ARGS_PLAYERSTATUS *args =
          reinterpret_cast<EV_ARGS_PLAYERSTATUS *>(ev.args);
      SIPSTERCall *call = ev.call;

      Local<Value> emit_argv[N_PLAYERSTATUS_FIELDS] = {
          Nan::New(ev_PLAYERSTATUS_playerStatus_symbol),
          Nan::New(args->songPath.c_str()).ToLocalChecked(),
          Nan::New(args->type),
          Nan::New(args->param)};

      std::cout << "dumb_cb EVENT_PLAYERSTATUS, field size:"
                << N_PLAYERSTATUS_FIELDS << "," << args->songPath
                << "," << args->type << "," << args->param << std::endl;
      if (call && call->emit)
      {
        call->emit->Call(call->handle(), N_PLAYERSTATUS_FIELDS, emit_argv);
      }
      else
      {
        std::cout << "media emit fail" << std::endl;
      }

      delete args;
    }
    break;
    case EVENT_BUDDYSTATUS:
    {
      std::cout << "dumb_cb EVENT_BUDDYSTATUS" << std::endl;

      EV_ARGS_BUDDYSTATUS *args =
          reinterpret_cast<EV_ARGS_BUDDYSTATUS *>(ev.args);
      SIPSTERBuddy *buddy = ev.buddy;

      Local<Value> emit_argv[N_BUDDYSTATUS_FIELDS] = {
          Nan::New(ev_BUDDYSTATUS_buddyStatus_symbol),
          Nan::New(args->uri.c_str()).ToLocalChecked(),
          Nan::New(args->statusText.c_str()).ToLocalChecked()};

      //std::cout << "dumb_cb EVENT_BUDDYSTATUS, field size:"
      //          << N_BUDDYSTATUS_FIELDS << "," << args->uri
      //          << "," << args->statusText << std::endl;
      if (buddy && buddy->emit)
      {
        buddy->emit->Call(buddy->handle(), N_BUDDYSTATUS_FIELDS, emit_argv);
      }
      else
      {
        std::cout << "buddy emit fail" << std::endl;
      }

      delete args;
    }
    break;
    case EVENT_SYSTEMSTATUS:
    {
      std::cout << "dumb_cb EVENT_SYSTEMSTATUS" << std::endl;

      EV_ARGS_SYSTEMSTATUS *args =
          reinterpret_cast<EV_ARGS_SYSTEMSTATUS *>(ev.args);

      SIPSTERPlatform *system = ev.system;

      Local<Value> emit_argv[N_SYSTEMSTATUS_FIELDS] = {
          Nan::New(ev_SYSTEMSTATUS_systemStatus_symbol),
          Nan::New(args->state.c_str()).ToLocalChecked()};

      std::cout << "dumb_cb EVENT_SYSTEMSTATUS, field size:"
                << N_SYSTEMSTATUS_FIELDS << "," << args->state << std::endl;
      if (system && system->emit)
      {
        system->emit->Call(system->handle(), N_SYSTEMSTATUS_FIELDS, emit_argv);
      }

      delete args;
    }
    break;
    default:
      std::cout << "unknown event " << ev.type << std::endl;
      break;
    }
  }
  uv_mutex_unlock(&event_mutex);
}

void logging_close_cb(uv_handle_t *handle) {}

#if NAUV_UVVERSION < 0x000b17
void logging_cb(uv_async_t *handle, int status)
{
  assert(status == 0);
#else
void logging_cb(uv_async_t *handle)
{
#endif
  Nan::HandleScope scope;
  Local<Value> log_argv[4];
  while (true)
  {
    uv_mutex_lock(&log_mutex);

    if (log_queue.empty())
    {
      uv_mutex_unlock(&log_mutex);
      break;
    }

    const CustomLogEntry log = log_queue.front();
    log_queue.pop_front();
    uv_mutex_unlock(&log_mutex);

    log_argv[0] = Nan::New<Integer>(log.level);
    log_argv[1] = Nan::New<String>(log.msg).ToLocalChecked();
    log_argv[2] = Nan::New<Number>(log.threadId);
    log_argv[3] = Nan::New<String>(log.threadName).ToLocalChecked();

    logger->func->Call(4, log_argv);
  }
}
// =============================================================================

// static methods ==============================================================
static NAN_METHOD(CreateRecorder)
{
  Nan::HandleScope scope;

  string dest;
  unsigned fmt = PJMEDIA_FILE_WRITE_PCM;
  pj_ssize_t max_size = 0;
  if (info.Length() > 0 && info[0]->IsString())
  {
    Nan::Utf8String dest_str(info[0]);
    dest = string(*dest_str);
    if (info.Length() > 1 && info[1]->IsString())
    {
      Nan::Utf8String fmt_str(info[1]);
      const char *fmtcstr = *fmt_str;
      if (strcasecmp(fmtcstr, "pcm") == 0)
        fmt = PJMEDIA_FILE_WRITE_PCM;
      else if (strcasecmp(fmtcstr, "alaw") == 0)
        fmt = PJMEDIA_FILE_WRITE_ALAW;
      else if (strcasecmp(fmtcstr, "ulaw") != 0)
        return Nan::ThrowError("Invalid media format");
    }
    if (info.Length() > 2 && info[2]->IsInt32())
    {
      pj_ssize_t size = static_cast<pj_ssize_t>(info[2]->Int32Value());
      if (size >= -1)
        max_size = size;
    }
  }
  else
    return Nan::ThrowTypeError("Missing destination filename");

  AudioMediaRecorder *recorder = new AudioMediaRecorder();
  try
  {
    recorder->createRecorder(dest, 0, max_size, fmt);
  }
  catch (Error &err)
  {
    delete recorder;
    string errstr = "recorder->createRecorder() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  Local<Object> med_obj;
  med_obj = Nan::New(SIPSTERMedia_constructor)
                ->GetFunction()
                ->NewInstance(0, NULL);
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(med_obj);
  med->media = recorder;
  med->is_media_new = true;

  info.GetReturnValue().Set(med_obj);
}

static NAN_METHOD(CreatePlayer)
{
#if 0
  Nan::HandleScope scope;

  HPlayer *player = new HPlayer();
  Local<Object> med_obj;
  med_obj = Nan::New(SIPSTERMedia_constructor)
                ->GetFunction()
                ->NewInstance(0, NULL);
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(med_obj);
  med->media = player;
  med->is_media_new = true;
  player->media = med;

  info.GetReturnValue().Set(med_obj);
#endif
  Nan::HandleScope scope;

  string filename;
  if (info.Length() > 0 && info[0]->IsString())
  {
    Nan::Utf8String dest_str(info[0]);
    filename = string(*dest_str);
  }
  else
    return Nan::ThrowTypeError("Missing source filename");

  SIPSTERPlayer *player = new SIPSTERPlayer();
  try
  {
    player->createPlayer(filename, PJMEDIA_FILE_NO_LOOP);
  }
  catch (Error &err)
  {
    delete player;
    string errstr = "player->createPlayer() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  Local<Object> med_obj;
  med_obj = Nan::New(SIPSTERMedia_constructor)
                ->GetFunction()
                ->NewInstance(0, NULL);
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(med_obj);
  med->media = player;
  med->is_media_new = true;
  player->media = med;
  player->options = PJMEDIA_FILE_NO_LOOP;

  info.GetReturnValue().Set(med_obj);
}

static NAN_METHOD(CreatePlaylist)
{
  Nan::HandleScope scope;

  unsigned opts = 0;
  vector<string> playlist;
  if (info.Length() > 0 && info[0]->IsArray())
  {
    const Local<Array> arr_obj = Local<Array>::Cast(info[0]);
    const uint32_t arr_length = arr_obj->Length();

    if (arr_length == 0)
      return Nan::ThrowError("Nothing to add to playlist");

    playlist.reserve(arr_length);
    for (uint32_t i = 0; i < arr_length; ++i)
    {
      Nan::Utf8String filename_str(arr_obj->Get(i));
      playlist.push_back(string(*filename_str));
    }
    if (info.Length() > 1 && info[1]->IsBoolean() && info[1]->BooleanValue())
      opts = PJMEDIA_FILE_NO_LOOP;
  }
  else
    return Nan::ThrowTypeError("Missing source filenames");

  SIPSTERPlayer *player = new SIPSTERPlayer();
  try
  {
    player->createPlaylist(playlist, "", opts);
  }
  catch (Error &err)
  {
    delete player;
    string errstr = "player->createPlayer() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  Local<Object> med_obj;
  med_obj = Nan::New(SIPSTERMedia_constructor)
                ->GetFunction()
                ->NewInstance(0, NULL);
  SIPSTERMedia *med = Nan::ObjectWrap::Unwrap<SIPSTERMedia>(med_obj);
  med->media = player;
  med->is_media_new = true;
  player->media = med;
  player->options = opts;

  info.GetReturnValue().Set(med_obj);
}

static NAN_METHOD(EPVersion)
{
  Nan::HandleScope scope;

  pj::Version v = ep->libVersion();
  Local<Object> vinfo = Nan::New<Object>();
  Nan::Set(vinfo,
           Nan::New("major").ToLocalChecked(),
           Nan::New(v.major));
  Nan::Set(vinfo,
           Nan::New("minor").ToLocalChecked(),
           Nan::New(v.minor));
  Nan::Set(vinfo,
           Nan::New("rev").ToLocalChecked(),
           Nan::New(v.rev));
  Nan::Set(vinfo,
           Nan::New("suffix").ToLocalChecked(),
           Nan::New(v.suffix.c_str()).ToLocalChecked());
  Nan::Set(vinfo,
           Nan::New("full").ToLocalChecked(),
           Nan::New(v.full.c_str()).ToLocalChecked());
  Nan::Set(vinfo,
           Nan::New("numeric").ToLocalChecked(),
           Nan::New(v.numeric));

  info.GetReturnValue().Set(vinfo);
}

static NAN_METHOD(SystemInit)
{
  Nan::HandleScope scope;
  //if (sipPlatform != NULL)
  //  return Nan:ThrowError("Already initialized system");
  Local<Object> platform_obj;
  platform_obj = Nan::New(SIPSTERPlatform_constructor)
                     ->GetFunction()
                     ->NewInstance(0, NULL);
  sipPlatform = Nan::ObjectWrap::Unwrap<SIPSTERPlatform>(platform_obj);

  uv_async_init(uv_default_loop(), &dumb, static_cast<uv_async_cb>(dumb_cb));

  info.GetReturnValue().Set(platform_obj);
}

static NAN_METHOD(EPInit)
{
  Nan::HandleScope scope;
  string errstr;

  if (ep_init)
    return Nan::ThrowError("Already initialized");

  if (!ep_create)
  {
    try
    {
      ep->libCreate();
      ep_create = true;
    }
    catch (Error &err)
    {
      errstr = "libCreate error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }

  Local<Value> val;
  if (info.Length() > 0 && info[0]->IsObject())
  {
    Local<Object> cfg_obj = info[0]->ToObject();
    val = cfg_obj->Get(Nan::New("uaConfig").ToLocalChecked());
    if (val->IsObject())
    {
      UaConfig uaConfig;
      Local<Object> ua_obj = val->ToObject();
      JS2PJ_UINT(ua_obj, maxCalls, uaConfig);
      JS2PJ_UINT(ua_obj, threadCnt, uaConfig);
      JS2PJ_BOOL(ua_obj, mainThreadOnly, uaConfig);

      val = ua_obj->Get(Nan::New("nameserver").ToLocalChecked());
      if (val->IsArray())
      {
        const Local<Array> arr_obj = Local<Array>::Cast(val);
        const uint32_t arr_length = arr_obj->Length();
        if (arr_length > 0)
        {
          vector<string> nameservers;
          for (uint32_t i = 0; i < arr_length; ++i)
          {
            const Local<Value> arr_val = arr_obj->Get(i);
            if (arr_val->IsString())
            {
              Nan::Utf8String ns_str(arr_val);
              nameservers.push_back(string(*ns_str));
            }
          }
          if (nameservers.size() > 0)
            uaConfig.nameserver = nameservers;
        }
      }

      JS2PJ_STR(ua_obj, userAgent, uaConfig);

      val = ua_obj->Get(Nan::New("stunServer").ToLocalChecked());
      if (val->IsArray())
      {
        const Local<Array> arr_obj = Local<Array>::Cast(val);
        const uint32_t arr_length = arr_obj->Length();
        if (arr_length > 0)
        {
          vector<string> stunServers;
          for (uint32_t i = 0; i < arr_length; ++i)
          {
            const Local<Value> arr_val = arr_obj->Get(i);
            if (arr_val->IsString())
            {
              Nan::Utf8String stun_str(arr_val);
              stunServers.push_back(string(*stun_str));
            }
          }
          if (stunServers.size() > 0)
            uaConfig.stunServer = stunServers;
        }
      }

      JS2PJ_BOOL(ua_obj, stunIgnoreFailure, uaConfig);
      JS2PJ_INT(ua_obj, natTypeInSdp, uaConfig);
      JS2PJ_BOOL(ua_obj, mwiUnsolicitedEnabled, uaConfig);

      ep_cfg.uaConfig = uaConfig;
    }

    val = cfg_obj->Get(Nan::New("logConfig").ToLocalChecked());
    if (val->IsObject())
    {
      LogConfig logConfig;
      Local<Object> log_obj = val->ToObject();
      JS2PJ_UINT(log_obj, msgLogging, logConfig);
      JS2PJ_UINT(log_obj, level, logConfig);
      JS2PJ_UINT(log_obj, consoleLevel, logConfig);
      JS2PJ_UINT(log_obj, decor, logConfig);
      JS2PJ_STR(log_obj, filename, logConfig);
      JS2PJ_UINT(log_obj, fileFlags, logConfig);

      val = log_obj->Get(Nan::New("writer").ToLocalChecked());
      if (val->IsFunction())
      {
        std::cout << "Get log writer" << std::endl;
        if (logger)
        {
          delete logger;
          uv_close(reinterpret_cast<uv_handle_t *>(&logging), logging_close_cb);
        }
        logger = new SIPSTERLogWriter();
        logger->func = new Nan::Callback(Local<Function>::Cast(val));
        logConfig.writer = logger;
        uv_async_init(uv_default_loop(),
                      &logging,
                      static_cast<uv_async_cb>(logging_cb));
      }
      else
      {
        std::cout << "Get no log writer" << std::endl;
      }

      ep_cfg.logConfig = logConfig;
      //ep_cfg.logConfig.level = 5;
    }

    val = cfg_obj->Get(Nan::New("medConfig").ToLocalChecked());
    if (val->IsObject())
    {
      MediaConfig medConfig;
      Local<Object> med_obj = val->ToObject();
      JS2PJ_UINT(med_obj, clockRate, medConfig);
      JS2PJ_UINT(med_obj, sndClockRate, medConfig);
      JS2PJ_UINT(med_obj, channelCount, medConfig);
      JS2PJ_UINT(med_obj, audioFramePtime, medConfig);
      JS2PJ_UINT(med_obj, maxMediaPorts, medConfig);
      JS2PJ_BOOL(med_obj, hasIoqueue, medConfig);
      JS2PJ_UINT(med_obj, threadCnt, medConfig);
      JS2PJ_UINT(med_obj, quality, medConfig);
      JS2PJ_UINT(med_obj, ptime, medConfig);
      JS2PJ_BOOL(med_obj, noVad, medConfig);
      JS2PJ_UINT(med_obj, ilbcMode, medConfig);
      JS2PJ_UINT(med_obj, txDropPct, medConfig);
      JS2PJ_UINT(med_obj, rxDropPct, medConfig);
      JS2PJ_UINT(med_obj, ecOptions, medConfig);
      JS2PJ_UINT(med_obj, ecTailLen, medConfig);
      JS2PJ_UINT(med_obj, sndRecLatency, medConfig);
      JS2PJ_UINT(med_obj, sndPlayLatency, medConfig);
      JS2PJ_INT(med_obj, jbInit, medConfig);
      JS2PJ_INT(med_obj, jbMinPre, medConfig);
      JS2PJ_INT(med_obj, jbMaxPre, medConfig);
      JS2PJ_INT(med_obj, jbMax, medConfig);
      JS2PJ_INT(med_obj, sndAutoCloseTime, medConfig);
      JS2PJ_BOOL(med_obj, vidPreviewEnableNative, medConfig);

      ep_cfg.medConfig = medConfig;
    }
  }

  try
  {
    ep_cfg.medConfig.clockRate = 44100;
    ep_cfg.medConfig.sndClockRate = 44100;
    ep_cfg.medConfig.noVad = 1;
    std::cout << "#######ep_cfg.medConfig.noVad=" << ep_cfg.medConfig.noVad << std::endl;
    ep->libInit(ep_cfg);

    //ep->codecSetPriority("G722", 139);
    //ep->codecSetPriority("L16/44100/1", 139);

    ep_init = true;
  }
  catch (Error &err)
  {
    errstr = "libInit error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  //uv_async_init(uv_default_loop(), &dumb, static_cast<uv_async_cb>(dumb_cb));

  if (Endpoint::instance().audDevManager().getDevCount() <= 0)
  {
    Endpoint::instance().audDevManager().setNullDev();
  }
  //Endpoint::instance().audDevManager().setNullDev();

  if ((info.Length() == 1 && info[0]->IsBoolean() && info[0]->BooleanValue()) || (info.Length() > 1 && info[1]->IsBoolean() && info[1]->BooleanValue()))
  {
    if (ep_start)
      return Nan::ThrowError("Already started");
    try
    {
      ep->libStart();
      ep_start = true;
    }
    catch (Error &err)
    {
      string errstr = "libStart error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }
  info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(EPStart)
{
  Nan::HandleScope scope;

  if (ep_start)
    return Nan::ThrowError("Already started");
  else if (!ep_init)
    return Nan::ThrowError("Not initialized yet");

  try
  {
    ep->libStart();
    ep_start = true;
  }
  catch (Error &err)
  {
    string errstr = "libStart error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
  info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(EPDestory)
{
  Nan::HandleScope scope;

  if (!ep_start)
    return Nan::ThrowError("Not started");

  try
  {
    ep->libDestroy();

    ep_start = false;
    ep_create = false;
    ep_init = false;
  }
  catch (Error &err)
  {
    string errstr = "libStart error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
  info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(EPGetConfig)
{
  Nan::HandleScope scope;
  string errstr;

  JsonDocument wdoc;

  try
  {
    wdoc.writeObject(ep_cfg);
  }
  catch (Error &err)
  {
    errstr = "JsonDocument.writeObject(EpConfig) error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  try
  {
    info.GetReturnValue().Set(
        Nan::New(wdoc.saveString().c_str()).ToLocalChecked());
  }
  catch (Error &err)
  {
    errstr = "JsonDocument.saveString error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
}

static NAN_METHOD(EPGetState)
{
  Nan::HandleScope scope;

  try
  {
    pjsua_state st = ep->libGetState();
    string state;
    switch (st)
    {
    case PJSUA_STATE_CREATED:
      state = "created";
      break;
    case PJSUA_STATE_INIT:
      state = "init";
      break;
    case PJSUA_STATE_STARTING:
      state = "starting";
      break;
    case PJSUA_STATE_RUNNING:
      state = "running";
      break;
    case PJSUA_STATE_CLOSING:
      state = "closing";
      break;
    default:
      return info.GetReturnValue().SetNull();
    }
    info.GetReturnValue().Set(Nan::New(state.c_str()).ToLocalChecked());
  }
  catch (Error &err)
  {
    string errstr = "libGetState error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
}

static NAN_METHOD(EPHangupAllCalls)
{
  Nan::HandleScope scope;

  try
  {
    ep->hangupAllCalls();
  }
  catch (Error &err)
  {
    string errstr = "hangupAllCalls error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
  info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(EPMediaActivePorts)
{
  Nan::HandleScope scope;

  info.GetReturnValue().Set(Nan::New(ep->mediaActivePorts()));
}

static NAN_METHOD(EPMediaMaxPorts)
{
  Nan::HandleScope scope;

  info.GetReturnValue().Set(Nan::New(ep->mediaMaxPorts()));
}

static NAN_METHOD(GetAudioDevices)
{
  Nan::HandleScope scope;

  Isolate *isolate = info.GetIsolate();
  Local<Array> devices = Array::New(isolate);
  AudioDevInfoVector audioDevs = Endpoint::instance().audDevManager().enumDev();
  for (unsigned int i = 0; i < audioDevs.size(); ++i)
  {
    AudioDevInfo *audioDev = audioDevs.at(i);
    Local<Object> vinfo = Nan::New<Object>();

    int pos = audioDev->name.find(" (");
    if (pos != std::string::npos && pos > 0)
    {
      audioDev->name = audioDev->name.substr(0, pos);
    }
    std::cout << "audio device, id:" << i << " name:" << audioDev->name << std::endl;

    Nan::Set(vinfo,
             Nan::New("name").ToLocalChecked(),
             Nan::New(audioDev->name.c_str()).ToLocalChecked());
    Nan::Set(vinfo,
             Nan::New("inputCount").ToLocalChecked(),
             Nan::New(audioDev->inputCount));
    Nan::Set(vinfo,
             Nan::New("outputCount").ToLocalChecked(),
             Nan::New(audioDev->outputCount));
    Nan::Set(vinfo,
             Nan::New("driver").ToLocalChecked(),
             Nan::New(audioDev->driver.c_str()).ToLocalChecked());

    devices->Set(i, vinfo);
  }

  info.GetReturnValue().Set(devices);
}

static NAN_METHOD(SetCodecPriority)
{
  Nan::HandleScope scope;

  string codecId;
  int priority = 0;
  if (info.Length() > 0 && info[0]->IsString()) // && info[1]->IsInt32())
  {
    Nan::Utf8String codec_str(info[0]);
    codecId = string(*codec_str);
    if (info.Length() > 1) {
      priority = info[1]->Int32Value();
      if (priority < 0)
        priority = 0;

    }
    try
    {
      ep->codecSetPriority(codecId, priority);
      std::cout << "codecSetPriority: codeId:" << codecId << ", priority:" << priority << std::endl;
    }
    catch (Error &err)
    {
      string errstr = "setCodecPriority error:" + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }
  else
  {
    string errstr = "setCodecPriority error, no codec id or priority";
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(GetCodecEnum)
{
  Nan::HandleScope scope;

  Isolate *isolate = info.GetIsolate();
  Local<Array> codec = Array::New(isolate);
  CodecInfoVector codecVec = Endpoint::instance().codecEnum();
  for (unsigned int i = 0; i < codecVec.size(); ++i)
  {
    CodecInfo *codecInfo = codecVec.at(i);
    std::cout << "codec, id:" << i << " code id:" << codecInfo->codecId << std::endl;
    codec->Set(i, Nan::New(codecInfo->codecId.c_str()).ToLocalChecked());
  }

  info.GetReturnValue().Set(codec);
}

extern "C"
{
  void init(Handle<Object> target)
  {
    Nan::HandleScope scope;

#define X(kind, literal) \
  ev_##kind##_##literal##_symbol.Reset(Nan::New(#literal).ToLocalChecked());
    EVENT_SYMBOLS
#undef X
#define X(kind, ctype, name, v8type, valconv) \
  kind##_##name##_symbol.Reset(Nan::New(#name).ToLocalChecked());
    INCALL_FIELDS
    CALLDTMF_FIELDS
    REGSTATE_FIELDS
    REGSTARTING_FIELDS
    INSTANTMESSAGE_FIELDS
    PLAYERSTATUS_FIELDS
    BUDDYSTATUS_FIELDS
    SYSTEMSTATUS_FIELDS
#undef X

    CALLDTMF_DTMF0_symbol.Reset(Nan::New("0").ToLocalChecked());
    CALLDTMF_DTMF1_symbol.Reset(Nan::New("1").ToLocalChecked());
    CALLDTMF_DTMF2_symbol.Reset(Nan::New("2").ToLocalChecked());
    CALLDTMF_DTMF3_symbol.Reset(Nan::New("3").ToLocalChecked());
    CALLDTMF_DTMF4_symbol.Reset(Nan::New("4").ToLocalChecked());
    CALLDTMF_DTMF5_symbol.Reset(Nan::New("5").ToLocalChecked());
    CALLDTMF_DTMF6_symbol.Reset(Nan::New("6").ToLocalChecked());
    CALLDTMF_DTMF7_symbol.Reset(Nan::New("7").ToLocalChecked());
    CALLDTMF_DTMF8_symbol.Reset(Nan::New("8").ToLocalChecked());
    CALLDTMF_DTMF9_symbol.Reset(Nan::New("9").ToLocalChecked());
    CALLDTMF_DTMFSTAR_symbol.Reset(Nan::New("*").ToLocalChecked());
    CALLDTMF_DTMFPOUND_symbol.Reset(Nan::New("#").ToLocalChecked());
    CALLDTMF_DTMFA_symbol.Reset(Nan::New("A").ToLocalChecked());
    CALLDTMF_DTMFB_symbol.Reset(Nan::New("B").ToLocalChecked());
    CALLDTMF_DTMFC_symbol.Reset(Nan::New("C").ToLocalChecked());
    CALLDTMF_DTMFD_symbol.Reset(Nan::New("D").ToLocalChecked());

    emit_symbol.Reset(Nan::New("emit").ToLocalChecked());

    uv_mutex_init(&event_mutex);
    uv_mutex_init(&log_mutex);
    uv_mutex_init(&async_mutex);
    uv_mutex_init(&local_media_mutex);

    SIPSTERAccount::Initialize(target);
    SIPSTERCall::Initialize(target);
    SIPSTERMedia::Initialize(target);
    SIPSTERTransport::Initialize(target);
    SIPSTERBuddy::Initialize(target);
    SIPSTERPlatform::Initialize(target);

    Nan::Set(target,
             Nan::New("systemInit").ToLocalChecked(),
             Nan::New<FunctionTemplate>(SystemInit)->GetFunction());

    Nan::Set(target,
             Nan::New("version").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPVersion)->GetFunction());
    Nan::Set(target,
             Nan::New("state").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPGetState)->GetFunction());
    Nan::Set(target,
             Nan::New("config").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPGetConfig)->GetFunction());
    Nan::Set(target,
             Nan::New("init").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPInit)->GetFunction());
    Nan::Set(target,
             Nan::New("start").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPStart)->GetFunction());
    Nan::Set(target,
             Nan::New("destory").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPDestory)->GetFunction());
    Nan::Set(target,
             Nan::New("hangupAllCalls").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPHangupAllCalls)->GetFunction());
    Nan::Set(target,
             Nan::New("mediaActivePorts").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPMediaActivePorts)->GetFunction());
    Nan::Set(target,
             Nan::New("mediaMaxPorts").ToLocalChecked(),
             Nan::New<FunctionTemplate>(EPMediaMaxPorts)->GetFunction());

    Nan::Set(target,
             Nan::New("createRecorder").ToLocalChecked(),
             Nan::New<FunctionTemplate>(CreateRecorder)->GetFunction());
    Nan::Set(target,
             Nan::New("createPlayer").ToLocalChecked(),
             Nan::New<FunctionTemplate>(CreatePlayer)->GetFunction());
    Nan::Set(target,
             Nan::New("createPlaylist").ToLocalChecked(),
             Nan::New<FunctionTemplate>(CreatePlaylist)->GetFunction());

    Nan::Set(target,
             Nan::New("enumDevs").ToLocalChecked(),
             Nan::New<FunctionTemplate>(GetAudioDevices)->GetFunction());

    Nan::Set(target,
             Nan::New("codecEnum").ToLocalChecked(),
             Nan::New<FunctionTemplate>(GetCodecEnum)->GetFunction());

    Nan::Set(target,
             Nan::New("setCodecPriority").ToLocalChecked(),
             Nan::New<FunctionTemplate>(SetCodecPriority)->GetFunction());
  }

  NODE_MODULE(sipster, init);
}

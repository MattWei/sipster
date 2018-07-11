#ifndef COMMON_H_
#define COMMON_H_

#include <node.h>
#include <nan.h>
#include <string.h>
//#include <strings.h>
//#include <regex.h>
#include <pjsua2.hpp>

#include "SIPSTERAccount.h"
#include "SIPSTERCall.h"
#include "SIPSTERMedia.h"
#include "SIPSTERBuddy.h"

using namespace std;
using namespace node;
using namespace v8;
using namespace pj;

#define SIP_DEBUG 0

#define JS2PJ_INT(js, prop, pj) do {                                           \
  val = Nan::Get(js, Nan::New(#prop).ToLocalChecked()).ToLocalChecked();       \
  if (val->IsInt32()) {                                                        \
    pj.prop = val->Int32Value();                                               \
  }                                                                            \
} while(0)
#define JS2PJ_UINT(js, prop, pj) do {                                          \
  val = Nan::Get(js, Nan::New(#prop).ToLocalChecked()).ToLocalChecked();       \
  if (val->IsUint32()) {                                                       \
    pj.prop = val->Uint32Value();                                              \
  }                                                                            \
} while(0)
#define JS2PJ_ENUM(js, prop, type, pj) do {                                    \
  val = Nan::Get(js, Nan::New(#prop).ToLocalChecked()).ToLocalChecked();       \
  if (val->IsInt32()) {                                                        \
    pj.prop = static_cast<type>(val->Int32Value());                            \
  }                                                                            \
} while(0)
#define JS2PJ_STR(js, prop, pj) do {                                           \
  val = Nan::Get(js, Nan::New(#prop).ToLocalChecked()).ToLocalChecked();       \
  if (val->IsString()) {                                                       \
    Nan::Utf8String v8str(val);                                                \
    pj.prop = string(*v8str);                                                  \
  }                                                                            \
} while(0)
#define JS2PJ_BOOL(js, prop, pj) do {                                          \
  val = Nan::Get(js, Nan::New(#prop).ToLocalChecked()).ToLocalChecked();       \
  if (val->IsBoolean()) {                                                      \
    pj.prop = val->BooleanValue();                                             \
  }                                                                            \
} while(0)

#define ENQUEUE_EVENT(ev) do {                                                 \
  uv_mutex_lock(&event_mutex);                                                 \
  event_queue.push_back(ev);                                                   \
  uv_mutex_unlock(&event_mutex);                                               \
  uv_async_send(&dumb);                                                        \
} while(0)

#define SETUP_EVENT(name)                                                      \
  SIPEventInfo ev;                                                             \
  EV_ARGS_##name* args = new EV_ARGS_##name;                                   \
  ev.type = EVENT_##name;                                                      \
  ev.args = reinterpret_cast<void*>(args)

#define SETUP_EVENT_NOARGS(name)                                               \
  SIPEventInfo ev;                                                             \
  ev.type = EVENT_##name

#define EVENT_TYPES                                                            \
  X(INCALL)                                                                    \
  X(CALLSTATE)                                                                 \
  X(CALLDTMF)                                                                  \
  X(REGSTATE)                                                                  \
  X(CALLMEDIA)                                                                 \
  X(PLAYEREOF)                                                                 \
  X(REGSTARTING)                                                               \
  X(INSTANTMESSAGE)                                                            \
  X(PLAYERSTATUS)                                                              \
  X(BUDDYSTATUS)  

#define EVENT_SYMBOLS                                                          \
  X(INCALL, call)                                                              \
  X(CALLSTATE, calling)                                                        \
  X(CALLSTATE, incoming)                                                       \
  X(CALLSTATE, early)                                                          \
  X(CALLSTATE, connecting)                                                     \
  X(CALLSTATE, confirmed)                                                      \
  X(CALLSTATE, disconnected)                                                   \
  X(CALLSTATE, state)                                                          \
  X(CALLDTMF, dtmf)                                                            \
  X(REGSTATE, registered)                                                      \
  X(REGSTATE, unregistered)                                                    \
  X(REGSTATE, state)                                                           \
  X(CALLMEDIA, media)                                                          \
  X(PLAYEREOF, eof)                                                            \
  X(REGSTARTING, registering)                                                  \
  X(REGSTARTING, unregistering)                                                \
  X(INSTANTMESSAGE, instantMessage)                                            \
  X(PLAYERSTATUS, playerStatus)                                                \
  X(BUDDYSTATUS, buddyStatus)
                    
enum SIPEvent {
#define X(kind)                                                                \
  EVENT_##kind,
  EVENT_TYPES
#undef X
};

struct SIPEventInfo {
  SIPEvent type;
  SIPSTERCall* call;
  SIPSTERAccount* acct;
  SIPSTERMedia* media;
  SIPSTERBuddy* buddy;
  void* args;
};

// registration change event(s) ================================================
#define N_REGSTATE_FIELDS 2
#define REGSTATE_FIELDS                                                        \
  X(REGSTATE, bool, active, Boolean, active)                                   \
  X(REGSTATE, int, statusCode, Integer, statusCode)
struct EV_ARGS_REGSTATE {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  REGSTATE_FIELDS
#undef X
};
// =============================================================================

// incoming call event =========================================================
#define N_INCALL_FIELDS 7
#define INCALL_FIELDS                                                          \
  X(INCALL, string, srcAddress, String, srcAddress.c_str())                    \
  X(INCALL, string, localUri, String, localUri.c_str())                        \
  X(INCALL, string, localContact, String, localContact.c_str())                \
  X(INCALL, string, remoteUri, String, remoteUri.c_str())                      \
  X(INCALL, string, remoteContact, String, remoteContact.c_str())              \
  X(INCALL, string, callId, String, callId.c_str())
struct EV_ARGS_INCALL {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  INCALL_FIELDS
#undef X
};
// =============================================================================

// call state change event(s) ==================================================
#define N_CALLSTATE_FIELDS 2
#define CALLSTATE_FIELDS                                                       \
  X(CALLSTATE, pjsip_inv_state, _state, Integer, _state)                       \
  X(CALLSTATE, int, _lastStatuscode, Integer, _lastStatuscode)
struct EV_ARGS_CALLSTATE {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  CALLSTATE_FIELDS
#undef X
};
// =============================================================================

// Reg/Unreg starting event ====================================================
#define N_REGSTARTING_FIELDS 1
#define REGSTARTING_FIELDS                                                     \
  X(REGSTARTING, bool, renew, Boolean, renew)
struct EV_ARGS_REGSTARTING {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  REGSTARTING_FIELDS
#undef X
};
// =============================================================================

// Instant message event ====================================================
#define N_INSTANTMESSAGE_FIELDS 3
#define INSTANTMESSAGE_FIELDS                                                     \
  X(INSTANTMESSAGE, string, fromUri, String, fromUri.c_str())              \
  X(INSTANTMESSAGE, string, msg, String, msg.c_str())
struct EV_ARGS_INSTANTMESSAGE {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  INSTANTMESSAGE_FIELDS
#undef X
};
// =============================================================================

// player status event ====================================================
#define N_PLAYERSTATUS_FIELDS 4
#define PLAYERSTATUS_FIELDS                                                     \
  X(PLAYERSTATUS, string, songPath, String, songPath.c_str())              \
  X(PLAYERSTATUS, int, type, Integer, type)                                \
  X(PLAYERSTATUS, int, param, Integer, param)                                
struct EV_ARGS_PLAYERSTATUS {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  PLAYERSTATUS_FIELDS
#undef X
};
// =============================================================================

// buddy status event ====================================================
#define N_BUDDYSTATUS_FIELDS 3
#define BUDDYSTATUS_FIELDS                                                     \
  X(BUDDYSTATUS, string, uri, String, uri.c_str())              \
  X(BUDDYSTATUS, string, statusText, String, statusText.c_str())                                                             
struct EV_ARGS_BUDDYSTATUS {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  BUDDYSTATUS_FIELDS
#undef X
};
// =============================================================================

#define N_CALLDTMF_FIELDS 1
#define CALLDTMF_FIELDS                                                        \
  X(CALLDTMF, char, digit, String, digit[0])
struct EV_ARGS_CALLDTMF {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  CALLDTMF_FIELDS
#undef X
};

#define N_PLAYEREOF_FIELDS 1
#define PLAYEREOF_FIELDS                                                        \
  X(PLAYEREOF, string, song, String, song.c_str())
struct EV_ARGS_PLAYEREOF {
#define X(kind, ctype, name, v8type, valconv) ctype name;
  PLAYEREOF_FIELDS
#undef X
};

#define X(kind, ctype, name, v8type, valconv)                                  \
  extern Nan::Persistent<String> kind##_##name##_symbol;
  INCALL_FIELDS
  CALLDTMF_FIELDS
  REGSTATE_FIELDS
  REGSTARTING_FIELDS
  INSTANTMESSAGE_FIELDS
  PLAYERSTATUS_FIELDS
  BUDDYSTATUS_FIELDS
#undef X

// start generic event-related definitions =====================================
#define X(kind, literal)                                                       \
  extern Nan::Persistent<String> ev_##kind##_##literal##_symbol;
  EVENT_SYMBOLS
#undef X

extern list<SIPEventInfo> event_queue;
extern uv_mutex_t event_mutex;
extern uv_mutex_t async_mutex;
extern uv_async_t dumb;
extern Nan::Persistent<String> emit_symbol;
// =============================================================================

extern Nan::Persistent<FunctionTemplate> SIPSTERMedia_constructor;
extern Nan::Persistent<FunctionTemplate> SIPSTERAccount_constructor;
extern Nan::Persistent<FunctionTemplate> SIPSTERCall_constructor;
extern Nan::Persistent<FunctionTemplate> SIPSTERTransport_constructor;
extern Nan::Persistent<FunctionTemplate> SIPSTERBuddy_constructor;
//extern Nan::Persistent<FunctionTemplate> SIPSTERAudioDevInfo_constructor;

extern Endpoint* ep;

#endif

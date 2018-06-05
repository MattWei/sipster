#ifndef SIPSTERBUDDY_H_
#define SIPSTERBUDDY_H_

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

class SIPSTERBuddy : public Buddy, public Nan::ObjectWrap {
public:
  Nan::Callback* emit;

  SIPSTERBuddy();
  ~SIPSTERBuddy();

  virtual void onBuddyState();
  
  static NAN_METHOD(New);
  static NAN_METHOD(SendInstantMessage);
  static NAN_METHOD(SubscribePresence);
  static void Initialize(Handle<Object> target);
};

#endif

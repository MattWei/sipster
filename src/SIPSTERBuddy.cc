#include "SIPSTERBuddy.h"
#include "common.h"

#include <iostream>

Nan::Persistent<FunctionTemplate> SIPSTERBuddy_constructor;

SIPSTERBuddy::SIPSTERBuddy() {
}
SIPSTERBuddy::~SIPSTERBuddy() {
  std::cout << "Delete buddy" << std::endl;
  if (emit) {
    std::cout << "Delete buddy emit" << std::endl;
    delete emit;
    std::cout << "Delete buddy emit succeed" << std::endl;
  }
    
  std::cout << "SIPSTERBuddy::~SIPSTERBuddy() finish" << std::endl;
}

void SIPSTERBuddy::onBuddyState()
{
  BuddyInfo bi = getInfo();
  std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`Buddy " << bi.uri << " is " << bi.presStatus.statusText << std::endl;
  SETUP_EVENT(BUDDYSTATUS);
  ev.buddy = this;

  args->uri = bi.uri;
  args->statusText = bi.presStatus.statusText;

  ENQUEUE_EVENT(ev);
}

NAN_METHOD(SIPSTERBuddy::New) {
  Nan::HandleScope scope;

  std::cout << "SIPSTERBuddy::New" << std::endl;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Use `new` to create instances of this object.");

  SIPSTERBuddy* buddy = new SIPSTERBuddy();

  buddy->Wrap(info.This());
  
  buddy->emit = new Nan::Callback(
    Local<Function>::Cast(buddy->handle()->Get(Nan::New(emit_symbol)))
  );

  info.GetReturnValue().Set(info.This());

  std::cout << "SIPSTERBuddy::New finish" << std::endl;
}

NAN_METHOD(SIPSTERBuddy::SendInstantMessage) {
  Nan::HandleScope scope;

  string msg;
  if (info.Length() > 0 && info[0]->IsString()) {
    Nan::Utf8String msg_str(info[0]);
    msg = string(*msg_str);
    
  } else
    return Nan::ThrowTypeError("Missing message detail");

  SIPSTERBuddy* buddy = Nan::ObjectWrap::Unwrap<SIPSTERBuddy>(info.This());
  SendInstantMessageParam param;
  param.content = msg;
  try {
    buddy->sendInstantMessage(param);
  } catch(Error& err) {
    string errstr = "Buddy.sendInstantMessage() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  std::cout << "SIPSTERBuddy::SendInstantMessage: " << msg << std::endl;
  info.GetReturnValue().SetUndefined();
}


NAN_METHOD(SIPSTERBuddy::SubscribePresence) {
  Nan::HandleScope scope;
  /*
  bool isSubscribePresence = false;
  if (info.Length() > 0 && info[0]->IsBoolean()) {
    isSubscribePresence = info[0]->BooleanValue();
  } else
    return Nan::ThrowTypeError("Missing SubscribePresence setting");

  SIPSTERBuddy* buddy = Nan::ObjectWrap::Unwrap<SIPSTERBuddy>(info.This());
  try {
    buddy->subscribePresence(isSubscribePresence);
  } catch(Error& err) {
    string errstr = "Buddy.SubscribePresence() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }
  
  std::cout << "SIPSTERBuddy::SubscribePresence: " << isSubscribePresence << std::endl;
  */
  info.GetReturnValue().SetUndefined();
}


void SIPSTERBuddy::Initialize(Handle<Object> target) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  Local<String> name = Nan::New("Buddy").ToLocalChecked();

  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(name);

  Nan::SetPrototypeMethod(tpl, "sendInstantMessage", SendInstantMessage);
  Nan::SetPrototypeMethod(tpl, "subscribePresence", SubscribePresence);

  Nan::Set(target, name, tpl->GetFunction());

  SIPSTERBuddy_constructor.Reset(tpl);
}

#include "SIPSTERAudioDevInfo.h"
#include "common.h"

#include <iostream>

Nan::Persistent<FunctionTemplate> SIPSTERAudioDevInfo_constructor;

SIPSTERAudioDevInfo::SIPSTERAudioDevInfo() : audioDev(NULL) {}
SIPSTERAudioDevInfo::~SIPSTERAudioDevInfo() {
}

NAN_METHOD(SIPSTERAudioDevInfo::New) {
  Nan::HandleScope scope;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Use `new` to create instances of this object.");

  SIPSTERAudioDevInfo* med = new SIPSTERAudioDevInfo();

  med->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

NAN_GETTER(SIPSTERAudioDevInfo::NameGetter) {
  SIPSTERAudioDevInfo* med = Nan::ObjectWrap::Unwrap<SIPSTERAudioDevInfo>(info.This());

  std::string name;

  if (med->audioDev) {
      name = med->audioDev->name;
  }

  info.GetReturnValue().Set(Nan::New(name.c_str()).ToLocalChecked());
}

NAN_GETTER(SIPSTERAudioDevInfo::InputCountGetter) {
    SIPSTERAudioDevInfo* med = Nan::ObjectWrap::Unwrap<SIPSTERAudioDevInfo>(info.This());

  unsigned inputCount = 0;

  if (med->audioDev) {
    try {
      inputCount = med->audioDev->inputCount;
    } catch(Error& err) {
      string errstr = "AudioMedia.getTxLevel() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }

  info.GetReturnValue().Set(Nan::New(inputCount));
}

NAN_GETTER(SIPSTERAudioDevInfo::OutputCountGetter) {
  SIPSTERAudioDevInfo* med = Nan::ObjectWrap::Unwrap<SIPSTERAudioDevInfo>(info.This());
    unsigned outputCount = 0;

  if (med->audioDev) {
    try {
      outputCount = med->audioDev->outputCount;
    } catch(Error& err) {
      string errstr = "AudioMedia.getTxLevel() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }

  info.GetReturnValue().Set(Nan::New(outputCount));
}

NAN_GETTER(SIPSTERAudioDevInfo::DriverGetter) {
  SIPSTERAudioDevInfo* med = Nan::ObjectWrap::Unwrap<SIPSTERAudioDevInfo>(info.This());
    std::string driver;

  if (med->audioDev) {
      driver = med->audioDev->driver;
  }

  info.GetReturnValue().Set(Nan::New(driver.c_str()).ToLocalChecked());
}

void SIPSTERAudioDevInfo::Initialize(Handle<Object> target) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  Local<String> name = Nan::New("AudioDevInfo").ToLocalChecked();

  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(name);

  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("name").ToLocalChecked(),
                   NameGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("inputCount").ToLocalChecked(),
                   InputCountGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("outputCount").ToLocalChecked(),
                   OutputCountGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("driver").ToLocalChecked(),
                   DriverGetter);

  Nan::Set(target, name, tpl->GetFunction());

  SIPSTERAudioDevInfo_constructor.Reset(tpl);
}

#ifndef SIPSTERAUDIODEVINFO_H_
#define SIPSTERAUDIODEVINFO_H_

#include <node.h>
#include <nan.h>
#include <string.h>
#include <pjsua2.hpp>

using namespace std;
using namespace node;
using namespace v8;
using namespace pj;

class SIPSTERAudioDevInfo : public Nan::ObjectWrap 
{
public:
    AudioDevInfo *audioDev;


  SIPSTERAudioDevInfo();
  ~SIPSTERAudioDevInfo();

  static NAN_METHOD(New);
    
  static NAN_GETTER(NameGetter);
  static NAN_GETTER(InputCountGetter);
  static NAN_GETTER(OutputCountGetter);
  static NAN_GETTER(DriverGetter);

  static void Initialize(Handle<Object> target);
};
#endif
#include "SIPSTERAccount.h"
#include "SIPSTERCall.h"
#include "SIPSTERTransport.h"
#include "common.h"

#include "SIPSTERAudioDevInfo.h"

#include <iostream>

Nan::Persistent<FunctionTemplate> SIPSTERAccount_constructor;

//regex_t fromuri_regex;
//regex_t touri_regex;

SIPSTERAccount::SIPSTERAccount() {
}
SIPSTERAccount::~SIPSTERAccount() {
  if (emit)
    delete emit;
}

AccountConfig SIPSTERAccount::genConfig(Local<Object> acct_obj) {
  Nan::HandleScope scope;

  AccountConfig acct_cfg;

  Local<Value> val;

  JS2PJ_INT(acct_obj, priority, acct_cfg);
  JS2PJ_STR(acct_obj, idUri, acct_cfg);

  val = acct_obj->Get(Nan::New("regConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountRegConfig regConfig;
    Local<Object> reg_obj = val->ToObject();
    JS2PJ_STR(reg_obj, registrarUri, regConfig);
    JS2PJ_BOOL(reg_obj, registerOnAdd, regConfig);

    val = reg_obj->Get(Nan::New("headers").ToLocalChecked());
    if (val->IsObject()) {
      const Local<Object> hdr_obj = val->ToObject();
      const Local<Array> hdr_props = hdr_obj->GetPropertyNames();
      const uint32_t hdr_length = hdr_props->Length();
      if (hdr_length > 0) {
        vector<SipHeader> sipheaders;
        for (uint32_t i = 0; i < hdr_length; ++i) {
          const Local<Value> key = hdr_props->Get(i);
          const Local<Value> value = hdr_obj->Get(key);
          SipHeader hdr;
          Nan::Utf8String name_str(key);
          Nan::Utf8String val_str(value);
          hdr.hName = string(*name_str);
          hdr.hValue = string(*val_str);
          sipheaders.push_back(hdr);
        }
        regConfig.headers = sipheaders;
      }
    }

    JS2PJ_UINT(reg_obj, timeoutSec, regConfig);
    JS2PJ_UINT(reg_obj, retryIntervalSec, regConfig);
    JS2PJ_UINT(reg_obj, firstRetryIntervalSec, regConfig);
    JS2PJ_UINT(reg_obj, delayBeforeRefreshSec, regConfig);
    JS2PJ_BOOL(reg_obj, dropCallsOnFail, regConfig);
    JS2PJ_UINT(reg_obj, unregWaitMsec, regConfig);
    JS2PJ_UINT(reg_obj, proxyUse, regConfig);

    acct_cfg.regConfig = regConfig;
  }
  val = acct_obj->Get(Nan::New("sipConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountSipConfig sipConfig;
    Local<Object> sip_obj = val->ToObject();

    val = sip_obj->Get(Nan::New("authCreds").ToLocalChecked());
    if (val->IsArray()) {
      const Local<Array> arr_obj = Local<Array>::Cast(val);
      const uint32_t arr_length = arr_obj->Length();
      if (arr_length > 0) {
        vector<AuthCredInfo> creds;
        for (uint32_t i = 0; i < arr_length; ++i) {
          const Local<Value> cred_value = arr_obj->Get(i);
          if (cred_value->IsObject()) {
            const Local<Object> auth_obj = cred_value->ToObject();
            AuthCredInfo credinfo;
            Local<Value> scheme_val = auth_obj->Get(Nan::New("scheme").ToLocalChecked());
            Local<Value> realm_val = auth_obj->Get(Nan::New("realm").ToLocalChecked());
            Local<Value> username_val = auth_obj->Get(Nan::New("username").ToLocalChecked());
            Local<Value> dataType_val = auth_obj->Get(Nan::New("dataType").ToLocalChecked());
            Local<Value> data_val = auth_obj->Get(Nan::New("data").ToLocalChecked());
            if (scheme_val->IsString()
                && realm_val->IsString()
                && username_val->IsString()
                && dataType_val->IsInt32()
                && data_val->IsString()) {
              Nan::Utf8String scheme_str(scheme_val);
              Nan::Utf8String realm_str(realm_val);
              Nan::Utf8String username_str(username_val);
              Nan::Utf8String data_str(data_val);
              credinfo.scheme = string(*scheme_str);
              credinfo.realm = string(*realm_str);
              credinfo.username = string(*username_str);
              credinfo.dataType = dataType_val->Int32Value();
              credinfo.data = string(*data_str);
              creds.push_back(credinfo);
            }
          }
        }
        if (creds.size() > 0)
          sipConfig.authCreds = creds;
      }
    }

    val = sip_obj->Get(Nan::New("proxies").ToLocalChecked());
    if (val->IsArray()) {
      const Local<Array> arr_obj = Local<Array>::Cast(val);
      const uint32_t arr_length = arr_obj->Length();
      if (arr_length > 0) {
        vector<string> proxies;
        for (uint32_t i = 0; i < arr_length; ++i) {
          const Local<Value> value = arr_obj->Get(i);
          Nan::Utf8String value_str(value);
          proxies.push_back(string(*value_str));
        }
        sipConfig.proxies = proxies;
      }
    }

    JS2PJ_STR(sip_obj, contactForced, sipConfig);
    JS2PJ_STR(sip_obj, contactParams, sipConfig);
    JS2PJ_STR(sip_obj, contactUriParams, sipConfig);
    JS2PJ_BOOL(sip_obj, authInitialEmpty, sipConfig);
    JS2PJ_STR(sip_obj, authInitialAlgorithm, sipConfig);

    // deviates from the pjsip config structure to accept a Transport instance
    // instead of a transport id since that information is made available to
    // JS land
    val = sip_obj->Get(Nan::New("transport").ToLocalChecked());
    if (Nan::New(SIPSTERTransport_constructor)->HasInstance(val)) {
      SIPSTERTransport* trans =
        Nan::ObjectWrap::Unwrap<SIPSTERTransport>(Local<Object>::Cast(val));
      sipConfig.transportId = trans->transId;
    }

    acct_cfg.sipConfig = sipConfig;
  }
  val = acct_obj->Get(Nan::New("callConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountCallConfig callConfig;
    Local<Object> call_obj = val->ToObject();
    JS2PJ_ENUM(call_obj, holdType, pjsua_call_hold_type, callConfig);
    JS2PJ_ENUM(call_obj, prackUse, pjsua_100rel_use, callConfig);
    JS2PJ_ENUM(call_obj, timerUse, pjsua_sip_timer_use, callConfig);
    JS2PJ_UINT(call_obj, timerMinSESec, callConfig);
    JS2PJ_UINT(call_obj, timerSessExpiresSec, callConfig);

    acct_cfg.callConfig = callConfig;
  }
  val = acct_obj->Get(Nan::New("presConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountPresConfig presConfig;
    Local<Object> pres_obj = val->ToObject();

    val = pres_obj->Get(Nan::New("headers").ToLocalChecked());
    if (val->IsObject()) {
      const Local<Object> hdr_obj = val->ToObject();
      const Local<Array> hdr_props = hdr_obj->GetPropertyNames();
      const uint32_t hdr_length = hdr_props->Length();
      if (hdr_length > 0) {
        vector<SipHeader> sipheaders;
        for (uint32_t i = 0; i < hdr_length; ++i) {
          const Local<Value> key = hdr_props->Get(i);
          const Local<Value> value = hdr_obj->Get(key);
          SipHeader hdr;
          Nan::Utf8String name_str(key);
          Nan::Utf8String value_str(value);
          hdr.hName = string(*name_str);
          hdr.hValue = string(*value_str);
          sipheaders.push_back(hdr);
        }
        presConfig.headers = sipheaders;
      }
    }

    JS2PJ_BOOL(pres_obj, publishEnabled, presConfig);
    JS2PJ_BOOL(pres_obj, publishQueue, presConfig);
    JS2PJ_UINT(pres_obj, publishShutdownWaitMsec, presConfig);
    JS2PJ_STR(pres_obj, pidfTupleId, presConfig);

    acct_cfg.presConfig = presConfig;
  }
  val = acct_obj->Get(Nan::New("mwiConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountMwiConfig mwiConfig;
    Local<Object> mwi_obj = val->ToObject();
    JS2PJ_BOOL(mwi_obj, enabled, mwiConfig);
    JS2PJ_UINT(mwi_obj, expirationSec, mwiConfig);

    acct_cfg.mwiConfig = mwiConfig;
  }
  val = acct_obj->Get(Nan::New("natConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountNatConfig natConfig;
    Local<Object> nat_obj = val->ToObject();
    JS2PJ_ENUM(nat_obj, sipStunUse, pjsua_stun_use, natConfig);
    JS2PJ_ENUM(nat_obj, mediaStunUse, pjsua_stun_use, natConfig);
    JS2PJ_BOOL(nat_obj, iceEnabled, natConfig);
    JS2PJ_INT(nat_obj, iceMaxHostCands, natConfig);
    JS2PJ_BOOL(nat_obj, iceAggressiveNomination, natConfig);
    JS2PJ_UINT(nat_obj, iceNominatedCheckDelayMsec, natConfig);
    JS2PJ_INT(nat_obj, iceWaitNominationTimeoutMsec, natConfig);
    JS2PJ_BOOL(nat_obj, iceNoRtcp, natConfig);
    JS2PJ_BOOL(nat_obj, iceAlwaysUpdate, natConfig);
    JS2PJ_BOOL(nat_obj, turnEnabled, natConfig);
    JS2PJ_STR(nat_obj, turnServer, natConfig);
    JS2PJ_ENUM(nat_obj, turnConnType, pj_turn_tp_type, natConfig);
    JS2PJ_STR(nat_obj, turnUserName, natConfig);
    JS2PJ_INT(nat_obj, turnPasswordType, natConfig);
    JS2PJ_STR(nat_obj, turnPassword, natConfig);
    JS2PJ_INT(nat_obj, contactRewriteUse, natConfig);
    JS2PJ_INT(nat_obj, contactRewriteMethod, natConfig);
    JS2PJ_INT(nat_obj, viaRewriteUse, natConfig);
    JS2PJ_INT(nat_obj, sdpNatRewriteUse, natConfig);
    JS2PJ_INT(nat_obj, sipOutboundUse, natConfig);
    JS2PJ_STR(nat_obj, sipOutboundInstanceId, natConfig);
    JS2PJ_STR(nat_obj, sipOutboundRegId, natConfig);
    JS2PJ_UINT(nat_obj, udpKaIntervalSec, natConfig);
    JS2PJ_STR(nat_obj, udpKaData, natConfig);

    acct_cfg.natConfig = natConfig;
  }
  val = acct_obj->Get(Nan::New("mediaConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountMediaConfig mediaConfig;
    Local<Object> media_obj = val->ToObject();

    val = media_obj->Get(Nan::New("transportConfig").ToLocalChecked());
    if (val->IsObject()) {
      TransportConfig transportConfig;
      Local<Object> obj = val->ToObject();
      JS2PJ_UINT(obj, port, transportConfig);
      JS2PJ_UINT(obj, portRange, transportConfig);
      JS2PJ_STR(obj, publicAddress, transportConfig);
      JS2PJ_STR(obj, boundAddress, transportConfig);
      JS2PJ_ENUM(obj, qosType, pj_qos_type, transportConfig);

      val = obj->Get(Nan::New("qosParams").ToLocalChecked());
      if (val->IsObject()) {
        pj_qos_params qos_params;
        Local<Object> qos_obj = val->ToObject();
        Local<Value> flags_val = qos_obj->Get(Nan::New("flags").ToLocalChecked());
        Local<Value> dscp_val = qos_obj->Get(Nan::New("dscp_val").ToLocalChecked());
        Local<Value> so_prio_val = qos_obj->Get(Nan::New("so_prio").ToLocalChecked());
        Local<Value> wmm_prio_val = qos_obj->Get(Nan::New("wmm_prio").ToLocalChecked());
        if (flags_val->IsUint32()) {
          qos_params.flags =
            static_cast<pj_uint8_t>(flags_val->Uint32Value());
        }
        if (dscp_val->IsUint32()) {
          qos_params.dscp_val =
            static_cast<pj_uint8_t>(dscp_val->Uint32Value());
        }
        if (so_prio_val->IsUint32()) {
          qos_params.so_prio =
            static_cast<pj_uint8_t>(so_prio_val->Uint32Value());
        }
        if (wmm_prio_val->IsUint32()) {
          qos_params.wmm_prio =
            static_cast<pj_qos_wmm_prio>(wmm_prio_val->Uint32Value());
        }
        transportConfig.qosParams = qos_params;
      }

      val = obj->Get(Nan::New("tlsConfig").ToLocalChecked());
      if (val->IsObject()) {
        TlsConfig tlsConfig;
        Local<Object> tls_obj = val->ToObject();
        JS2PJ_STR(tls_obj, CaListFile, tlsConfig);
        JS2PJ_STR(tls_obj, certFile, tlsConfig);
        JS2PJ_STR(tls_obj, privKeyFile, tlsConfig);
        JS2PJ_STR(tls_obj, password, tlsConfig);
        JS2PJ_ENUM(tls_obj, method, pjsip_ssl_method, tlsConfig);

        val = tls_obj->Get(Nan::New("ciphers").ToLocalChecked());
        if (val->IsArray()) {
          const Local<Array> arr_obj = Local<Array>::Cast(val);
          const uint32_t arr_length = arr_obj->Length();
          if (arr_length > 0) {
            vector<int> ciphers;
            for (uint32_t i = 0; i < arr_length; ++i) {
              const Local<Value> value = arr_obj->Get(i);
              ciphers.push_back(value->Int32Value());
            }
            tlsConfig.ciphers = ciphers;
          }
        }

        JS2PJ_BOOL(tls_obj, verifyServer, tlsConfig);
        JS2PJ_BOOL(tls_obj, verifyClient, tlsConfig);
        JS2PJ_BOOL(tls_obj, requireClientCert, tlsConfig);
        JS2PJ_UINT(tls_obj, msecTimeout, tlsConfig);
        JS2PJ_ENUM(tls_obj, qosType, pj_qos_type, tlsConfig);

        val = tls_obj->Get(Nan::New("qosParams").ToLocalChecked());
        if (val->IsObject()) {
          pj_qos_params qos_params;
          Local<Object> qos_obj = val->ToObject();
          Local<Value> flags_val = qos_obj->Get(Nan::New("flags").ToLocalChecked());
          Local<Value> dscp_val = qos_obj->Get(Nan::New("dscp_val").ToLocalChecked());
          Local<Value> so_prio_val = qos_obj->Get(Nan::New("so_prio").ToLocalChecked());
          Local<Value> wmm_prio_val = qos_obj->Get(Nan::New("wmm_prio").ToLocalChecked());
          if (flags_val->IsUint32()) {
            qos_params.flags =
              static_cast<pj_uint8_t>(flags_val->Uint32Value());
          }
          if (dscp_val->IsUint32()) {
            qos_params.dscp_val =
              static_cast<pj_uint8_t>(dscp_val->Uint32Value());
          }
          if (so_prio_val->IsUint32()) {
            qos_params.so_prio =
              static_cast<pj_uint8_t>(so_prio_val->Uint32Value());
          }
          if (wmm_prio_val->IsUint32()) {
            qos_params.wmm_prio =
              static_cast<pj_qos_wmm_prio>(wmm_prio_val->Uint32Value());
          }
          tlsConfig.qosParams = qos_params;
        }

        JS2PJ_BOOL(tls_obj, qosIgnoreError, tlsConfig);

        transportConfig.tlsConfig = tlsConfig;
      }

      mediaConfig.transportConfig = transportConfig;
    }

    JS2PJ_BOOL(media_obj, lockCodecEnabled, mediaConfig);
    JS2PJ_BOOL(media_obj, streamKaEnabled, mediaConfig);
    JS2PJ_ENUM(media_obj, srtpUse, pjmedia_srtp_use, mediaConfig);
    JS2PJ_INT(media_obj, srtpSecureSignaling, mediaConfig);
    JS2PJ_ENUM(media_obj, ipv6Use, pjsua_ipv6_use, mediaConfig);

    acct_cfg.mediaConfig = mediaConfig;
  }
  val = acct_obj->Get(Nan::New("videoConfig").ToLocalChecked());
  if (val->IsObject()) {
    AccountVideoConfig videoConfig;
    Local<Object> vid_obj = val->ToObject();
    JS2PJ_BOOL(vid_obj, autoShowIncoming, videoConfig);
    JS2PJ_BOOL(vid_obj, autoTransmitOutgoing, videoConfig);
    JS2PJ_UINT(vid_obj, windowFlags, videoConfig);
    JS2PJ_INT(vid_obj, defaultCaptureDevice, videoConfig);
    JS2PJ_INT(vid_obj, defaultRenderDevice, videoConfig);
    JS2PJ_ENUM(vid_obj,
               rateControlMethod,
               pjmedia_vid_stream_rc_method,
               videoConfig);
    JS2PJ_UINT(vid_obj, rateControlBandwidth, videoConfig);

    acct_cfg.videoConfig = videoConfig;
  }

  return acct_cfg;
}

void SIPSTERAccount::onInstantMessage(OnInstantMessageParam &prm)
{
    std::cout << "onInstantMessage from: " << prm.fromUri << ", msg:" << prm.msgBody  << std::endl;
    SETUP_EVENT(INSTANTMESSAGE);
    ev.acct = this;

    args->fromUri = prm.fromUri;
    args->msg = prm.msgBody;

    ENQUEUE_EVENT(ev);
}

void SIPSTERAccount::onRegStarted(OnRegStartedParam &prm) {
  SETUP_EVENT(REGSTARTING);
  ev.acct = this;

  args->renew = prm.renew;

  ENQUEUE_EVENT(ev);
}

void SIPSTERAccount::onRegState(OnRegStateParam &prm) {
  AccountInfo ai = getInfo();

  SETUP_EVENT(REGSTATE);
  ev.acct = this;

  args->active = ai.regIsActive;
  args->statusCode = prm.code;

  ENQUEUE_EVENT(ev);
}

void SIPSTERAccount::onIncomingCall(OnIncomingCallParam &iprm) {
  SIPSTERCall* call = new SIPSTERCall(*this, iprm.callId);
  CallInfo ci = call->getInfo();

  SETUP_EVENT(INCALL);
  ev.call = call;
  ev.acct = this;

  args->localContact = ci.localContact;
  args->remoteContact = ci.remoteContact;
  args->callId = ci.callIdString;
  args->srcAddress = iprm.rdata.srcAddress;

  const char* msgcstr = NULL;
  int res;

  // pjsip replaces uri info if it exceeds 128 characters, so we have to
  // get the real uris from the original SIP message
  if (ci.remoteUri == "<-error: uri too long->") {

    /* TODO
    msgcstr = iprm.rdata.wholeMsg.c_str();
    regmatch_t match[2];
    res = regexec(&fromuri_regex, msgcstr, 2, match, 0);
    if (res == 0) {
      args->remoteUri = string(msgcstr + match[1].rm_so,
                               match[1].rm_eo - match[1].rm_so);
    }
    */
    std::cout << ci.remoteUri << std::endl;
  } else
    args->remoteUri = ci.remoteUri;

  if (ci.localUri == "<-error: uri too long->") {
    /*
    if (!msgcstr)
      msgcstr = iprm.rdata.wholeMsg.c_str();
    regmatch_t match[2];
    res = regexec(&touri_regex, msgcstr, 2, match, 0);
    if (res == 0) {
      args->localUri = string(msgcstr + match[1].rm_so,
                              match[1].rm_eo - match[1].rm_so);
    }
    */
   std::cout << ci.localUri << std::endl;
  } else
    args->localUri = ci.localUri;

  ENQUEUE_EVENT(ev);
}

NAN_METHOD(SIPSTERAccount::New) {
  Nan::HandleScope scope;

  if (!info.IsConstructCall())
    return Nan::ThrowError("Use `new` to create instances of this object.");

  SIPSTERAccount* acct = new SIPSTERAccount();

  AccountConfig acct_cfg;
  string errstr;
  bool isDefault = false;
  if (info.Length() > 0 && info[0]->IsObject()) {
    acct_cfg = genConfig(info[0]->ToObject());

    if (info.Length() > 1 && info[1]->IsBoolean())
      isDefault = info[1]->BooleanValue();
  } else if (info.Length() > 0 && info[0]->IsBoolean())
    isDefault = info[0]->BooleanValue();

  try {
    acct->create(acct_cfg, isDefault);
  } catch(Error& err) {
    delete acct;
    errstr = "Account.create() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  acct->Wrap(info.This());
  acct->Ref();

  acct->emit = new Nan::Callback(
    Local<Function>::Cast(acct->handle()->Get(Nan::New(emit_symbol)))
  );

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(SIPSTERAccount::Modify) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  AccountConfig acct_cfg;
  if (info.Length() > 0 && info[0]->IsObject())
    acct_cfg = genConfig(info[0]->ToObject());
  else
    return Nan::ThrowTypeError("Missing renew argument");

  try {
    std::cout << "sip account:" << acct_cfg.idUri << std::endl;
    std::cout << "registerarUri:" << acct_cfg.regConfig.registrarUri << std::endl;

    acct->modify(acct_cfg);
  } catch(Error& err) {
    string errstr = "Account->modify() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_GETTER(SIPSTERAccount::ValidGetter) {
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());
  info.GetReturnValue().Set(Nan::New(acct->isValid()));
}

NAN_GETTER(SIPSTERAccount::DefaultGetter) {
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());
  info.GetReturnValue().Set(Nan::New(acct->isDefault()));
}

NAN_SETTER(SIPSTERAccount::DefaultSetter) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  if (value->BooleanValue()) {
    try {
      acct->setDefault();
    } catch(Error& err) {
      string errstr = "Account->setDefault() error: " + err.info();
      return Nan::ThrowError(errstr.c_str());
    }
  }
}

NAN_METHOD(SIPSTERAccount::GetInfo) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  AccountInfo ai;
  try {
    ai = acct->getInfo();
  } catch(Error& err) {
    string errstr = "Account->getInfo() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  Local<Object> info_obj = Nan::New<Object>();
  Nan::Set(info_obj,
           Nan::New("uri").ToLocalChecked(),
           Nan::New(ai.uri.c_str()).ToLocalChecked());
  Nan::Set(info_obj,
           Nan::New("regIsConfigured").ToLocalChecked(),
           Nan::New(ai.regIsConfigured));
  Nan::Set(info_obj,
           Nan::New("regIsActive").ToLocalChecked(),
           Nan::New(ai.regIsActive));
  Nan::Set(info_obj,
           Nan::New("regExpiresSec").ToLocalChecked(),
           Nan::New(ai.regExpiresSec));
  // TODO: onlineStatus*, regStatus*, regLastErr?

  info.GetReturnValue().Set(info_obj);
}

NAN_METHOD(SIPSTERAccount::SetRegistration) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  bool renew;
  if (info.Length() > 0 && info[0]->IsBoolean())
    renew = info[0]->BooleanValue();
  else
    return Nan::ThrowTypeError("Missing renew argument");

  try {
    acct->setRegistration(renew);
  } catch(Error& err) {
    string errstr = "Account->setRegistration() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERAccount::SetTransport) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  TransportId tid;
  if (info.Length() > 0
      && Nan::New(SIPSTERTransport_constructor)->HasInstance(info[0])) {
    SIPSTERTransport* trans =
      Nan::ObjectWrap::Unwrap<SIPSTERTransport>(Local<Object>::Cast(info[0]));
    tid = trans->transId;
  } else
    return Nan::ThrowTypeError("Missing Transport instance");

  try {
    acct->setTransport(tid);
  } catch(Error& err) {
    string errstr = "Account->setTransport() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERAccount::MakeCall) {
  Nan::HandleScope scope;

  std::cout << "make call param size:" << info.Length() << std::endl;
  string dest;
  CallOpParam prm;
  int audioDevId = -1;
  if (info.Length() > 0 && info[0]->IsString()) {
    Nan::Utf8String dest_str(info[0]);
    dest = string(*dest_str);

    if (info.Length() > 1 && info[1]->IsString()) {
        Nan::Utf8String param_str(info[1]);

        SipHeader sipHeader;
        sipHeader.hName = "AppMessage";
        sipHeader.hValue = string(*param_str);
        SipHeaderVector sipHeaderVector;
        sipHeaderVector.push_back(sipHeader);
        SipTxOption sipTxOption;
        sipTxOption.headers = sipHeaderVector;
        prm.txOption = sipTxOption;

        std::cout << "call param:" << string(*param_str) << std::endl;
      if (info.Length() > 2 && info[2]->IsInt32()) {
        audioDevId = (int)(info[2]->Int32Value());
        /*
        Local<Object> audioDev_obj = info[2]->ToObject();

        Local<Value> val;
        AudioDevInfo audioDevice;
        JS2PJ_STR(audioDev_obj, name, audioDevice);
        JS2PJ_UINT(audioDev_obj, inputCount, audioDevice);
        JS2PJ_UINT(audioDev_obj, outputCount, audioDevice);
        JS2PJ_STR(audioDev_obj, driver, audioDevice);

        audioDev = new AudioDevInfo();
        audioDev->name = audioDevice.name;
        audioDev->inputCount = audioDevice.inputCount;
        audioDev->outputCount = audioDevice.outputCount;
        audioDev->driver = audioDevice.driver;

        std::cout << "Make call to " << dest << " is use audio device:" << audioDevice.name  << std::endl;
        */
        /*
         && Nan::New(SIPSTERAudioDevInfo_constructor)->HasInstance(info[2])
        audioDev = Nan::ObjectWrap::Unwrap<SIPSTERAudioDevInfo>(Local<Object>::Cast(info[2]));
        */
        if (info.Length() > 3) {
          prm.statusCode = static_cast<pjsip_status_code>(info[3]->Int32Value());
          if (info.Length() > 4 && info[4]->IsString()) {
            Nan::Utf8String reason_str(info[4]);
            prm.reason = string(*reason_str);
          }
        }
      } else if (info.Length() > 2 && info[2]->IsBoolean()) {
        std::cout << "Make call param 3 is not object is boolean" << std::endl; 
      }
    }
  } else
    return Nan::ThrowTypeError("Missing call destination");

  Handle<Value> new_call_args[1] = { info.This() };
  Local<Object> call_obj =
    Nan::New(SIPSTERCall_constructor)->GetFunction()
                                     ->NewInstance(1, new_call_args);
  SIPSTERCall* call = Nan::ObjectWrap::Unwrap<SIPSTERCall>(call_obj);

  try {
    #if 0
    if (audioDev != NULL) {
      std::cout << "Make call to " << dest << " is use audio device:" << audioDev->name  << std::endl;
      call->setAudioDevice(audioDev);
    }
    #endif
    AudDevManager& mgr = Endpoint::instance().audDevManager();
    int oldDeviceId = mgr.getCaptureDev();
    if (audioDevId >= 0) {
      call->setAudoConnect(true);
      if (oldDeviceId != audioDevId) {
        mgr.setCaptureDev(audioDevId);

        AudioDevInfo devInfo = mgr.getDevInfo(mgr.getCaptureDev());
        std::cout << "Set Capture dev to " << audioDevId << ", acture id:" << mgr.getCaptureDev() << ",name" << devInfo.name << std::endl;
      }
    }

    call->makeCall(dest, prm);

    if (audioDevId >= 0 && oldDeviceId != audioDevId) {
      mgr.setCaptureDev(oldDeviceId);
      AudioDevInfo devInfo = mgr.getDevInfo(mgr.getCaptureDev());
      std::cout << "Reset Capture dev to id:" << mgr.getCaptureDev() << ",name" << devInfo.name << std::endl;
    }
  } catch(Error& err) {
    string errstr = "Call.makeCall() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().Set(call_obj);
}


NAN_METHOD(SIPSTERAccount::AddBuddy) {
  Nan::HandleScope scope;

  std::cout << "SIPSTERAccount::AddBuddy" << std::endl;
  
  string buddyUri;
  bool isSubscribePresence = false;

  if (info.Length() > 0 && info[0]->IsString()) {
    Nan::Utf8String dest_str(info[0]);
    buddyUri = string(*dest_str);

    if (info.Length() > 1 && info[1]->IsBoolean()) {
      isSubscribePresence = info[1]->BooleanValue();
    }
  } else
    return Nan::ThrowTypeError("Missing call destination");

  //Handle<Value> new_buddy_args[1] = { info.This() };
  Local<Object> buddy_obj =
    Nan::New(SIPSTERBuddy_constructor)->GetFunction()
                                     ->NewInstance();
  SIPSTERBuddy* buddy = Nan::ObjectWrap::Unwrap<SIPSTERBuddy>(buddy_obj);
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  try {
    BuddyConfig cfg;
    cfg.uri = buddyUri;
    buddy->create(*(acct), cfg);
    buddy->subscribePresence(isSubscribePresence);
    acct->addBuddy(buddy);
    std::cout << "Add buddy " << buddyUri << "is subscribe presence " << isSubscribePresence << std::endl;
  } catch(Error& err) {
    string errstr = "Buddy.creak() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  } catch (...) {
    string errstr = "Buddy.creak() error";
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().Set(buddy_obj);
}


NAN_METHOD(SIPSTERAccount::DelBuddy) {
  Nan::HandleScope scope;

  std::cout << "SIPSTERAccount::DelBuddy" << std::endl;
  
  string buddyUri;
  if (info.Length() > 0 && info[0]->IsString()) {
    Nan::Utf8String dest_str(info[0]);
    buddyUri = string(*dest_str);
  } else
    return Nan::ThrowTypeError("Missing call destination");

  std::cout << "SIPSTERAccount::DelBuddy:" << buddyUri << std::endl;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());
  try {
    Buddy *buddy = acct->findBuddy(buddyUri);
    if (buddy != NULL) {
      //std::cout << "SIPSTERAccount::DelBuddy acct remove buddy" << std::endl;
      buddy->subscribePresence(false);
      acct->removeBuddy(buddy);
      //std::cout << "SIPSTERAccount::DelBuddy acct remove buddy succeed" << std::endl;
      //delete buddy;

      //std::cout << "SIPSTERAccount::DelBuddy succeed" << std::endl;
    }
  } catch(Error& err) {
    string errstr = "Buddy.delete() error: " + err.info();
    return Nan::ThrowError(errstr.c_str());
  } catch (...) {
    string errstr = "Buddy.delete()";
    std::cout << "SIPSTERAccount::DelBuddy fail" << std::endl;
    return Nan::ThrowError(errstr.c_str());
  }

  info.GetReturnValue().SetUndefined();
}



NAN_METHOD(SIPSTERAccount::DoRef) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  acct->Ref();

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(SIPSTERAccount::DoUnref) {
  Nan::HandleScope scope;
  SIPSTERAccount* acct = Nan::ObjectWrap::Unwrap<SIPSTERAccount>(info.This());

  acct->Unref();

  info.GetReturnValue().SetUndefined();
}

void SIPSTERAccount::Initialize(Handle<Object> target) {
  Nan::HandleScope scope;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  Local<String> name = Nan::New("Account").ToLocalChecked();

  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  tpl->SetClassName(name);

  Nan::SetPrototypeMethod(tpl, "modify", Modify);
  Nan::SetPrototypeMethod(tpl, "makeCall", MakeCall);
  Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
  Nan::SetPrototypeMethod(tpl, "setRegistration", SetRegistration);
  Nan::SetPrototypeMethod(tpl, "setTransport", SetTransport);
  Nan::SetPrototypeMethod(tpl, "ref", DoRef);
  Nan::SetPrototypeMethod(tpl, "unref", DoUnref);
  Nan::SetPrototypeMethod(tpl, "addBuddy", AddBuddy);
  Nan::SetPrototypeMethod(tpl, "delBuddy", DelBuddy);

  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("valid").ToLocalChecked(),
                   ValidGetter);
  Nan::SetAccessor(tpl->PrototypeTemplate(),
                   Nan::New("default").ToLocalChecked(),
                   DefaultGetter,
                   DefaultSetter);

  Nan::Set(target, name, tpl->GetFunction());

  SIPSTERAccount_constructor.Reset(tpl);

/*
  int res;
  res = regcomp(&fromuri_regex,
                "^From:.*(<sip:.+>)",
                REG_ICASE|REG_EXTENDED|REG_NEWLINE);
  if (res != 0) {
    char errbuf[128];
    errbuf[0] = '\0';
    regerror(res, &fromuri_regex, errbuf, 128);
    fprintf(stderr, "Could not compile 'From:' URI regex: %s\n", errbuf);
    exit(1);
  }

  res = regcomp(&touri_regex,
                "^To:.*(<sip:.+>)",
                REG_ICASE|REG_EXTENDED|REG_NEWLINE);
  if (res != 0) {
    char errbuf[128];
    errbuf[0] = '\0';
    regerror(res, &touri_regex, errbuf, 128);
    fprintf(stderr, "Could not compile 'To:' URI regex: %s\n", errbuf);
    exit(1);
  }
  */
}

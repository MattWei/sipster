var EventEmitter = require('events').EventEmitter,
    inherits = require('util').inherits;

var addon = require('../build/Release/sipster.node');
addon.Account.prototype.__proto__ = EventEmitter.prototype;
addon.Call.prototype.__proto__ = EventEmitter.prototype;
addon.Media.prototype.__proto__ = EventEmitter.prototype;
addon.Transport.prototype.__proto__ = EventEmitter.prototype;
addon.Buddy.prototype.__proto__ = EventEmitter.prototype;
addon.SipPlatform.prototype.__proto__ = EventEmitter.prototype;

exports = {
  init: addon.init,
  start: addon.start,
  disconnect:addon.destory,
  hangupAllCalls: addon.hangupAllCalls,
  createRecorder: addon.createRecorder,
  createPlayer: addon.createPlayer,
  createPlaylist: addon.createPlaylist,
  systemInit: addon.systemInit,
  setCodecPriority: addon.setCodecPriority,
  
  version: addon.version(),

  get config() {
    return JSON.parse(addon.config());
  },
  get state() {
    return addon.state();
  },
  get mediaActivePorts() {
    return addon.mediaActivePorts();
  },
  get mediaMaxPorts() {
    return addon.mediaMaxPorts();
  },

  get enumDevs() {
    return addon.enumDevs();
  },

  get codecEnum() {
    return addon.codecEnum();
  },

  Transport: addon.Transport,
  Account: addon.Account,
};

var consts = require('./constants'),
    keys = Object.keys(consts),
    keysLen = keys.length;
for (var k = 0, key; k < keysLen; ++k) {
  key = keys[k];
  exports[key] = consts[key];
}

module.exports = exports;

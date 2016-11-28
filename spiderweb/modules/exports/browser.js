const EventEmitter = require('events');
const assert = require('assert');
const webExtension = process.binding('web_extension');

const openPorts = new Map();
const connectEmitter = new EventEmitter();

webExtension.setRecvMessageCallback((message) => {
  message = JSON.parse(message);
  let type = message.type;
  let portId = message.portId;
  switch (type) {
    case 'message':
      let port = openPorts.get(portId);
      assert(port, 'Open port for ' + portId);
      port.emitter.emit('message', message.message);
      break;
    case 'connect':
      connectEmitter.emit('connect', new Port(portId, 'runtime'));
      break;
    default:
      throw new Error('Bad event type ' + type);
  }
});

// Emulate Chromium's Event object using a node EventEmitter.
// TODO: there's a lot more here https://developer.chrome.com/apps/events#type-Event
class Event {
  constructor(name, emitter) {
    this.name = name;
    this.emitter = emitter;
  }

  addListener(callback) {
    this.emitter.addListener(this.name, callback);
  }
}

// Emulate Chromium's Port object.
// https://developer.chrome.com/extensions/runtime#type-Port
class Port {
  constructor(portId, name) {
    this.portId = portId;
    this.name = name;
    openPorts.set(portId, this);
    this.emitter = new EventEmitter();

    this.onDisconnect = new Event('disconnect', this.emitter);
    this.onMessage = new Event('message', this.emitter);
  }

  postMessage(message) {
    webExtension.postMessage({
      portId: this.portId,
      message: message
    });
  }

  disconnect() {
    assert(false, 'TODO: Port.disconnect');
  }
}

exports.onConnect = new Event('connect', connectEmitter);

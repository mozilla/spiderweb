const EventEmitter = require('events');
const assert = require('assert');
const webExtension = process.binding('web_extension');

let globalPortId = 1;
const openPorts = new Map();

webExtension.setRecvMessageCallback((message) => {
  message = JSON.parse(message);
  let portId = message.portId;
  let port = openPorts.get(portId);
  assert(port, 'Open port for ' + portId);
  port.emitter.emit('message', message.message);
});

// Emulate Chromium's Event object.
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

exports.runtime = {
  connect: function () {
    return new Port(globalPortId++, 'runtime');
  }
};

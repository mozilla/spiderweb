const path = require('path');
const Module = require('module');
const timers = require('timers');

// setImmediate and process.nextTick makes use of uv_check and uv_prepare to
// run the callbacks, however since we only run uv loop on requests, the
// callbacks wouldn't be called until something else activated the uv loop,
// which would delay the callbacks for arbitrary long time. So we should
// initiatively activate the uv loop once setImmediate and process.nextTick is
// called.
var wrapWithActivateUvLoop = function (func) {
  return function () {
    process.activateUvLoop();
    return func.apply(this, arguments);
  }
};

process.nextTick = wrapWithActivateUvLoop(process.nextTick);

global.setImmediate = wrapWithActivateUvLoop(timers.setImmediate);

global.clearImmediate = timers.clearImmediate;

// setTimeout needs to update the polling timeout of the event loop, when
// called under Chromium's event loop the node's event loop won't get a chance
// to update the timeout, so we have to force the node's event loop to
// recalculate the timeout in browser process.
global.setTimeout = wrapWithActivateUvLoop(timers.setTimeout);
global.setInterval = wrapWithActivateUvLoop(timers.setInterval);

var globalPaths = Module.globalPaths;

// Expose public APIs.
globalPaths.push(path.join(__dirname, 'exports'))

// TODO: in the future the path to the script should probably come from the
// parent process as an absolute path. Temporarily for testing resolve it
// relative to the working directory.
const mainStartupScript = process.argv[2];
Module._load(mainStartupScript, Module, true)

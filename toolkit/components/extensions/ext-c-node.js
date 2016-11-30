"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

let nextPortId = 1;

extensions.registerSchemaAPI("node", "addon_child", context => {
  let {extension} = context;
  return {
    node: {
      connect: () => {
        let nodeLoader = Cc["@mozilla.org/spiderweb/nodeloader;1"]
                         .getService(Ci.nsINodeLoader);
        let portId = nextPortId++;

        let uri = NetUtil.newURI(extension.manifest.node);
        let fullPath = uri.QueryInterface(Ci.nsIFileURL).file.path;

        // TODO: make it possible to add multiple event listeners, for a demo only
        // one is needed
        let onMessage;
        // TODO: node should only be started once
        nodeLoader.start(fullPath, (msg) => {
          if (!onMessage) {
            throw new Error("No one is listening.");
          }
          // call port on message
          onMessage(JSON.parse(msg).message);
        });

        nodeLoader.postMessage(JSON.stringify({type: "connect", portId: portId}));

        let port = {
          name: "node",

          disconnect: () => {
            throw new Error("todo");
          },

          postMessage: msg => {
            nodeLoader.postMessage(JSON.stringify({type: "message", portId: portId, message: msg}));
          },

          onDisconnect: () => {
            throw new Error("todo");
          },

          onMessage: {
            addListener: listener => {
              if (onMessage) {
                throw new Error("Only one listener for now.");
              }
              onMessage = listener;
            }
          }
        };

        port = Cu.cloneInto(port, context.cloneScope, {cloneFunctions: true});

        return port;
      },
    },
  };
});

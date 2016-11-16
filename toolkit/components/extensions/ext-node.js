"use strict";

extensions.registerSchemaAPI("node", "addon_parent", context => {
  let {extension} = context;
  return {
    node: {
      start: () => {
        // TODO: call in to c++ and start node
      },
    },
  };
});

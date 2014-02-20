
function evalWithCache(code, ctx) {
  ctx = ctx || {};
  ctx = Object.create(ctx, {
    fileName: { value: "evalWithCacheCode.js" },
    lineNumber: { value: 0 }
  });
  code = code instanceof Object ? code : cacheEntry(code);

  // We create a new global
  if (!("global" in ctx))
    ctx.global = newGlobal();

  // The generation counter is used to represent environment variations which
  // might cause the program to run differently, and thus to have a different
  // set of functions executed.
  ctx.global.generation = 0;
  var res1 = evaluate(code, Object.create(ctx, {saveBytecode: { value: true } }));

  ctx.global.generation = 1;
  var res2 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true }, saveBytecode: { value: true } }));

  ctx.global.generation = 2;
  var res3 = evaluate(code, Object.create(ctx, {loadBytecode: { value: true } }));

  ctx.global.generation = 3;
  var res0 = evaluate(code, ctx);

  if (ctx.assertEqResult) {
    assertEq(res0, res1);
    assertEq(res0, res2);
    assertEq(res0, res3);
  }
}

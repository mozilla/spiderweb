/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "node.h"
#include "NodeBindings.h"

namespace {

v8::Handle<v8::String> ToJSON(v8::Isolate* isolate, v8::Handle<v8::Value> object) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  // Get JSON stringify from the global
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Object> JSON = v8::Local<v8::Object>::Cast(
      global->Get(v8::String::NewFromUtf8(isolate, "JSON")));
  v8::Local<v8::Function> stringify = v8::Local<v8::Function>::Cast(
      JSON->Get(v8::String::NewFromUtf8(isolate, "stringify")));

  // Call stringify on the object
  v8::Local<v8::Value> args[] = { object };
  return v8::Local<v8::String>::Cast(stringify->Call(JSON, 1, args));
}

void PostMessage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  // Check the number of arguments passed.
  if (args.Length() != 1) {
    // Throw an Error that is passed back to JavaScript
    isolate->ThrowException(v8::Exception::TypeError(
        v8::String::NewFromUtf8(isolate, "Wrong number of arguments")));
    return;
  }
  v8::Local<v8::String> message = ToJSON(isolate, args[0]);
  mozilla::NodeBindings* nodeBindings = mozilla::NodeBindings::Instance();
  nodeBindings->SendMessage(message);
}

void SetMessageCallback(const v8::FunctionCallbackInfo<v8::Value>&args) {
  mozilla::NodeBindings::Instance()->SetRecvMessageCallback(args.GetIsolate(), v8::Local<v8::Function>::Cast(args[0]));
}

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context, void* priv) {
  NODE_SET_METHOD(exports, "postMessage", PostMessage);
  NODE_SET_METHOD(exports, "setRecvMessageCallback", SetMessageCallback);
}

}  // namespace

NODE_MODULE_CONTEXT_AWARE_BUILTIN(web_extension, Initialize)

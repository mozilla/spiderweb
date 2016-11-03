// Copyright (c) 2013 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef MOZILLA_COMMON_NODE_BINDINGS_H_
#define MOZILLA_COMMON_NODE_BINDINGS_H_

#include <list>
#include "uv.h"
#include "v8.h"
#include "nsISupportsImpl.h"

class MessageLoop;

namespace node {
class Environment;
}

namespace mozilla {

class NodeBindings {
 public:
  // Needed to use message loop's PostTask
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::NodeBindings);

  static NodeBindings* Create();

  // Setup V8, libuv.
  void Initialize(int argc, char** argv);

  // Create the environment and load node.js.
  ::node::Environment* CreateEnvironment(v8::Handle<v8::Context> context,
                                        int argc,
                                        char** argv);

  // Load node.js in the environment.
  void LoadEnvironment(::node::Environment* env);

  // Prepare for message loop integration.
  void PrepareMessageLoop();

  // Do message loop integration.
  virtual void RunMessageLoop();

  // Gets/sets the environment to wrap uv loop.
  void set_uv_env(::node::Environment* env) { uv_env_ = env; }
  ::node::Environment* uv_env() const { return uv_env_; }

  void ActivateUVLoop(v8::Isolate* isolate);

 protected:
  explicit NodeBindings();
  // Made protected for ref counting.
  virtual ~NodeBindings();

  // Called to poll events in new thread.
  virtual void PollEvents() = 0;

  // Run the libuv loop for once.
  void UvRunOnce();

  // Make the main thread run libuv loop.
  void WakeupMainThread();

  // Interrupt the PollEvents.
  void WakeupEmbedThread();

  // Main thread's MessageLoop.
  MessageLoop* message_loop_;

  // Main thread's libuv loop.
  uv_loop_t* uv_loop_;

 private:
  // Thread to poll uv events.
  static void EmbedThreadRunner(void *arg);

  // Whether the libuv loop has ended.
  bool embed_closed_;

  // Dummy handle to make uv's loop not quit.
  uv_async_t dummy_uv_handle_;

  // Thread for polling events.
  uv_thread_t embed_thread_;

  // Semaphore to wait for main loop in the embed thread.
  uv_sem_t embed_sem_;

  // Environment that to wrap the uv loop.
  ::node::Environment* uv_env_;

  static void OnCallNextTick(uv_async_t* handle);
  uv_async_t call_next_tick_async_;
  std::list<::node::Environment*> pending_next_ticks_;
  void PreMainMessageLoopRun();
  v8::Isolate::Scope* isolate_scope;
  v8::Isolate* isolate;
};

}  // namespace mozilla

#endif  // MOZILLA_COMMON_NODE_BINDINGS_H_

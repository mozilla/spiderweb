/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeProcessParent.h"
#include "nsIFile.h"
#include "nsIRunnable.h"

#include "base/string_util.h"
#include "base/process_util.h"

#include <string>

using std::vector;
using std::string;

using mozilla::node::NodeProcessParent;
using mozilla::ipc::GeckoChildProcessHost;
using base::ProcessArchitecture;

namespace mozilla {
namespace node {

NodeProcessParent::NodeProcessParent()
: GeckoChildProcessHost(GeckoProcessType_Node)
{
  MOZ_COUNT_CTOR(NodeProcessParent);
}

NodeProcessParent::~NodeProcessParent()
{
  MOZ_COUNT_DTOR(NodeProcessParent);
}

bool
NodeProcessParent::Launch(int32_t aTimeoutMs)
{
  vector<string> args;

  // Determine the binary path and push it onto the args.
  //
  // GMPProcessParent does this too, but then it calls SyncLaunch,
  // which eventually calls GeckoChildProcessHost::PerformAsyncLaunchInternal,
  // which calls GeckoChildProcessHost::GetPathToBinary and prepends the result
  // to the args.  So perhaps we need to modify GetPathToBinary to determine
  // the binary path for the Node binary.
  //
  // TODO: figure that out.
  //
//   FilePath exePath;
// #ifdef OS_WIN
//   exePath = FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
// #else
//   exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
// #endif
//   exePath = exePath.DirName();
//   exePath = exePath.AppendASCII("spiderweb");
//   args.push_back(exePath.value());

  // TODO: Use AsyncLaunch in our case?
  return SyncLaunch(args, aTimeoutMs, base::GetCurrentProcessArchitecture());
}

void
NodeProcessParent::Delete()
{
  MessageLoop* currentLoop = MessageLoop::current();
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();

  if (currentLoop == ioLoop) {
    Join();
    delete this;
    return;
  }

  ioLoop->PostTask(NewNonOwningRunnableMethod(this, &NodeProcessParent::Delete));
}

} // namespace node
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeChild.h"
#include "NodeProcessChild.h"
#include "mozilla/ipc/ProcessChild.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace node {

MOZ_IMPLICIT NodeChild::NodeChild()
{
  MOZ_COUNT_CTOR(NodeChild);
}

MOZ_IMPLICIT NodeChild::~NodeChild()
{
  MOZ_COUNT_DTOR(NodeChild);
}

bool
NodeChild::Init(base::ProcessId aParentPid,
                MessageLoop* aIOLoop,
                IPC::Channel* aChannel)
{
  if (NS_WARN_IF(!Open(aChannel, aParentPid, aIOLoop))) {
    return false;
  }

  return SendPing();
}

IPCResult
NodeChild::RecvPong()
{
  printf("Pong!\n");
  return IPC_OK();
}

} // namespace node
} // namespace mozilla

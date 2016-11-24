/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeParent.h"

namespace mozilla {
namespace node {

using namespace mozilla::ipc;

NodeParent::NodeParent()
  : mProcess(nullptr)
{
  MOZ_COUNT_CTOR(NodeParent);
}

MOZ_IMPLICIT NodeParent::~NodeParent()
{
  MOZ_COUNT_DTOR(NodeParent);
  MOZ_ASSERT(!mProcess);
}

void
NodeParent::Init()
{
}

nsresult
NodeParent::LaunchProcess()
{
  MOZ_ASSERT(!mProcess);

  mProcess = new NodeProcessParent();

  if (!mProcess->Launch(30 * 1000)) {
    mProcess->Delete();
    mProcess = nullptr;
    return NS_ERROR_FAILURE;
  }

  if (!Open(mProcess->GetChannel(),
            base::GetProcId(mProcess->GetChildProcessHandle()))) {
    mProcess->Delete();
    mProcess = nullptr;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
NodeParent::DeleteProcess()
{
  Close();

  mProcess->Delete();
  mProcess = nullptr;
}

mozilla::ipc::IPCResult
NodeParent::RecvPing()
{
  printf("Ping!\n");
  if (SendPong()) {
    return IPC_OK();
  }
  return IPC_FAIL_NO_REASON(this);
}

void
NodeParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace node
} // namespace mozilla

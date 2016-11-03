/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeParent.h"

namespace mozilla {
namespace node {

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

bool
NodeParent::RecvPing()
{
  printf("Ping!\n");

  // TODO move the node init code out of Ping

  // Spidernode needs the path to the ICU data.
#if EXPOSE_INTL_API && defined(MOZ_ICU_DATA_ARCHIVE)
  nsAutoCString icuDataPath(u_getDataDirectory());
#else
  nsAutoCString icuDataPath("");
#endif

  // TODO remove hardcoded init script and use value from extension
  nsTArray<nsCString> args;
  args.AppendElement(NS_LITERAL_CSTRING("node"));
  args.AppendElement(NS_LITERAL_CSTRING("test.js"));
  if (!SendStartNode(args, icuDataPath)) {
    return false;
  }
  return SendPong();
}

void
NodeParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace node
} // namespace mozilla

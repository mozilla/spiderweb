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

NodeProcessParent::NodeProcessParent(const std::string& aNodePath)
: GeckoChildProcessHost(GeckoProcessType_Node),
  mNodePath(aNodePath)
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
  return false;
}

void
NodeProcessParent::Delete(nsCOMPtr<nsIRunnable> aCallback)
{
  mDeletedCallback = aCallback;
  XRE_GetIOMessageLoop()->PostTask(NewNonOwningRunnableMethod(this, &NodeProcessParent::DoDelete));
}

void
NodeProcessParent::DoDelete()
{
  MOZ_ASSERT(MessageLoop::current() == XRE_GetIOMessageLoop());
  Join();

  if (mDeletedCallback) {
    mDeletedCallback->Run();
  }

  delete this;
}

} // namespace node
} // namespace mozilla

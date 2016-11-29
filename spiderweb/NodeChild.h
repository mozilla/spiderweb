/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/node/PNodeChild.h"

namespace mozilla {
namespace node {

class NodeChild : public PNodeChild
{
public:
  virtual mozilla::ipc::IPCResult RecvStartNode(nsTArray<nsCString>&& aInitArgs,
                                                const nsCString& aICUDataDir);
  virtual mozilla::ipc::IPCResult RecvMessage(const nsCString& aMessage);

  MOZ_IMPLICIT NodeChild();
  virtual ~NodeChild();

  bool Init(base::ProcessId aParentPid,
            MessageLoop* aIOLoop,
            IPC::Channel* aChannel);

private:

};

} // namespace node
} // namespace mozilla

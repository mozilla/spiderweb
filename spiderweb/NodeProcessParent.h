/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/Attributes.h"
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/thread.h"
#include "chrome/common/child_process_host.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

class nsIRunnable;

namespace mozilla {
namespace node {

class NodeProcessParent final : public mozilla::ipc::GeckoChildProcessHost
{
public:
  explicit NodeProcessParent();
  ~NodeProcessParent();

  bool Launch(int32_t aTimeoutMs);

  void Delete();

  bool CanShutdown() override { return true; }

  using mozilla::ipc::GeckoChildProcessHost::GetChannel;
  using mozilla::ipc::GeckoChildProcessHost::GetChildProcessHandle;

private:
  DISALLOW_COPY_AND_ASSIGN(NodeProcessParent);
};

} // namespace node
} // namespace mozilla

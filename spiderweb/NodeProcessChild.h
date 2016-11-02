/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/ipc/ProcessChild.h"
#include "NodeChild.h"

namespace mozilla {
namespace node {

class NodeLoader;

class NodeProcessChild final : public mozilla::ipc::ProcessChild {
protected:
  typedef mozilla::ipc::ProcessChild ProcessChild;

public:
  explicit NodeProcessChild(ProcessId aParentPid);
  ~NodeProcessChild();

  bool Init() override;
  void CleanUp() override;

private:
  DISALLOW_COPY_AND_ASSIGN(NodeProcessChild);
  NodeChild mNodeChild;
};

} // namespace node
} // namespace mozilla

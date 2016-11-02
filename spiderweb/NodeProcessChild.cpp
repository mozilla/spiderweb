/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeProcessChild.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "mozilla/ipc/IOThreadChild.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace node {

NodeProcessChild::NodeProcessChild(ProcessId aParentPid)
: ProcessChild(aParentPid)
{
}

NodeProcessChild::~NodeProcessChild()
{
}

bool
NodeProcessChild::Init()
{
  return mNodeChild.Init(ParentPid(),
                         IOThreadChild::message_loop(),
                         IOThreadChild::channel());
}

void
NodeProcessChild::CleanUp()
{
}

} // namespace node
} // namespace mozilla

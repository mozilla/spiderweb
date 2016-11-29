/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "NodeProcessParent.h"
#include "mozilla/node/PNodeParent.h"

class nsINodeObserver;

namespace mozilla {
namespace node {

class NodeParent : public PNodeParent
{
public:
  NodeParent(const nsACString& script, nsINodeObserver* observer);
  virtual ~NodeParent();

  void
  Init();

  nsresult
  LaunchProcess();

  void
  DeleteProcess();

private:
  NodeProcessParent* mProcess;
  nsCOMPtr<nsINodeObserver> mNodeObserver;
  nsCString mScript;

  virtual mozilla::ipc::IPCResult
  RecvMessage(const nsCString& aMessage);

  virtual void
  ActorDestroy(ActorDestroyReason aWhy);
};

} // namespace node
} // namespace mozilla

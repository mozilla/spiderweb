/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NodeParent.h"

namespace mozilla {
namespace node {

NodeParent::NodeParent()
{
  MOZ_COUNT_CTOR(NodeParent);
}

MOZ_IMPLICIT NodeParent::~NodeParent()
{
  MOZ_COUNT_DTOR(NodeParent);
}

void
NodeParent::Init()
{
}

bool
NodeParent::RecvPing()
{
  printf("Ping!\n");
  return SendPong();
}

void
NodeParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace node
} // namespace mozilla

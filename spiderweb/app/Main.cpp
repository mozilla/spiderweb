/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "node.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"
#include "GMPLoader.h"

extern "C" {
  void _register_async_wrap(void);
  void _register_cares_wrap(void);
  void _register_fs_event_wrap(void);
  void _register_js_stream(void);
  void _register_buffer(void);
  void _register_config(void);
  void _register_contextify(void);
  void _register_crypto(void);
  void _register_fs(void);
  void _register_http_parser(void);
  void _register_icu(void);
  void _register_os(void);
  void _register_util(void);
  void _register_v8(void);
  void _register_zlib(void);
  void _register_pipe_wrap(void);
  void _register_process_wrap(void);
  void _register_signal_wrap(void);
  void _register_spawn_sync(void);
  void _register_stream_wrap(void);
  void _register_tcp_wrap(void);
  void _register_timer_wrap(void);
  void _register_tls_wrap(void);
  void _register_tty_wrap(void);
  void _register_udp_wrap(void);
  void _register_uv(void);
}

int main(int argc, char* argv[])
{
  // We never actually want to call this code, but when these aren't here they
  // seem to be optimized away.
  // TODO: Find a better way to make sure they don't disappear.
  if (!argc && argv) {
    _register_async_wrap();
    _register_cares_wrap();
    _register_fs_event_wrap();
    _register_js_stream();
    _register_buffer();
    _register_config();
    _register_contextify();
    _register_crypto();
    _register_fs();
    _register_http_parser();
    _register_icu();
    _register_os();
    _register_util();
    _register_v8();
    _register_zlib();
    _register_pipe_wrap();
    _register_process_wrap();
    _register_signal_wrap();
    _register_spawn_sync();
    _register_stream_wrap();
    _register_tcp_wrap();
    _register_timer_wrap();
    _register_tls_wrap();
    _register_tty_wrap();
    _register_udp_wrap();
    _register_uv();
  }

  XRE_SetProcessType(argv[--argc]);

  XREChildData childData;

  printf("Node child process about to XRE_InitChildProcess\n");
  nsresult rv = XRE_InitChildProcess(argc, argv, &childData);
  NS_ENSURE_SUCCESS(rv, 1);

  node::Start(argc, argv);

  return 0;
}

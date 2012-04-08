/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>

/* this is a copy of test-async.  this is just so that I can
 * build up a few requests and test that uv_current_tick will
 * return the proper value
 */

static uv_prepare_t prepare_handle;

static uv_async_t async1_handle;

static int prepare_cb_called = 0;

static volatile int async1_cb_called = 0;
static int async1_closed = 0;

static int close_cb_called = 0;

static uintptr_t thread1_id = 0;


/* Thread 1 makes sure that async1_cb_called reaches 3 before exiting. */
void thread1_entry(void *arg) {
  uv_sleep(50);

  while (1) {
    switch (async1_cb_called) {
      case 0:
        uv_async_send(&async1_handle);
        break;

      case 1:
        uv_async_send(&async1_handle);
        break;

      case 2:
        uv_async_send(&async1_handle);
        break;

      default:
        return;
    }
  }
}

static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);
  close_cb_called++;
}


static void async1_cb(uv_async_t* handle, int status) {
  ASSERT(handle == &async1_handle);
  ASSERT(status == 0);

  async1_cb_called++;
  printf("async1_cb #%d\n", async1_cb_called);

  if (async1_cb_called > 2 && !async1_closed) {
    async1_closed = 1;
    uv_close((uv_handle_t*)handle, close_cb);
  }
}


static void prepare_cb(uv_prepare_t* handle, int status) {
  ASSERT(handle == &prepare_handle);
  ASSERT(status == 0);

  switch (prepare_cb_called) {
    case 0:
      thread1_id = uv_create_thread(thread1_entry, NULL);
      ASSERT(thread1_id != 0);
      break;

    case 1:
      uv_close((uv_handle_t*)handle, close_cb);
      break;

    default:
      FATAL("Should never get here");
  }

  prepare_cb_called++;
}


TEST_IMPL(async) {
  int r;
  
  r = uv_prepare_init(uv_default_loop(), &prepare_handle);
  ASSERT(r == 0);
  r = uv_prepare_start(&prepare_handle, prepare_cb);
  ASSERT(r == 0);

  r = uv_async_init(uv_default_loop(), &async1_handle, async1_cb);
  ASSERT(r == 0);

  r = uv_run(uv_default_loop());
  ASSERT(r == 0);

  r = uv_wait_thread(thread1_id);
  ASSERT(r == 0);

  ASSERT(prepare_cb_called == 2);
  ASSERT(async1_cb_called > 2);
  ASSERT(close_cb_called == 2);
  
  // here is the business end of the this test
  ASSERT(3 == uv_current_tick(uv_default_loop()));
  
  uv_tick_me_off(uv_default_loop());
  
  ASSERT(4 == uv_current_tick(uv_default_loop()));
  // end the business

  return 0;
}

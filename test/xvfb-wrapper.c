/*
 * Copyright © 2014 Ran Benita <ran234@gmail.com>
 * Copyright © 2023 Pierre Le Marre <dev@wismill.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <spawn.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "test.h"
#include "xvfb-wrapper.h"
#include "xkbcommon/xkbcommon-x11.h"

int xvfb_wrapper(int (*f)(void)) {
    int ret;
    char display[512];
    char *envp[] = { NULL };
    char *xvfb_argv[] = {
        (char *) "Xvfb", display, NULL
    };
    pid_t xvfb_pid = 0;
    char *xhost = NULL;
    int xdpy_current = 0;
    int xdpy_candidate;

    /*
     * What all of this mess does is:
     * 1. Launch Xvfb on available DISPLAY.
     * 2. Make an xcb connection to this display.
     * 3. Launch xkbcomp to change the keymap of the new display (doing
     *    this programmatically is major work [which we may yet do some
     *    day for xkbcommon-x11] so we use xkbcomp for now).
     * 4. Download the keymap back from the display using xkbcommon-x11.
     * 5. Compare received keymap to the uploaded keymap.
     * 6. Kill the server & clean up.
     */

    /*
     * Detect current display, in order to avoid reusing it.
     * It is OK if it fails (e.g. in headless mode).
     */
    xcb_parse_display(NULL, &xhost, &xdpy_current, NULL);

    /*
     * IANA assigns TCP port numbers from 6000 through 6063 to X11
     * clients.  In addition, the current XCB implementation shows
     * that, when an X11 client tries to establish a TCP connetion,
     * the port number needed is specified by adding 6000 to a given
     * display number.  So, one of reasonable ranges of xdpy_candidate
     * is [0, 63].
     */
    for (xdpy_candidate = 63; xdpy_candidate >= 0; xdpy_candidate--) {
        if (xdpy_candidate == xdpy_current) {
            continue;
        }
        snprintf(display, sizeof(display),
                 "%s:%d", (xhost != NULL) ? xhost : "",
                 xdpy_candidate);
        ret = posix_spawnp(&xvfb_pid, "Xvfb", NULL, NULL, xvfb_argv, envp);
        if (ret == 0) {
            break;
        }
    }
    free(xhost);

    if (ret != 0) {
        ret = SKIP_TEST;
        goto err_xvfd;
    }

    /* Wait for Xvfb fully waking up to accept a connection from a client. */
    sleep(1);

    /* Run the function requiring a running X server */
    ret = f();

err_xvfd:
    if (xvfb_pid > 0)
        kill(xvfb_pid, SIGTERM);
    return ret;
}

//
//  SLARunVLC.h
//  SwitchLiveATC
//
// Start and control VLC to play a LiveATC stream
//

/*
 * Copyright (c) 2019, Birger Hoppe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef SLARunVLC_h
#define SLARunVLC_h

// parameters to use if not overriden in advanced settings
#define VLC_DEFAULT_PARAM   "-I dummy"

#define ERR_SHELL_FAILED    "ShellExecute failed with %lu - %s"
#define ERR_FORK_FAILED     "Fork failed with %d - %s"
#define ERR_EXEC_VLC        "Could not start '%s' for playing '%s'"
#define ERR_ERRNO           "%d - %s"
#define ERR_SIGNAL          "Signal %d"

#define DBG_POPEN_PID       "pid is %d for command '%s'"

// stop still running VLC instances
void RVStopAll();

// start a VLC instance with the given stream to play
// (all else is read from the config)
bool RVPlayStream(const std::string& playUrl);

#endif /* SLARunVLC_h */

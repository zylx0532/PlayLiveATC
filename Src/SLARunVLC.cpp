//
//  SLARunVLC.cpp
//
// Start and control VLC to play a LiveATC stream

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

#include "SwitchLiveATC.h"

#if IBM
#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#endif

//
// MARK: Individual Thread controlling VLC
//

int vlcPid = 0;

void RVDoPlayStream (const std::string& playUrl)
{
#if IBM

#else
    // Idea is: We call the command line to start VLC in the background.
    //          Doing so returns a text like
    //            [1]  <pid>
    //          on the command line, ie. given the pid we later need to kill
    
    char vlc[256];
    char url[256];
    strcpy_s (vlc, sizeof(vlc), dataRefs.GetVLCPath().c_str());
    strcpy_s (url, sizeof(url), playUrl.c_str());

    char* const params[3] = {
        vlc,
        url,
        NULL
    };
    
    
    int pid = fork();
    if (pid == 0) {
        // child shall run VLC
        execvp(vlc, params);
        // we only get here if the above call failed to find 'vlc', return the error code to the caller:
        _Exit(errno);
    } else if (pid < 0) {
        // error during fork
        SHOW_MSG(logERR, ERR_EXEC_VLC, vlc, url);
        LOG_MSG(logERR, ERR_FORK_FAILED, errno, std::strerror(errno));
    } else {
        // fork successful...other process should be running now
        vlcPid = pid;
        LOG_MSG(logDEBUG, DBG_POPEN_PID, vlcPid, vlc);

        // wait for 2 seconds and then check if the child already terminated
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // expected is that the child is still running!
        int stat = 0;
        if (wait4(vlcPid, &stat, WNOHANG, NULL) != 0) {
            // exited already!
            SHOW_MSG(logERR, ERR_EXEC_VLC, vlc, url);
            // we return errno via _Exit, so fetch it and output it into the log
            if (WIFEXITED(stat)) {
                LOG_MSG(logERR, ERR_ERRNO, WEXITSTATUS(stat), std::strerror(WEXITSTATUS(stat)));
            } else if (WIFSIGNALED(stat)) {
                LOG_MSG(logERR, ERR_SIGNAL, WTERMSIG(stat));
            }
        }
    }
#endif
}

//
// MARK: Thread-controlling functions called from outside
//

// stop still running VLC instances
void RVStopAll()
{
#if IBM

#else
    if (vlcPid > 0) {
        kill(vlcPid, SIGTERM);
    }
    vlcPid = 0;
#endif
}

// start a VLC instance with the given stream to play
// (all else is read from the config)
bool RVPlayStream(const std::string& playUrl)
{
    RVStopAll();
    RVDoPlayStream(playUrl);
//    std::thread thrVLC(RVDoPlayStream, playUrl);
//    auto hThr = thrVLC.native_handle();
//    thrVLC.detach();
//    std::this_thread::sleep_for(std::chrono::seconds(5));
//    if (hThr != NULL)
//        pthread_cancel(hThr);
    return true;
}


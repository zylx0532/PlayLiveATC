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
#include <shellapi.h>
#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#endif

//
// MARK: Globals
//

// one COM channel per COM channel - obviously ;)
COMChannel gChn[COM_CNT] = {
    {0}, {1}
};

//
// MARK: Class representing one COM channel, which can run a VLC stream
//

// stop VLC, reset frequency
void COMChannel::ClearChannel()
{
    StopStreamAsync();
    frequ = 0;
    frequString.clear();
}


// if _new differs from frequ
// THEN we consider it a change to a new frequency
bool COMChannel::doChange(int _new)
{
    if (_new != frequ) {
        char buf[20];
        frequ = _new;
        snprintf(buf, sizeof(buf), "%d.%03d", frequ / 1000, frequ % 1000);
        frequString = buf;
        return true;
    } else {
        return false;
    }
}

// VLC play control

void COMChannel::StartStreamAsync (std::string&& url)
{
    // we don't need control over the thread, it lasts for 2s only
    futVlcStart = std::async(std::launch::async, &COMChannel::StartStream, this, url);
}

void COMChannel::StopStreamAsync ()
{
    // we don't need control over the thread, it lasts for 2s only
    futVlcStart = std::async(std::launch::async, &COMChannel::StopStream, this);
}

void COMChannel::StartStream (std::string&& url)
{
    // make sure there is no stream running at the moment
    StopStream();
    
    // store our new URL
    playUrl = std::move(url);
    
    // character-array access to what we need for calls
    // calls to execvp/ShellExecuteExA don't work with string::c_str() pointers
    char vlc[256];
    char params[512];
    strcpy_s(vlc, sizeof(vlc), dataRefs.GetVLCPath().c_str());
    std::string allParams(GetVlcParams());
    strcpy_s(params, sizeof(params), allParams.c_str());

    LOG_MSG(logDEBUG, DBG_RUN_CMD, vlc, allParams.c_str());

#if IBM
    
    SHELLEXECUTEINFO exeVlc = { 0 };
    exeVlc.cbSize   = sizeof(exeVlc);
    exeVlc.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    exeVlc.lpVerb = NULL;
    exeVlc.lpFile = vlc;
    exeVlc.lpParameters = params;
    exeVlc.nShow = SW_SHOWNOACTIVATE;
    if (ShellExecuteExA(&exeVlc)) {
        // save process handle of VLC process for later kill
        vlcProc = exeVlc.hProcess;
    }
    else
    {
        SHOW_MSG(logERR, ERR_EXEC_VLC, vlc, params);
        LOG_MSG(logERR, ERR_SHELL_FAILED, GetLastError(), GetErrorStr().c_str());
    }
#else
    // Mac/Linux:
    // Classic fork/execvp approach
    
    // execvp requires parameters in individual array elements
    // so we separate them at the spaces as a command line would do, too
    std::vector<char*> vParams;             // this will become the vector of pointers pointing to the individual parameters
    vParams.push_back(vlc);                 // by convention 0th parameter is path to executable
    vParams.push_back(params);              // here starts the 1st actual parameter
    for (char* p = strchr(params, ' ');
         p != NULL;
         p = strchr(p+1, ' '))
    {
        *p = '\0';                          // zero-terminate individual parameter, overwriting the space
        if (*(p+1) != ' ' &&                // this handles the case of multiple spaces
            *(p+1) != '\0')                 // ...and string end
            vParams.push_back(p+1);
    }
    vParams.push_back(NULL);                // need a terminating NULL for execvp

    // now do the fork
    vlcPid = fork();
    
    if (vlcPid == 0) {
        // CHILD PROCESS - which is to start VLC
        
        // Inactivate all signal handlers, they point into core XP code!
        for (int sig = SIGHUP; sig <= SIGUSR2; sig++)
            signal(sig, SIG_DFL);
        
        // child shall just and only run VLC
        execvp(vlc, vParams.data());
        
        // we only get here if the above call failed to find 'vlc', return the error code to the caller
        // without triggering any signal handlers (well...double safety...we just removed them anyway)
        _Exit(errno);
        
    } else if (vlcPid < 0) {
        // error during fork
        SHOW_MSG(logERR, ERR_EXEC_VLC, vlc, allParams.c_str());
        LOG_MSG(logERR, ERR_FORK_FAILED, errno, std::strerror(errno));
    } else {
        // fork successful...other process should be running now
        LOG_MSG(logDEBUG, DBG_FORK_PID, vlcPid, playUrl.c_str());
        
        // wait for 2 seconds and then check if the child already terminated
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // expected is that the child is still running!
        int stat = 0;
        if (wait4(vlcPid, &stat, WNOHANG, NULL) != 0) {
            // exited already!
            vlcPid = 0;
            SHOW_MSG(logERR, ERR_EXEC_VLC, vlc, allParams.c_str());
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

void COMChannel::StopStream ()
{
#if IBM
    if (vlcProc) {
        // we send WM_QUIT, just in case...seems to clean up tray icon at least
        HWND hwndVLC = FindMainWindow(vlcProc);
        if (hwndVLC) {
            PostThreadMessageA(GetWindowThreadProcessId(hwndVLC, NULL), WM_QUIT, 0, 0);
            // can WaitForSingleObject do this wait "better"?
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        
        // this is not nice...but VLC doesn't react properly to WM_QUIT or WM_DESTROY: The window's gone, but not the process.
        TerminateProcess(vlcProc, 0);
        vlcProc = NULL;
    }
#else
    if (vlcPid > 0) {
        LOG_MSG(logDEBUG, DBG_KILL_PID, vlcPid, playUrl.c_str());
        kill(vlcPid, SIGTERM);
    }
    vlcPid = 0;
#endif
}

// compute complete parameter string for VLC
std::string COMChannel::GetVlcParams()
{
    std::string params = dataRefs.GetVLCParams();
    // FIXME: Actual desync time!!!
    str_replace(params, PARAM_REPL_DESYNC, "0");
    str_replace(params, PARAM_REPL_URL, playUrl);
    return params;
}

//
// MARK: Global functions using COMChannel
//

// stop still running VLC instances
// (synchronously! Expected to be called during deactivation/shutdown only)
void RVStopAll()
{
    for (COMChannel& chn: gChn)
        chn.StopStream();
}


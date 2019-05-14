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

#define ERR_SHELL_FAILED    "ShellExecute failed with %lu - %s"
#define ERR_FORK_FAILED     "Fork failed with %d - %s"
#define ERR_EXEC_VLC        "Could not run: %s %s"
#define ERR_ERRNO           "%d - %s"
#define ERR_SIGNAL          "Signal %d"

#define DBG_RUN_CMD         "Will run: %s %s"
#define DBG_FORK_PID        "pid is %d for url '%s'"
#define DBG_KILL_PID        "killing pid %d for url %s"

//
// MARK: Class representing one COM channel, which can run a VLC stream
//

class COMChannel {
protected:
    int idx = -1;               // COM idx, starting from 0
    // X-Plane data
    int frequ = 0;              // currently tuned frequncy [Hz as returned by XP]
    std::string frequString;    // frequncy as string in format ###.###
    // LiveATC data
    std::string playUrl;        // URL to play according to LiveATC
    // VLC data
    // the actual process running VLC, needed to stop it:
#if IBM
    HANDLE vlcProc = NULL;
#else
    int vlcPid = 0;
#endif
    std::future<void> futVlcStart;
    
public:
    COMChannel(int i) : idx(i) {}
    int GetIdx() const { return idx; }
    // stop VLC, reset frequency
    void ClearChannel();

    // X-Plane data
    int GetFrequ() const                   { return frequ; }
    std::string GetFrequString() const     { return frequString; }
    
    // if _new differs from frequ
    // THEN we consider it a change to a new frequency
    bool doChange(int _new);

    // LiveATC data
    std::string GetPlayURL() const { return playUrl; }

    // VLC control
    // asynchronous/non-blocking cllas, which encapsulate blocking calls in threads
    void StartStreamAsync (std::string&& url);
    void StopStreamAsync ();
    
    // blocking calls, called by above asynch threads
    void StartStream (std::string&& url);
    void StopStream ();
    
    // compute complete parameter string for VLC
    std::string GetVlcParams();
};

//
// MARK: Global Functions
//

extern COMChannel gChn[COM_CNT];

// stop still running VLC instances
void RVStopAll();


#endif /* SLARunVLC_h */

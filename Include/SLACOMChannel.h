//
//  SLACOMChannel.h
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

#ifndef SLACOMChannel_h
#define SLACOMChannel_h

#define LIVE_ATC_DOMAIN     "www.liveatc.net"
#define LIVE_ATC_BASE       "https://" LIVE_ATC_DOMAIN
#define LIVE_ATC_URL        LIVE_ATC_BASE "/search/f.php?freq=%s"

#define MSG_COM_IS_NOW      "COM%d is now %s, tuning to '%s'"
#define MSG_COM_IS_NOW_IN   "COM%d is now %s, tuning to '%s' with %ds delay"
#define MSG_COM_COUNTDOWN   "COM%d: %ds till '%s' starts"
#define MSG_AP_OUT_OF_REACH "'%s' now out of reach"
#define DBG_QUERY_URL       "Sending query %s"
#define WARN_RE_ICAO        "Could not find %s in LiveATC reply"
#define DBG_STREAM_NOT_UP   "Stream %s skipped as it is not UP but '%s'"
#define DBG_ADDING_STREAM   "Adding    stream %s"
#define DBG_REPL_STREAM     "Replacing stream %s"
#define DBG_AP_NOT_FOUND    "Could not find airport %s in X-Plane's nav database"
#define DBG_AP_CLOSEST      "Closest airport is %s (%.1fnm)"
#define DBG_AP_NO_CLOSEST   "No airport found within %.1fnm"
#define DBG_RUN_CMD         "Will run: %s %s"
#define DBG_FORK_PID        "pid is %d for url '%s'"
#define DBG_KILL_PID        "killing pid %d for url %s"

// 100 KB of storage initially
constexpr std::string::size_type READ_BUF_INIT_SIZE = 100 * 1024;
constexpr long ADD_COUNTDOWN_DELAY_MS = 500;
#define ERR_CURL_INIT           "Could not initialize CURL: %s"
#define ERR_CURL_EASY_INIT      "Could not initialize easy CURL"
#define ERR_CURL_NO_LIVEATC     "Could not query LiveATC.net for version info: %d - %s"
#define ERR_CURL_HTTP_RESP      "%s: HTTP response is not OK but %ld"
#define ERR_CURL_REVOKE_MSG     "revocation"                // appears in error text if querying revocation list fails
#define ERR_CURL_DISABLE_REV_QU "%s: Querying revocation list failed - have set CURLSSLOPT_NO_REVOKE and am trying again"

#define ERR_SHELL_FAILED    "ShellExecute failed with %lu - %s"
#define ERR_FORK_FAILED     "Fork failed with %d - %s"
#define ERR_EXEC_VLC        "Could not run: %s %s"
#define ERR_ERRNO           "%d - %s"
#define ERR_SIGNAL          "Signal %d"

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
    struct LiveATCDataTy {
        std::string airportIcao;    // airport of that URL, like "KSFO"
        std::string streamName;     // like "Tower"
        std::string playUrl;        // URL to play according to LiveATC
        int nFacilities = 0;        // number of facilities table rows
        std::string dbgOut () const { return airportIcao+'|'+streamName+'|'+std::to_string(nFacilities)+'|'+playUrl; }
    };
    LiveATCDataTy curr, prev;   // current in use / previous
    positionTy currPos;         // position of current in-use airport
    std::atomic_flag flagStartingStream = ATOMIC_FLAG_INIT;
    
    // VLC data
protected:
    // the actual process running VLC, needed to stop it:
#if IBM
    HANDLE vlcPid  = NULL;
    HANDLE vlcPrev = NULL;      // previous VLC process during desync period
#else
    int vlcPid  = 0;
    int vlcPrev = 0;            // previous VLC process during desync period
#endif
    std::future<void> futVlcStart;
    std::chrono::time_point<std::chrono::steady_clock> desyncDone;
    
public:
    COMChannel(int i) : idx(i) {}
    inline int GetIdx() const { return idx; }
    // stop VLC, reset frequency
    void ClearChannel();

    // X-Plane data
    inline int GetFrequ() const                   { return frequ; }
    inline std::string GetFrequString() const     { return frequString; }
    
    // if _new differs from frequ
    // THEN we consider it a change to a new frequency
    bool doChange(int _new);

    // LiveATC data
    const LiveATCDataTy& GetLiveATCData() const { return curr; }

    // VLC control
    // asynchronous/non-blocking cllas, which encapsulate blocking calls in threads
    void StartStreamAsync ();
    void StopStreamAsync (bool bPrev);
    // called every second: start new streams, stop out of reach ones...
    void RegularMaintenance ();

    // blocking calls, called by above asynch threads
    void StartStream ();
    void StopStream (bool bPrev);
    // stop still running VLC instances
    static void StopAll();

    // VLC status
    inline bool IsPlaying (bool bPrev) const { return bPrev ? vlcPrev > 0 : vlcPid > 0; }
    inline bool IsPlaying () const { return vlcPid > 0 || vlcPrev > 0; }
    int GetSecTillDesyncDone () const;
    inline bool IsDesyncing() const { return vlcPid > 0 && GetSecTillDesyncDone() > 0; }
    inline bool IsDesyncDone() const { return GetSecTillDesyncDone() <= 0; }

    // compute complete parameter string for VLC
    std::string GetVlcParams();
    
    // *** Determination of stream URL to play ***
protected:
    // turn current stream to previous
    void TurnCurrToPrev ();
    // query LiveATC, parse result...come up with stream url to play
    bool FetchUrlForFrequ ();
    
    // maps airport ICAO to (best) stream url for current frequency
    typedef std::map<std::string,LiveATCDataTy> AirportStreamMapTy;
    AirportStreamMapTy mapAirportStream;
    void ParseForAirportStreams ();
    
    // find closest airport in mapAirportStream
    AirportStreamMapTy::iterator FindClosestAirport();
    
    // communication with LiveATC.net
    std::string readBuf;
    static bool bDisableRevocationList;
    bool QueryLiveATC ();
    static size_t CB_StoreAll (char *ptr, size_t, size_t nmemb, void* userdata);
};

//
// MARK: Global array
//

extern COMChannel gChn[COM_CNT];

#endif /* SLACOMChannel_h */

//
//  PLACOMChannel.h
//  PlayLiveATC
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

#ifndef PLACOMChannel_h
#define PLACOMChannel_h

#define LIVE_ATC_DOMAIN     "www.liveatc.net"
#define LIVE_ATC_BASE       "https://" LIVE_ATC_DOMAIN
#define LIVE_ATC_URL        LIVE_ATC_BASE "/search/f.php?freq=%s"
#define LIVE_ATC_PLS        ".pls"

#define ENV_VLC_PLUGIN_PATH "VLC_PLUGIN_PATH"

#define MSG_COM_IS_NOW      "COM%d is now %s, tuning to '%s'"
#define MSG_COM_IS_NOW_IN   "COM%d is now %s, tuning to '%s' with %lds delay"
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
#define DBG_STREAM_STOP     "Stopping playback of '%s' (%s)"

/// 100 KB of network response storage initially
constexpr std::string::size_type READ_BUF_INIT_SIZE = 100 * 1024;
constexpr long ADD_COUNTDOWN_DELAY_S = 1;   ///< [s] countdown delay (for query, buffering...)

#define ERR_VLC_INIT        "Could not init VLC: %s"
#define ERR_GET_LIVE_ATC    "Could not "
#define ERR_VLC_PLAY        "Could not play '%s': %s"

/// status of a stream
enum StreamStatusTy {
    STREAM_NOT_INIT = 0,            ///< startup status: not initialized
    STREAM_NO_FREQU,                ///< no frequency set, not active
    STREAM_NOT_PLAYING,             ///< frequency defined, but no stream playing
    STREAM_SEARCHING,               ///< frequency defined, searching for matching stream
    STREAM_BUFFERING,               ///< VLC started with stream, not yet emmitting sound
    STREAM_PLAYING,                 ///< VLC playing a LiveATC stream
};

/// @brief Stream's status as text
/// @param s The status, which is to be converted to text.
std::string GetStatusStr (StreamStatusTy s);

/// Data returned by LiveATC
struct LiveATCDataTy {
    // LiveATC data
    std::string airportIcao;    ///< Airport of that URL, like "KSFO"
    positionTy  airportPos;     ///< Position of the airport for distance calculations
    std::string streamName;     ///< Stream name, like "Tower"
    std::string playUrl;        ///< URL to play according to LiveATC
    int nFacilities = 0;        ///< Number of facilities table rows (the lower the better!)
    
    /// Copy from another object
    void CopyFrom (const LiveATCDataTy& o) { *this = o; }

    /// Textual summary (Icao if needed + stream name)
    inline std::string Summary () const
    { return startsWith(streamName, airportIcao) ? streamName : airportIcao+'|'+streamName; }
    /// Textual debug output, e.g. for log file
    inline std::string dbgStatus () const
    { return Summary()+'|'+std::to_string(nFacilities)+'|'+playUrl; }
};

/// Map of data returned by LiveATC, key is airport ICAO
typedef std::map<std::string,LiveATCDataTy> LiveATCDataMapTy;


/// Adds frequency and VLC data
struct StreamCtrlTy : public LiveATCDataTy {
    // XP data
    int frequ = 0;              ///< currently tuned frequency in Hz as returned by XP
    std::string frequString;    ///< frequncy in kHz as string in format ###.###

    // VLC control
    std::unique_ptr<VLC::Instance>      pInst;  ///< VLC instance
    std::unique_ptr<VLC::MediaPlayer>   pMP;    ///< VLC media player
    std::unique_ptr<VLC::Media>         pMedia; ///< VLC media to be played, changes with new LiveATC streams
    
    /// Is VLC properly initialized?
    inline bool IsValid() const { return pInst && pMP; }
    /// stream's status
    StreamStatusTy GetStatus() const;
    /// Is COM channel defined, i.e. at least a frequency set?
    bool IsDefined () const { return GetStatus() >= STREAM_NOT_PLAYING; }

    /// Stops playback and clears all data
    void StopAndClear ();
    
    /// Textual summary (stream and status)
    inline std::string Summary () const
    { return LiveATCDataTy::Summary() + " (" + GetStatusStr(GetStatus()) + ")"; }
    /// Textual debug output, e.g. for log file
    inline std::string dbgStatus () const
    { return LiveATCDataTy::dbgStatus() + '|' + GetStatusStr(GetStatus()); }
};

/// Represents one COM channel, its frequency and playback streams.
class COMChannel
{
protected:
    int idx = -1;               ///< COM idx, starting from 0
    
    /// Two sets of data, one in use, one being phased out
    StreamCtrlTy dataA, dataB;
    /// Pointers indicating which of the two sets is active, which one is being phased out
    StreamCtrlTy *curr = &dataA, *prev = &dataB;
    /// Indicates if a stream is starting up right now
    std::atomic_flag flagStartingStream = ATOMIC_FLAG_INIT;
    
protected:
    /// Network read buffer for LiveATC response
    std::string readBuf;
    /// Stored future when using std::async, typically not used later on
    std::future<void> futVlcStart;
    /// make StartStream() stop early
    volatile bool bAbortStart = false;
    /// Time point when audio desync should be finished (fair guess)
    std::chrono::time_point<std::chrono::steady_clock> desyncDone;
    
public:

    /// @brief Constructor does not init VLC
    /// @param i 0-based COM index, usually 0 or 1
    COMChannel(int i);
    
    /// Destructor calls CleanupVLC()
    ~COMChannel();
    
    /// Initialize the VLC smart pointers, can fail if wrong directory configured
    bool InitVLC();
    
    /// Cleans up the VLC smart pointers in a proper order
    void CleanupVLC();
    
    /// Is VLC properly initialized?
    bool IsValid() const;
    
    /// COM index, typically just 0 or 1
    inline int GetIdx() const { return idx; };

    // X-Plane data
    
    /// Current Frequency in Hz as returned by XP
    inline int GetFrequ() const                     { return curr->frequ; }
    /// Current Frequency in kHz as formatted string with 3 digits */
    inline std::string GetFrequString() const       { return curr->frequString; }
    /// Current LiveATC data
    const StreamCtrlTy& GetStreamCtrlData() const     { return *curr; }
    

    /// Called every second: start new streams, stop out of reach ones...
    void RegularMaintenance ();

    /// Stop all playback, reset frequency and other data.
    void ClearChannel();
    
    // VLC status
    
    /// COM channel's status, mostly depends on `curr->GetStatus()` but takes StartStream() into consideration
    StreamStatusTy GetStatus() const;
    /// Is COM channel defined / has frequency?
    inline bool IsDefined() const { return GetStatus() >= STREAM_NOT_PLAYING; }

    /// Seconds till audio desync is done
    int GetSecTillDesyncDone () const;
    /// Is audio desync still under way?
    inline bool IsDesyncing() const  { return GetSecTillDesyncDone() > 0; }
    /// Is audio desync done?
    inline bool IsDesyncDone() const { return GetSecTillDesyncDone() <= 0; }
    
    /// @brief Textual status summary for end user
    /// @param bFull Report both `curr` and `prev`? Default: just `curr`
    std::string Summary (bool bFull = false) const;
    /// @brief Textual status summary for debug purposes
    /// @param bFull Report both `curr` and `prev`? Default: just `curr`
    std::string dbgStatus (bool bFull = false) const;

public:
    // Static functions to act on *all* COM channel objects
    
    /// Initialize *all* VLC instances
    static bool InitAllVLC();
    
    /// Stop *all* still running VLC playbacks of *all* COM channels
    static void StopAll();
    
    /// Cleanup *all* VLC instances, also stops all playback
    static void CleanupAllVLC();

    /// Set the volume of all playback streams
    static void SetAllVolume(int vol);

    /// (un)Mute all playback streams
    static void MuteAll(bool bDoMute = true);
    
protected:
    /// @brief Checks for and performs change in frequency
    /// @param _new New frequency in Hz as returned by XP.
    bool doChange(int _new);

    // VLC control
    
    /// Start the `curr` stream asynchronously by calling StartStream() via `std::async`
    void StartStreamAsync ();
    /// Blocking call to start the `curr` stream
    void StartStream ();
    /// @brief Stop `curr` or `prev` stream immediately
    /// @param bPrev Stop `prev`? (Otherwise stop `curr`)
    void StopStream (bool bPrev);
    
    /// Checks if an async StartStream() operation is in progress
    bool IsAsyncRunning () const;
    /// Aborts StartStream()) and waits for it to complete
    void AbortAndWaitForAsync ();

    // *** Determination of stream URL to play ***
    /// Turn `curr` stream into `prev`
    void TurnCurrToPrev ();
    /// Query LiveATC, parse result, update `curr` with found stream if any
    bool FetchUrlForFrequ ();
    
    /// maps airport ICAO to (best) stream url for current frequency
    LiveATCDataMapTy mapAirportStream;
    /// Parses `readBuf` for airports and relevant streams
    void ParseForAirportStreams ();
    /// @brief Find closest airport in `mapAirportStream`
    /// @return Iterator pointing to airportStream data with updated airportPos
    LiveATCDataMapTy::iterator FindClosestAirport();
};

//
// MARK: Global array
//

extern COMChannel gChn[COM_CNT];

#endif /* PLACOMChannel_h */

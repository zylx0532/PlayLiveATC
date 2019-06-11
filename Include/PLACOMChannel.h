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

#define MSG_COM_IS_ATIS     "COM%d is now %s, referring to %s, suppressed in favour of XP's ATIS"
#define MSG_COM_IS_NOW      "COM%d is now %s, tuning to '%s'"
#define MSG_COM_IS_NOW_IN   "COM%d is now %s, tuning to '%s' with %lds delay"
#define MSG_STBY_IS_NOW_IN  "COM%d stand-by is now %s, pre-buffering '%s' with %lds delay"
#define MSG_COM_COUNTDOWN   "COM%d: %ds till '%s' starts"
#define MSG_AP_CHANGE       "COM%d: Tuning to '%s' as this is closest now"
#define MSG_AP_OUT_OF_REACH "COM%d: '%s' now out of reach"
#define MSG_AP_STDBY_CHANGE "COM%d stand-by: Tuning to '%s' as this is closest now"
#define MSG_AP_STDBY_OUT_OF_REACH "COM%d stand-by: '%s' now out of reach"
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
    STREAM_DESYNCING,               ///< VLC is buffering the audio desync period
    STREAM_MUTED,                   ///< VLC would be playing but is muted
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
    
    /// Is this an ATIS channel / has "ATIS" in its name?
    inline bool IsATIS () const
    { return streamName.find("ATIS") != std::string::npos; }

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
    
protected:
    // XP data
    int frequ = 0;              ///< currently tuned frequency in Hz as returned by XP
    std::string frequString;    ///< frequncy in kHz as string in format ###.###
    bool bStandbyPrebuf = false;///< pre-buffering the stand-by frequency?
    /// maps of all _potential_ airport streams for current `frequ`
    LiveATCDataMapTy mapAirportStream;
    /// Network read buffer for LiveATC response
    std::string readBuf;
    /// Time point when audio desync should be finished (fair guess)
    std::chrono::time_point<std::chrono::steady_clock> desyncDone;
    /// last volume set, value to be restored when unmuting
    int volume = 100;
    /// Muted? Which is simulated by setting volume = 0
    bool bMute = false;

public:
    // VLC control
    std::unique_ptr<VLC::MediaPlayer>   pMP;    ///< VLC media player
    std::unique_ptr<VLC::Media>         pMedia; ///< VLC media to be played, changes with new LiveATC streams
    
public:
    /// @brief Set frequency including frequency string
    /// @param f Frequenc in Hz as returned by XP
    void SetFrequ (int f);
    /// Get frequency in Hz
    inline int GetFrequ () const { return frequ; }
    inline std::string GetFrequStr () const { return frequString; }
    
    /// Is VLC properly initialized?
    inline bool IsValid() const { return bool(pMP); }
    /// stream's status
    StreamStatusTy GetStatus() const;
    /// Is COM channel defined, i.e. at least a frequency set?
    inline bool IsDefined () const { return GetStatus() >= STREAM_NOT_PLAYING; }
    /// Pre-buffering the stand-by frequency?
    inline bool IsStandbyPrebuf() const { return bStandbyPrebuf && IsDefined(); }
    /// Set as pre-buffering
    inline void SetStandbyPrebuf(bool b) { bStandbyPrebuf=b; }

    /// Query LiveATC, parse result, update `curr` with found stream if any
    bool FetchUrlForFrequ ();
    /// Parses `readBuf` for airports and relevant streams
    void ParseForAirportStreams ();
    /// @brief Find closest airport in `mapAirportStream`
    /// @return Iterator pointing to airportStream data with updated airportPos
    LiveATCDataMapTy::iterator FindClosestAirport();
    /// The end iterator is needed to work with the above result
    inline LiveATCDataMapTy::iterator AirportStreamsEnd() { return mapAirportStream.end(); }

    /// @brief Set audio desync, also sets the time when done
    /// @param sec Seconds to delay the audio playback for desync
    void SetAudioDesync (long sec);
    /// Seconds till audio desync is done
    int GetSecTillDesyncDone () const;
    /// Is audio desync still under way?
    inline bool IsDesyncing() const  { return GetSecTillDesyncDone() > 0; }
    /// Is audio desync done?
    inline bool IsDesyncDone() const { return GetSecTillDesyncDone() <= 0; }
    /// Clears the desync timer (and only the timer, does not change VLC's desync setting)
    inline void ClearDesyncTimer () { desyncDone = std::chrono::time_point<std::chrono::steady_clock>(); }

    /// Return volume as last set with SetVolume
    inline int GetVolume() const { return volume; }
    /// @brief Set the volume incl. unmute, and save the value in case of mute
    /// @param v New Volume
    void SetVolume(int v);
    /// Return if currently muted
    inline bool IsMuted() const { return bMute;  }
    /// @brief Set (un)mute
    /// @param mute Mute? or unmute?
    void SetMute(bool mute);

    /// Stops playback and clears all data
    void StopAndClear ();
    
    /// Textual summary (stream and status)
    inline std::string Summary (StreamStatusTy eStatus = STREAM_NOT_INIT) const
    { return LiveATCDataTy::Summary() + " (" + GetStatusStr(eStatus ? eStatus : GetStatus()) + ")"; }
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
    /// Stored future when using std::async, typically not used later on
    std::future<void> futVlcStart;
    /// make StartStream() stop early
    volatile bool bAbortStart = false;
    
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

    /// @brief Textual status summary for end user
    /// @param bPrev Report `prev` instead of `curr`?
    std::string Summary (bool bPrev = false) const;
    /// @brief Textual status summary for debug purposes
    /// @param bPrev Report `prev` instead of `curr`?
    std::string dbgStatus (bool bPrev = false) const;

public:
    // Static functions to act on *all* COM channel objects
    
    /// Initialize *all* VLC instances
    static bool InitAllVLC();
    
    /// Stop *all* still running VLC playbacks of *all* COM channels
    static void StopAll();
    
    /// Cleanup *all* VLC instances, also stops all playback
    static void CleanupAllVLC();

    /// Update list of available audio devices
    static void UpdateVLCOutputDevices();

    /// @brief Set all MediaPlayer to use the given audio device
    /// @param dev The VLC audio device to use
    static void SetAllAudioDevice(const std::string& devId);

    /// Set the volume of all playback streams
    static void SetAllVolume(int vol);

    /// (un)Mute all playback streams
    static void MuteAll(bool bDoMute = true);
    
    /// checks if there is any active stream playing ATIS
    static bool AnyATISPlaying();
    
protected:
    /// @brief Initial stand-by frequency when frequencies were swapped
    /// Stored to detect that the stand-by frequency has been changed away
    /// from an initial value as only then pre-buffering applies.
    int initFrequStandBy = 0;
    
    /// @brief Last seen dialed-in stand-by frequency.
    /// Stored to detect a stable _change_ to a new stand-by frequency
    int lastFrequStandby = 0;
    
    /// @brief Checks for and performs change in frequency
    /// @param _new New frequency in Hz as returned by XP.
    bool doChange(int _new);
    
    /// @brief Checks for and starts pre-buffering of the standby frequency.
    /// @param _new Current standby frequency in Hz as returned by XP.
    bool doStandbyPrebuf(int _new);

    // VLC control
    
    /// @brief Start a stream asynchronously by calling StartStream() via `std::async`
    /// @param bStandby Start the stand-by frequncy stream for pre-buffering? Otherwise start `curr`
    void StartStreamAsync (bool bStandby);
    /// @brief Blocking call to start a stream
    /// @param bStandby Start the stand-by frequncy stream for pre-buffering? Otherwise start `curr`
    void StartStream (bool bStandby);
    /// @brief Stop `curr` or `prev` stream immediately
    /// @param bPrev Stop `prev`? (Otherwise stop `curr`)
    void StopStream (bool bPrev);
    
    /// determines and sets proper volum/mute status
    void SetVolumeMute ();
    
    /// Checks if an async StartStream() operation is in progress
    bool IsAsyncRunning () const;
    /// Aborts StartStream()) and waits for it to complete
    void AbortAndWaitForAsync ();

    // *** Determination of stream URL to play ***
    /// Turn `curr` stream into `prev`
    void TurnCurrToPrev ();
};

//
// MARK: Global array
//

extern COMChannel gChn[COM_CNT];

extern std::vector<VLC::AudioOutputDeviceDescription> gVLCOutputDevs;

#endif /* PLACOMChannel_h */

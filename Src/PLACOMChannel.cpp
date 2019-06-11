//
//  PLACOMChannel.cpp
//
// Fetch url to play from LiveTAC
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

#include "PlayLiveATC.h"

//
// MARK: Globals
//

// one COM channel per COM channel - obviously ;)
COMChannel gChn[COM_CNT] = {
    {0}, {1}
};

/// Static arguments passed to the VLC initialization
const char* vlcArgs[] = {
#ifdef _DEBUG
    "-vvv",
#endif
    "--no-lua"              // this reduces the number of plugins required, but also necessitates our parsing of the .pls URL
};

/// Number of `vlcArgs`
const int vlcArgC = sizeof(vlcArgs)/sizeof(vlcArgs[0]);

/// The one and only VLC instance
std::unique_ptr<VLC::Instance> gVLCInst;

/// List of available output devices
std::vector<VLC::AudioOutputDeviceDescription> gVLCOutputDevs;

//
// MARK: Global VLC functions
//

/// Creates the global VLC instance object, if it doesn't exist yet
/// @warning Fails if VLC plugins cannot be found.
bool CreateVLCInstance ()
{
    if (!gVLCInst) {
#if !(IBM)
        // Tell VLC where to find the plugins
        // (In Windows, this has no effect.)
        if (!dataRefs.GetVLCPath().empty())
            setenv(ENV_VLC_PLUGIN_PATH, dataRefs.GetVLCPath().c_str(), 1);
#endif

        // create VLC instance
        try {
            gVLCInst = std::make_unique<VLC::Instance>(vlcArgC, vlcArgs);
            gVLCInst->setAppId(PLUGIN_SIGNATURE, PLA_VERSION_FULL, "");
            gVLCInst->setUserAgent(HTTP_USER_AGENT, HTTP_USER_AGENT);
        }
        catch (std::runtime_error e)
        {
            LOG_MSG(logERR, ERR_VLC_INIT, e.what());
            return false;
        }
        catch (...)
        {
            LOG_MSG(logERR, ERR_VLC_INIT, "?");
            return false;
        }
    }
    return true;
}

/// Removes the global VLC instance object
void CleanupVLCInstance ()
{
    gVLCInst = nullptr;
}

/// Static function to convert a status enum to a status text
/// @param s The status to be converted.
/// @return A text representing the passed in status `s`
std::string GetStatusStr (StreamStatusTy s)
{
    switch (s) {
        case STREAM_NOT_INIT:   return "not initialized";
        case STREAM_NO_FREQU:   return "no frequency";
        case STREAM_NOT_PLAYING:return "not playing";
        case STREAM_SEARCHING:  return "searching";
        case STREAM_BUFFERING:  return "buffering";
        case STREAM_DESYNCING:  return "desyncing";
        case STREAM_MUTED:      return "muted";
        case STREAM_PLAYING:    return "playing";
        default: return "?";        // must not happen
    }
}

// save the new frequency
void StreamCtrlTy::SetFrequ(int f)
{
    char buf[20];
    frequ = f;
    snprintf(buf, sizeof(buf),
             "%d.%03d",
             f / 1000, f % 1000);
    frequString = buf;
}

//
// MARK: Helper structs
//

/// Stream's status, decides all but STREAM_SEARCHING (see COMChannel::GetStatus())
StreamStatusTy StreamCtrlTy::GetStatus() const
{
    // not initialized at all?
    if (!IsValid())
        return STREAM_NOT_INIT;
    
    // Is desync period still running?
    if (IsDesyncing())
        return STREAM_DESYNCING;

    // playing a stream, i.e. emmitting sound?
    if (pMP->isPlaying()) {
        return bMute ? STREAM_MUTED : STREAM_PLAYING;
    }
    
    // media defined? Then let's assume VLC is instructed to play and will soon
    if (pMedia)
        return STREAM_BUFFERING;
    
    // no frequency selected?
    if (!frequ || frequString.empty())
        return STREAM_NO_FREQU;

    // so there's a frequency but no media? Then we are not playing it.
    return STREAM_NOT_PLAYING;
}


//
// MARK: Determine new stream URL to play
//

/// 1. Queries LiveATC.net with defined frequency using HttpGet()
/// 2. Parses the resulting web page for airport-specific streams (ParseForAirportStreams())
/// 3. Finds the airport closest to current location (FindClosestAirport())
/// 4. If an airport-stream is in reach copies its data into `curr`
bool StreamCtrlTy::FetchUrlForFrequ ()
{
    // ask LiveATC, fills readBuf
    // put together the url
    char url[100];
    snprintf(url, sizeof(url), LIVE_ATC_URL, frequString.c_str());
    if (!HttpGet(url, readBuf)) {
        // HTTP went wrong, clear this channel for now so we don't try again without the user doing someting
        StopAndClear();
        return false;
    }
    
    // parse this response -> fill mapAirportStream
    ParseForAirportStreams();
    if (mapAirportStream.empty())
        return false;
    
    // find closest airport in mapAirportStream
    const LiveATCDataMapTy::iterator apIter = FindClosestAirport();
    if (apIter == mapAirportStream.end())
        return false;
    
    // found an airport, use that data from now on
    CopyFrom(apIter->second);
    return true;
}

// parse readBuffer and fill mapAirportStream
void StreamCtrlTy::ParseForAirportStreams ()
{
    // start with a clean map
    mapAirportStream.clear();
    
    // pass over the readBuffer, airport section by airport section
    for (std::string::size_type
         pos = readBuf.find("<tr><td><strong>ICAO:"),
         nextPos = std::string::npos;
         pos != std::string::npos;
         pos = nextPos)
    {
        // find the next airport thereafter, so we know the section we are working on
        nextPos = readBuf.find("<tr><td><strong>ICAO:", pos+1);
        
        // the section we really work on now
        const std::string apSec(readBuf.substr(pos, nextPos-pos));
        std::smatch m;
        
        // identify information in the LiveATC reply
        LiveATCDataTy streamData;
        static std::regex reIcao ( R"#(<tr><td><strong>ICAO: </strong>(\w\w\w\w)<strong>)#" );
        static std::regex reName ( R"#(<td bgcolor="lightblue"><strong>(.+?)</strong>)#" );
        static std::regex reStat ( R"#(<tr><td><strong>Feed Status:</strong> <font color=\\?"\w+\\?"><strong>(\w+)</strong>)#" );
        static std::regex reUrl  ( R"#(<a href="(.+?)" onClick=)#" );
        
        if (!std::regex_search(apSec, m, reIcao))       { LOG_MSG(logWARN, WARN_RE_ICAO, "airport ICAO"); continue; }
        streamData.airportIcao = { m[1].str() };
        
        if (!std::regex_search(apSec, m, reName))       { LOG_MSG(logWARN, WARN_RE_ICAO, "stream name"); continue; }
        streamData.streamName = m[1].str();
        
        // is stream not UP?
        if (!std::regex_search(apSec, m, reStat))       { LOG_MSG(logWARN, WARN_RE_ICAO, "stream status"); /* assume UP */ }
        else if (m[1] != "UP")
        { LOG_MSG(logDEBUG, DBG_STREAM_NOT_UP, streamData.streamName.c_str(), m[1].str().c_str()); continue; }
        
        // URL to play, most likely just relative to the current server but not an absolute URL
        if (!std::regex_search(apSec, m, reUrl))        { LOG_MSG(logWARN, WARN_RE_ICAO, "stream URL"); continue; }
        streamData.playUrl = m[1].str();
        if (streamData.playUrl.substr(0,4) != "http")
            streamData.playUrl = std::string(LIVE_ATC_BASE) + streamData.playUrl;
        
        // count tables rows in facilities table
        pos = apSec.find("<table class=\"freqTable\"");
        if (pos != std::string::npos) {
            for (pos = apSec.find("<tr><td class=\"td", pos+1);
                 pos != std::string::npos;
                 pos = apSec.find("<tr><td class=\"td", pos+1),
                 streamData.nFacilities++);
        }
        
        // is such an airport already in our map?
        LiveATCDataMapTy::iterator mapIter = mapAirportStream.find(streamData.airportIcao);
        if (mapIter != mapAirportStream.end())
        {
            // only replace if current find is better.
            // Stream is considered better if there are less lines in the
            // 'facilities' table, i.e. the stream is more specific to the
            // searched frequency
            if (streamData.nFacilities < mapIter->second.nFacilities)
            {
                LOG_MSG(logDEBUG, DBG_REPL_STREAM, streamData.dbgStatus().c_str());
                mapIter->second = std::move(streamData);
            }
        }
        else
        {
            LOG_MSG(logDEBUG, DBG_ADDING_STREAM, streamData.dbgStatus().c_str());
            mapAirportStream.emplace(streamData.airportIcao, std::move(streamData));
        }
    }
}


// find closest airport in mapAirportStream
LiveATCDataMapTy::iterator StreamCtrlTy::FindClosestAirport()
{
    // Sanity check...there must be any for us to find one
    if (mapAirportStream.empty())
        return mapAirportStream.end();
    
    // plane's position
    const positionTy planePos = dataRefs.GetUsersPlanePos();
    
    // loop airports in mapAirportStream and for each of it
    // determine distance to plane, remember the closest airport
    LiveATCDataMapTy::iterator closestAirport = mapAirportStream.end();
    double closestDist_nm = dataRefs.GetMaxRadioDist();    // with this init we will not consider airports father away
    for (LiveATCDataMapTy::iterator iter = mapAirportStream.begin();
         iter != mapAirportStream.end();
         )
    {
        // if we don't know the airport's position yet
        LiveATCDataTy& atcData = iter->second;
        if (std::isnan(atcData.airportPos.lat())) {
            // find that airport in X-Plane's nav database
            float lat = NAN, lon = NAN, alt_m = NAN;
            XPLMNavRef apRef = XPLMFindNavAid(NULL, iter->first.c_str(), NULL, NULL, NULL, xplm_Nav_Airport);
            if (apRef) {
                XPLMGetNavAidInfo(apRef, NULL, &lat, &lon, &alt_m, NULL, NULL, NULL, NULL, NULL);
                atcData.airportPos = positionTy (lat, lon, alt_m);
            }
        }
        
        if (!std::isnan(atcData.airportPos.lat())) {
            // check current distance to airport and if it is the closest seen so far
            double dist_nm = planePos.dist(atcData.airportPos) / M_per_NM;
            if (dist_nm < closestDist_nm) {
                closestDist_nm = dist_nm;
                closestAirport = iter;
            }
            // then try next airport
            iter++;
        } else {
            LOG_MSG(logDEBUG, DBG_AP_NOT_FOUND, iter->first.c_str());
            // remove this one from the list
            iter = mapAirportStream.erase(iter);
        }
    }
    
    // return the closest Airport found, if any
    return closestAirport;
}

void StreamCtrlTy::SetAudioDesync (long desyncSecs)
{
    // set audio desync (microseconds!)
    pMP->setAudioDelay(int64_t(desyncSecs) * 1000000L);
    // set the desync timer
    desyncDone = std::chrono::steady_clock::now() +
    std::chrono::seconds(desyncSecs);
}

// return number of seconds till desync should be done
int StreamCtrlTy::GetSecTillDesyncDone () const
{
    return int(std::chrono::duration_cast<std::chrono::seconds>
               (desyncDone - std::chrono::steady_clock::now()).count());
}

void StreamCtrlTy::SetVolume(int v)
{
    // normalize
    if (v < 0) v = 0;
    if (v > 100) v = 100;

    // unmute
    bMute = false;

    // set volume
    volume = v;
    if (pMP)
        pMP->setVolume(volume);
}

// Mute and UnMute
void StreamCtrlTy::SetMute(bool mute)
{
    bMute = mute;
    if (pMP)
        pMP->setVolume(bMute ? 0 : volume);
}

void StreamCtrlTy::StopAndClear()
{
    // stop any stream in case something's running
    pMP->stop();

    // LiveATCDataTy
    airportIcao.clear();
    airportPos = positionTy();
    streamName.clear();
    playUrl.clear();
    nFacilities = 0;
    
    // StreamCtrlTy
    frequ = 0;
    frequString.clear();
    bStandbyPrebuf = false;
    mapAirportStream.clear();
    
    // clear media
    pMedia = nullptr;
}

//
// MARK: Class representing one COM channel, which can run a VLC stream
//

COMChannel::COMChannel(int i) :
idx(i)
{}

COMChannel::~COMChannel()
{
    CleanupVLC();
}

// Initialize the VLC smart pointers, can fail if wrong directory configured
bool COMChannel::InitVLC()
{
    // must have an instance
    LOG_ASSERT(gVLCInst);
    
    // just in case - cleanup in proper order
    CleanupVLC();

    // (re)create media player
    dataA.pMP = std::make_unique<VLC::MediaPlayer>(*gVLCInst);
    dataB.pMP = std::make_unique<VLC::MediaPlayer>(*gVLCInst);
    return true;
}

// Cleans up the VLC smart pointers in a proper order
void COMChannel::CleanupVLC()
{
    // async thread running? That's something we need to wait for (blocking!)
    AbortAndWaitForAsync();
    futVlcStart = std::future<void>();
    
    // stop orderly
    if (dataA.pMP && dataA.pMP->isPlaying())
        dataA.pMP->stop();
    if (dataB.pMP && dataB.pMP->isPlaying())
        dataB.pMP->stop();
    
    // these are smart pointers, i.e. assigning nullptr destroys an assigned object properly
    dataB.pMedia    = nullptr;
    dataB.pMP       = nullptr;
    dataA.pMedia    = nullptr;
    dataA.pMP       = nullptr;
}

// Is VLC properly initialized?
bool COMChannel::IsValid() const
{
    return dataA.IsValid() && dataB.IsValid();
}


/// Should be called every second, e.g. from a flight loop callback
/// 1. Checks for frequency change, and if not changed:
/// 2. Checks distance and then might stop the channel, or switch over to another radio
void COMChannel::RegularMaintenance ()
{
    // not initialized?
    if (!IsValid())
        return;
    
    // need to stop previous frequency after curr'ent desync is done?
    if (prev->IsDefined() && !prev->IsStandbyPrebuf() && curr->IsDesyncDone()) {
        StopStream(true);
    }
    
    // *** COM frequency change ***
    // get current frequency and check if this is considered a change
    if (doChange(dataRefs.GetComFreq(idx))) {
        // it is: we start a new stream
        StartStreamAsync(false);
        return;
    }
    
    // check for mute status
    SetVolumeMute();
    
    // *** only every 10th call do the expensive stuff ***
    static int callCnt = 0;
    if (++callCnt <= 10)
        return;
    callCnt = 0;
    
    // *** Pre-buffering ***
    doStandbyPrebuf(dataRefs.GetComStandbyFreq(idx));
    
    // *** Checks on the active stream ***
    if (curr->IsDefined()) {
        // Find the _currently_ closest airport
        const LiveATCDataMapTy::iterator apIter = curr->FindClosestAirport();
        if (apIter != curr->AirportStreamsEnd() &&
            apIter->first != curr->airportIcao)
        {
            // an(other) airport stream is closer, switch to it!
            
            // If there is something playing at the moment
            // move current stream aside (might stop pre-buffering)
            if (GetStatus() >= STREAM_BUFFERING) {
                TurnCurrToPrev();
                curr->SetFrequ(prev->GetFrequ());    // save frequency, stays the same!
                SHOW_MSG(logINFO, MSG_AP_CHANGE, idx+1,
                         apIter->second.streamName.c_str());
            }
            
            // copy the new airport's stream
            curr->CopyFrom(apIter->second);

            // ATIS handling: Only play this newly found stream if it is not ATIS or if LiveATC's ATIS is preferred
            if (dataRefs.PreferLiveATCAtis() || !curr->IsATIS())
                StartStreamAsync(false);
        }
        // If we are playing then check distance to current station
        else if (GetStatus() >= STREAM_BUFFERING)
        {
            positionTy posPlane = dataRefs.GetUsersPlanePos();
            // and stop playing if too far out
            if (posPlane.dist(curr->airportPos) / M_per_NM > dataRefs.GetMaxRadioDist()) {
                SHOW_MSG(logINFO, MSG_AP_OUT_OF_REACH, idx+1,
                         curr->streamName.c_str());
                StopStream(false);
            }
        }
    }
    
    // *** Checks on the second stream, only if pre-buffering ***
    if (prev->IsStandbyPrebuf()) {
        const LiveATCDataMapTy::iterator apIter = prev->FindClosestAirport();
        if (apIter != prev->AirportStreamsEnd() &&
            apIter->first != prev->airportIcao)
        {
            // an(other) airport stream is closer, switch to it!
            
            // If there is something buffering at the moment
            // just stop it
            if (prev->GetStatus() >= STREAM_BUFFERING) {
                prev->StopAndClear();
                LOG_MSG(logINFO, MSG_AP_STDBY_CHANGE, idx+1,
                        apIter->second.streamName.c_str());
            }
            
            // copy the new airport's stream
            prev->CopyFrom(apIter->second);
            prev->SetStandbyPrebuf(true);
            StartStreamAsync(true);
        }
        // If we are buffering then check distance to current station
        else if (prev->GetStatus() >= STREAM_BUFFERING)
        {
            positionTy posPlane = dataRefs.GetUsersPlanePos();
            // and stop playing if too far out
            if (posPlane.dist(prev->airportPos) / M_per_NM > dataRefs.GetMaxRadioDist()) {
                LOG_MSG(logINFO, MSG_AP_STDBY_OUT_OF_REACH, idx+1,
                        prev->streamName.c_str());
                StopStream(true);
            }
        }

    }
}

// stop VLC, reset frequency
void COMChannel::ClearChannel()
{
    // not initialized?
    if (!IsValid())
        return;

    // stop output of prev
    StopStream(true);
    
    // (if needed abort startup and) stop output of curr
    AbortAndWaitForAsync();         // this might block till StartStream aborts
    StopStream(false);
}

// COM channel's status, mostly depends on `curr->GetStatus()` but takes StartStream() into consideration
StreamStatusTy COMChannel::GetStatus() const
{
    // it actually _is_ the stream's status in all but one cases
    StreamStatusTy streamStatus = curr->GetStatus();
    
    // There are some override cases:
    
    // If we are not playing then we might be actively searching:
    if (streamStatus == STREAM_NOT_PLAYING)
        return IsAsyncRunning() ? STREAM_SEARCHING : STREAM_NOT_PLAYING;

    return streamStatus;
}

// Textual status summary for end user
std::string COMChannel::Summary (bool bPrev) const
{
    return bPrev ? prev->Summary() : curr->Summary(GetStatus());
}

// Textual status summary for debug purposes
std::string COMChannel::dbgStatus (bool bPrev) const
{
    return bPrev ? prev->dbgStatus() : curr->dbgStatus();
}

//
// MARK: Static functions
//

// Initialize *all* VLC instances
bool COMChannel::InitAllVLC()
{
    // in case of a re-init we firstly stop orderly
    if (gVLCInst)
        CleanupAllVLC();
    
    // create global VLC instance
    if (!CreateVLCInstance())
        return false;

    // fetch all possible output devices
    UpdateVLCOutputDevices();
    
    // create channels
    for (COMChannel& chn: gChn) {
        if (!chn.InitVLC())
            return false;
    }
    return true;
}

// static function: stop still running VLC instances
/// @warning Blocks! Expected to be called during deactivation/shutdown only.
void COMChannel::StopAll()
{
    for (COMChannel& chn: gChn) {
        chn.StopStream(true);
        chn.StopStream(false);
    }
}

// Cleanup *all* VLC instances, also stops all playback
void COMChannel::CleanupAllVLC()
{
    // cleanup the channels
    for (COMChannel& chn: gChn)
        chn.CleanupVLC();
    
    // cleanup the central VLC instance object
    CleanupVLCInstance();
}

// Update list of available audio devices
void COMChannel::UpdateVLCOutputDevices()
{
    // just use any existing MediaPlayer instance to do so:
    if (!gChn[0].dataA.pMP)
        return;
    gVLCOutputDevs.clear();
    gVLCOutputDevs = std::move(gChn[0].dataA.pMP->outputDeviceEnum());
}

// Set all MediaPlayer to use the given audio device
void COMChannel::SetAllAudioDevice(const std::string& devId)
{
    for (COMChannel& chn : gChn) {
        if (chn.dataA.pMP)
            chn.dataA.pMP->outputDeviceSet(devId);
        if (chn.dataB.pMP)
            chn.dataB.pMP->outputDeviceSet(devId);
    }
}


// Set the volume of all playback streams
void COMChannel::SetAllVolume(int vol)
{
    for (COMChannel& chn : gChn) {
        chn.dataA.SetVolume(vol);
        chn.dataB.SetVolume(vol);
    }
}

// (un)Mute all playback streams
void COMChannel::MuteAll(bool bDoMute)
{
    for (COMChannel& chn : gChn) {
        chn.dataA.SetMute(bDoMute);
        chn.dataB.SetMute(bDoMute);
    }
}

// checks if there is any active stream playing ATIS
bool COMChannel::AnyATISPlaying()
{
    for (const COMChannel& chn : gChn)
        if (chn.curr->IsATIS())
            return true;
    return false;
}

//
// MARK: Protected functions
//

/// Main function to react on a frequency change.
/// Compares new frequency to current frequency.
/// If frequency changes then the process to look up a
/// LiveATC stream and its subsequent startup is triggered,
/// also leading to the current stream being phased out.
bool COMChannel::doChange(int _new)
{
    if (_new != curr->GetFrequ()) {
        
        // Are we pre-buffering the new frequency already in prev?
        if (prev->IsStandbyPrebuf() && prev->GetFrequ() == _new)
        {
            // stop the current stream playback
            // if the pre-buffering is done already
            if (prev->IsDesyncDone())
                StopStream(false);
            // exchange the streams
            std::swap(prev, curr);
            SHOW_MSG(logINFO, MSG_COM_IS_NOW_IN, idx+1, curr->GetFrequStr().c_str(),
                     curr->streamName.c_str(),
                     dataRefs.GetDesyncPeriod());
            // pre-buffering is done, we are now active
            curr->SetStandbyPrebuf(false);
            // The -newly- previous stream defines the initial stand-by frequency,
            // which we are _not_ to pre-buffer. We just stopped listening to it!
            initFrequStandBy = prev->GetFrequ();
            // make sure volume is correct so we (usually) hear something
            SetVolumeMute();
            return false;
        }
        
        // move current stream aside
        TurnCurrToPrev();
        // save the new frequency
        curr->SetFrequ(_new);
        return true;
    } else {
        return false;
    }
}

/// The standby frequency is pre-buffered so that when switching
/// COM frequencies the audio desync period has already passed.
/// In effect, the new audio stream is available to the pilot immediately.
/// (S)He has not to wait till audio desync is done.
/// For pre-bufferung the stream is certainly muted.
///
/// Pre-buffering puts an additional burden on the underlying LiveATC
/// servers. So we try to do it as nice and intelligent as possible:
///
/// - It only happens once the standby frequency _changes_ again.
///   It won't happen to the previously active frequency only
///   because frequencies swapped. The pilot needs to dial a new
///   standby frequency first.
/// - It is not applied to ATIS streams as we play ATIS streams without
///   audio desync anyway. They can be played without any further
///   delay once they are activated.
/// - We use the `prev` object for pre-buffering and only if it is available.
///   If `prev` is still busy while switching to a new `curr` stream,
///   which is desyncing, then we wait. We won't put a third load in parallel
///   on the LiveATC servers.
bool COMChannel::doStandbyPrebuf(int _new)
{
    // One-time init: If the initial stand-by frequency had not been set before
    //                do so now.
    if (!initFrequStandBy)
        initFrequStandBy = dataRefs.GetComStandbyFreq(idx);

    // Don't we need pre-buffering at all according to configuration?
    if (dataRefs.GetDesyncPeriod() <= 0 ||          // no desync, no need for buffering
        !dataRefs.ShallPreBufferStandbyFrequ())
        return false;
    
    // As long as the standby frequency has not changed away
    // from its original value we do nothing.
    if (_new == initFrequStandBy)
        return false;
    
    // Dialing a frequency is something that takes a moment as the pilot
    // really has to "dial" the KHz and the Hz part of the frequency
    // separately. So we wait for the standby frequency to stay stable
    // between two calls (usually 10s apart) before we take action.
    
    // Is _new different from the last we saw 10s ago?
    if (_new != lastFrequStandby) {
        // Then let's remember what we see now
        lastFrequStandby = _new;
        
        // In case we are pre-buffering already right now we can, however,
        // stop that pre-buffering already.
        if (prev->IsDefined() && prev->IsStandbyPrebuf())
            StopStream(true);
        
        // but don't start anything now...standby frequency is still being changed
        return false;
        
    }
    
    // The stand-by frequency is different from initial and stayed the same for 10s.
    
    // Is prev already pre-buffering our frequency?
    if (prev->IsStandbyPrebuf() && prev->GetFrequ() == _new)
        return false;
    
    // Is prev not available for us because it's busy with something else?
    if (prev->IsDefined() && !prev->IsStandbyPrebuf())
        return false;
    
    // But there's another async startup running right now?
    // That must have priority
    if (IsAsyncRunning())
        return false;
    
    // So: We can use prev and _start_ some pre-buffering
    
    // just in case a clean cleanup
    StopStream(true);
    
    // Start the pre-buffering
    prev->SetFrequ(_new);
    prev->SetStandbyPrebuf(true);
    StartStreamAsync(true);
    return true;
}

// VLC play control

void COMChannel::StartStreamAsync (bool bStandby)
{
    // Don't want to run multiple threads in parallel...this might block!
    AbortAndWaitForAsync();
    // Start the stream asynchronously
    futVlcStart = std::async(std::launch::async,
                             &COMChannel::StartStream, this, bStandby);
}

/// Starts the `curr` streams. Expects `curr.frequ` to be properly set.
/// All else will be determined by this function.
/// @warning This function can take several seconds to complete as several
///          HTTP requests are to be done!
void COMChannel::StartStream (bool bStandby)
{
    // start it only once...it could take a moment
    if (flagStartingStream.test_and_set())
        return;     // return immediately if startup in progress
    
    // other threads can set this flag to have StartSteam abort early
    bAbortStart = false;
    
    // which stream are we working on?
    StreamCtrlTy& strm = bStandby ? *prev : *curr;

    // For how long shall audio desync last?
    long desyncSecs = dataRefs.GetDesyncPeriod();
    
    // handling of secondary stream only if we start curr
    if (!bStandby)
    {
        // if there is no audio desync or we shall not wait for it
        // then stop the pushed-aside stream right away
        if (desyncSecs <= 0 || !dataRefs.ShallRunPrevFrequTillDesync()) {
            curr->ClearDesyncTimer();
            StopStream(true);
        } else if (desyncSecs > 0) {
            // save time when audio desync should be finished
            curr->SetAudioDesync(desyncSecs);
        }

        // Temporarily deactivate XP's ATIS.
        // We only know if the new frequency is an ATIS frequency
        // once we queried LiveATC. That can take 1 or 2 seconds or even longer.
        // During this delay XP might already start chattering its own ATIS.
        // We stop that here and re-enable later if the channel happened not to be
        // an ATIS channel.
        if (dataRefs.PreferLiveATCAtis())
            dataRefs.EnableXPsATIS(false);
    }
    else
    {
        // pre-buffering not needed if there is no desync
        if (desyncSecs <= 0)
            bAbortStart = true;
    }

    // playUrl might have been filled by RegularMaintenance
    // when an airport came in reach
    if (!bAbortStart && strm.playUrl.empty()) {
        // find a new URL of a stream to play -> playUrl
        if (!strm.FetchUrlForFrequ())
            bAbortStart = true;
    }
    
    // strm is now updated and contains the to-be stream
    
    // in most cases the initial URL points to a .pls file,
    // which is a simple playlist format.
    // To avoid running LUA scripts for parsing HTTP responses in the VLC instance
    // (which is difficult to control from another application not
    //  running right within the VLC folder)
    // we quickly parse the .pls format ourselves and extract the actual URL
    
    // Example for https://www.liveatc.net/play/kjfk_gnd.pls :
    //      [playlist]
    //      File1=http://d.liveatc.net/kjfk_gnd
    //      Title1=KJFK Ground
    //      Length1=-1
    
    if (!bAbortStart && endsWith(strm.playUrl, LIVE_ATC_PLS)) {
        std::string playlist;
        if (HttpGet(strm.playUrl, playlist))
        {
            static std::regex rePlsFile1 ( R"#(File1=(http\S+))#" );
            std::smatch m;
            if (!std::regex_search(playlist, m, rePlsFile1))
            { LOG_MSG(logWARN, WARN_RE_ICAO, "File1"); }
            else strm.playUrl = { m[1].str() };
        }
        else
        {
            // HTTP went wrong, clear this channel for now so we don't try again without the user doing someting
            strm.StopAndClear();
            bAbortStart = true;
        }
    }
    
    // *** ATIS handling ***
    if (!bAbortStart && strm.IsATIS())
    {
        // If strm is an ATIS stream and we are to pre-buffer
        // then just don't do it: We don't need to pre-buffer ATIS
        // as it will be played without delay anyway
        if (bStandby) {
            bAbortStart = true;
        } else {
            // We don't desync ATIS streams!
            desyncSecs = 0;             // no desync period
            curr->ClearDesyncTimer();
            StopStream(true);           // latest now kill pushed-aside previous stream
            if (dataRefs.PreferLiveATCAtis()) {
                // we will play LiveATC, so suppress X-Plane's internal ATIS
                dataRefs.EnableXPsATIS(false);
            } else {
                // X-Plane's ATIS preferred! So we do not play LiveATC's stream
                dataRefs.EnableXPsATIS(true);
                LOG_MSG(logINFO, MSG_COM_IS_NOW_IN, idx+1, strm.GetFrequStr().c_str(),
                        strm.streamName.c_str());
                // return
                bAbortStart = true;
            }
        }
    }
    
    // abort early?
    if (bAbortStart) {
        strm.ClearDesyncTimer();
        if (!bStandby) {
            StopStream(true);           // latest now kill pushed-aside previous stream
        } else {
            // Tried starting a stand-by pre-buffering,
            // but failed for some reason. Make sure we don't try this
            // frequency again. We do so by expecting the pilot to dial
            // something else:
            initFrequStandBy = strm.GetFrequ();
        }
        flagStartingStream.clear();
        return;
    }
    
    // tell the world we work on a frequency
    if (!bStandby) {
        if (desyncSecs > 0) {
            SHOW_MSG(logINFO, MSG_COM_IS_NOW_IN, idx+1, strm.GetFrequStr().c_str(),
                     strm.streamName.c_str(),
                     desyncSecs);
        } else {
            SHOW_MSG(logINFO, MSG_COM_IS_NOW, idx+1, strm.GetFrequStr().c_str(),
                     strm.streamName.c_str());
        }
    } else {
        LOG_MSG(logINFO, MSG_STBY_IS_NOW_IN, idx+1, strm.GetFrequStr().c_str(),
                strm.streamName.c_str(),
                desyncSecs);
    }
    
    // Create and play the media
    strm.pMedia = std::make_unique<VLC::Media>(*gVLCInst,
                                               strm.playUrl,
                                               VLC::Media::FromLocation);
    strm.pMP->setMedia(*strm.pMedia);
    if (!strm.pMP->play()) {
        // playback failed
        strm.ClearDesyncTimer();
        SHOW_MSG(logERR, ERR_VLC_PLAY, strm.playUrl.c_str(), vlcErrMsg().c_str());
    } else {
        // playback started successfully
        if (desyncSecs > 0) {
            // set audio desync if requested and the desync timer
            strm.SetAudioDesync(desyncSecs);
        }

        // set audio device and volume
        strm.pMP->outputDeviceSet(dataRefs.GetAudioDev());
        SetVolumeMute();
    }
    
    // done starting this stream
    flagStartingStream.clear();
}

void COMChannel::StopStream (bool bPrev)
{
    StreamCtrlTy& atcData = bPrev ? *prev : *curr;
    
    // stop that stream
    if (!atcData.GetFrequStr().empty() && atcData.GetStatus() >= STREAM_BUFFERING) {
        LOG_MSG(logDEBUG, DBG_STREAM_STOP,
                atcData.streamName.c_str(), atcData.GetFrequStr().c_str());
    }
    atcData.StopAndClear();

    // stop any timer
    atcData.ClearDesyncTimer();
}

/// determines and sets proper volum/mute status
void COMChannel::SetVolumeMute ()
{
    // globally muted? Or this channel muted?
    if (dataRefs.IsMuted() || dataRefs.ShallMuteCom(idx)) {
        curr->SetMute(true);
        prev->SetMute(true);
    } else {
        curr->SetVolume(dataRefs.GetVolume());
        if (prev->IsStandbyPrebuf())
            prev->SetMute(true);
        else
            prev->SetVolume(dataRefs.GetVolume());
    }
}

// Checks if an async StartStream() operation is in progress
bool COMChannel::IsAsyncRunning () const
{
    return futVlcStart.valid() &&
    futVlcStart.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

// Wait for StartStream() to complete
void COMChannel::AbortAndWaitForAsync()
{
    if (futVlcStart.valid()) {
        bAbortStart = true;         // let StartStream() abort as soon as possible
        futVlcStart.get();
    }
}

/// If `prev` is still active it is stopped first, which would block
void COMChannel::TurnCurrToPrev()
{
    // if there's still a previous stream running kill it
    if (prev->IsDefined())
        StopStream(true);
    
    // now swap curr<->prev, so that curr then is an initialized fresh object
    std::swap(curr, prev);
    
    // The -newly- previous stream defines the initial stand-by frequency,
    // which we are _not_ to pre-buffer. We just stopped listening to it!
    initFrequStandBy = prev->GetFrequ();
    
    // if the -now- previous stream is playing then start the desync countdown
    // (will be set again in StartStream, but if we don't
    //  set anything now then it might be stopped early by
    //  RegularMainteanance(), which runs in main thread)
    // save time when audio desync should be finished
    long desyncSecs = dataRefs.GetDesyncPeriod();
    if (desyncSecs > 0)
        curr->SetAudioDesync(desyncSecs);
}

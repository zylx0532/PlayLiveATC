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
    "--no-lua"
};

/// Number of `vlcArgs`
constexpr size_t vlcArgC = sizeof(vlcArgs)/sizeof(vlcArgs[0]);

//
// MARK: Helper structs
//

// static function to return a status text
std::string GetStatusStr (StreamStatusTy s)
{
    switch (s) {
        case STREAM_NOT_INIT:   return "not initialized";
        case STREAM_NO_FREQU:   return "no frequency";
        case STREAM_NOT_PLAYING:return "not playing";
        case STREAM_SEARCHING:  return "searching";
        case STREAM_BUFFERING:  return "buffering";
        case STREAM_PLAYING:    return "playing";
        default: return "?";        // must not happen
    }
}



/// Stream's status, cannot decide all but STREAM_SEARCHING (see COMChannel::GetStatus())
StreamStatusTy StreamCtrlTy::GetStatus() const
{
    // not initialized at all?
    if (!IsValid())
        return STREAM_NOT_INIT;
    
    // playing a stream, i.e. emmitting sound?
    if (pMP->isPlaying())
        return STREAM_PLAYING;
    
    // media defined? Then let's assume VLC is instructed to play and will soon
    if (pMedia)
        return STREAM_BUFFERING;
    
    // no frequency selected?
    if (!frequ || frequString.empty())
        return STREAM_NO_FREQU;
    
    // so there's a frequency but no media? Then we are not playing it.
    return STREAM_NOT_PLAYING;
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
    // tell VLC where to find the plugins
    setenv(ENV_VLC_PLUGIN_PATH, dataRefs.GetVLCPath().c_str(), 1);
    
    // just in case - cleanup in proper order
    CleanupVLC();

    // create VLC instance
    try {
        dataA.pInst     = std::make_unique<VLC::Instance>(vlcArgC, vlcArgs);
        dataA.pInst->setAppId(PLUGIN_SIGNATURE, PLA_VERSION_FULL, "");
        dataA.pInst->setUserAgent(HTTP_USER_AGENT, HTTP_USER_AGENT);
        dataA.pMP       = std::make_unique<VLC::MediaPlayer>(*dataA.pInst);
        
        dataB.pInst     = std::make_unique<VLC::Instance>(vlcArgC, vlcArgs);
        dataB.pInst->setAppId(PLUGIN_SIGNATURE, PLA_VERSION_FULL, "");
        dataB.pInst->setUserAgent(HTTP_USER_AGENT, HTTP_USER_AGENT);
        dataB.pMP       = std::make_unique<VLC::MediaPlayer>(*dataA.pInst);
        
        return true;
    }
    catch (std::runtime_error e)
    {
        LOG_MSG(logERR, ERR_VLC_INIT, e.what());
    }
    catch (...)
    {
        LOG_MSG(logERR, ERR_VLC_INIT, "?");
    }
    CleanupVLC();
    return false;
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
    dataB.pInst     = nullptr;
    dataA.pMedia    = nullptr;
    dataA.pMP       = nullptr;
    dataA.pInst     = nullptr;
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
    
    // need to stop previous frequency after desync is done?
    if (prev->IsDefined() && IsDesyncDone()) {
        StopStream(true);
    }
    
    // get current frequency and check if this is considered a change
    if (doChange(dataRefs.GetComFreq(idx))) {
        // it is: we start a new stream
        StartStreamAsync();
        return;
    }
    
    // *** only every 10th call do the expensive stuff ***
    static int callCnt = 0;
    if (++callCnt <= 10)
        return;
    callCnt = 0;
    
//    LOG_MSG(logDEBUG, "curr: %s", curr->dbgStatus().c_str());
//    LOG_MSG(logDEBUG, "prev: %s", prev->dbgStatus().c_str());

    // If we are playing then check distance to current station
    if (GetStatus() >= STREAM_BUFFERING) {
        positionTy posPlane = dataRefs.GetUsersPlanePos();
        // and stop playing if too far out
        if (posPlane.dist(curr->airportPos) / M_per_NM > dataRefs.GetMaxRadioDist()) {
            SHOW_MSG(logINFO, MSG_AP_OUT_OF_REACH, curr->streamName.c_str());
            StopStream(false);
        }
    } else if (GetStatus() == STREAM_NOT_PLAYING) {
        // we have a frequency but are not currently playing anything,
        // find closest airport in mapAirportStream,
        // maybe something is in reach now?
        const LiveATCDataMapTy::iterator apIter = FindClosestAirport();
        if (apIter != mapAirportStream.end()) {
            // found an airport, so if we just start the stream now we will find and play that one:
            curr->CopyFrom(apIter->second);
            StartStreamAsync();
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
    if (streamStatus != STREAM_NOT_PLAYING)
        return streamStatus;
    
    // if steam is NOT_PLAYING then we might just be searching
    return IsAsyncRunning() ? STREAM_SEARCHING : STREAM_NOT_PLAYING;
}

// return number of seconds till desync should be done
int COMChannel::GetSecTillDesyncDone () const
{
    return int(std::chrono::duration_cast<std::chrono::seconds>
    (desyncDone - std::chrono::steady_clock::now()).count());
}

// Textual status summary for end user
std::string COMChannel::Summary (bool bFull) const
{
    return bFull ?
    curr->Summary() + "\nPrev: " + prev->Summary() :
    curr->Summary();
}

// Textual status summary for debug purposes
std::string COMChannel::dbgStatus (bool bFull) const
{
    return bFull ?
    curr->dbgStatus() + "\nPrev: " + prev->dbgStatus() :
    curr->dbgStatus();
}

//
// MARK: Static functions
//

// Initialize *all* VLC instances
bool COMChannel::InitAllVLC()
{
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
    for (COMChannel& chn: gChn)
        chn.CleanupVLC();
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
    if (_new != curr->frequ) {
        // move current stream aside
        TurnCurrToPrev();
        // save the new frequency
        char buf[20];
        curr->frequ = _new;
        snprintf(buf, sizeof(buf),
                 "%d.%03d",
                 curr->frequ / 1000, curr->frequ % 1000);
        curr->frequString = buf;
        return true;
    } else {
        return false;
    }
}

// VLC play control

void COMChannel::StartStreamAsync ()
{
    // we don't need control over the thread, it lasts for 2s only
    futVlcStart = std::async(std::launch::async, &COMChannel::StartStream, this);
}

/// Starts the `curr` streams. Expects `curr.frequ` to be properly set.
/// All else will be determined by this function.
/// @warning This function can take several seconds to complete as several
///          HTTP requests are to be done!
void COMChannel::StartStream ()
{
    // start it only once...it could take a moment
    if (flagStartingStream.test_and_set())
        return;     // return immediately if startup in progress
    
    // other threads can set this flag to have StartSteam abort early
    bAbortStart = false;
    
    // save time when audio desync should be finished
    long desyncSecs = dataRefs.GetDesyncPeriod();
    if (desyncSecs > 0)
        desyncDone = std::chrono::steady_clock::now() +
        std::chrono::seconds(desyncSecs + ADD_COUNTDOWN_DELAY_S);
    
    // if there is no audio desync or we shall not wait for it
    // then kill the pushed-aside process right away
    if (desyncSecs <= 0 || !dataRefs.ShallRunPrevFrequTillDesync()) {
        StopStream(true);
    }

    // playUrl might have been filled by RegularMaintenance
    // when an airport came in reach
    if (curr->playUrl.empty()) {
        // find a new URL of a stream to play -> playUrl
        if (!FetchUrlForFrequ() || bAbortStart) {
            flagStartingStream.clear();
            return;
        }
    }
    
    // curr is now updated and contains the to-be stream
    
    // tell the world we work on a frequency
    if (desyncSecs > 0) {
        SHOW_MSG(logINFO, MSG_COM_IS_NOW_IN, idx+1, curr->frequString.c_str(),
                 curr->streamName.c_str(),
                 desyncSecs);
    } else {
        SHOW_MSG(logINFO, MSG_COM_IS_NOW, idx+1, curr->frequString.c_str(),
                 curr->streamName.c_str());
    }
    
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
    
    if (endsWith(curr->playUrl, LIVE_ATC_PLS)) {
        std::string playlist;
        if (HttpGet(curr->playUrl, playlist))
        {
            static std::regex rePlsFile1 ( R"#(File1=(http\S+))#" );
            std::smatch m;
            if (!std::regex_search(playlist, m, rePlsFile1))
            { LOG_MSG(logWARN, WARN_RE_ICAO, "File1"); }
            else curr->playUrl = { m[1].str() };
        }
        else
        {
            // HTTP went wrong, clear this channel for now so we don't try again without the user doing someting
            curr->StopAndClear();
            bAbortStart = true;
        }
    }
    
    // abort early?
    if (bAbortStart) {
        flagStartingStream.clear();
        return;
    }

    // Create and play the media
    curr->pMedia = std::make_unique<VLC::Media>(*curr->pInst, curr->playUrl, VLC::Media::FromLocation);
    curr->pMP->setMedia(*curr->pMedia);
    if (!curr->pMP->play()) {
        // playback failed
        desyncDone = std::chrono::time_point<std::chrono::steady_clock>();
        SHOW_MSG(logERR, ERR_VLC_PLAY, curr->playUrl.c_str(), vlcErrMsg().c_str());
    } else {
        // playback started successfully
        if (desyncSecs > 0) {
            // set audio desync if requested (microseconds!)
            curr->pMP->setAudioDelay(int64_t(desyncSecs) * 1000000L);
            // now set the desync timer again, should be more reliable now
            // after many HTTP queries are done and actuall playback started
            desyncDone = std::chrono::steady_clock::now() +
            std::chrono::seconds(desyncSecs);
        }
    }
    
    // done starting this stream
    flagStartingStream.clear();
}

void COMChannel::StopStream (bool bPrev)
{
    StreamCtrlTy& atcData = bPrev ? *prev : *curr;
    
    // stop that stream
    if (!atcData.frequString.empty()) {
        LOG_MSG(logDEBUG, DBG_STREAM_STOP,
                atcData.streamName.c_str(), atcData.frequString.c_str());
    }
    atcData.StopAndClear();
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

// compute complete parameter string for VLC
std::string COMChannel::GetVlcParams()
{
    std::string params = dataRefs.GetVLCParams();
    str_replace(params, PARAM_REPL_DESYNC, std::to_string(dataRefs.GetDesyncPeriod()));
    str_replace(params, PARAM_REPL_URL, curr->playUrl);
    return params;
}

/// If `prev` is still active it is stopped first, which would block
void COMChannel::TurnCurrToPrev()
{
    // if there's still a previous stream running kill it
    if (prev->IsDefined())
        StopStream(true);
    
    // now swap curr<->prev, so that curr then is an initialized fresh object
    std::swap(curr, prev);
}

//
// MARK: Determine new stream URL to play
//

/// 1. Queries LiveATC.net with defined frequency using HttpGet()
/// 2. Parses the resulting web page for airport-specific streams (ParseForAirportStreams())
/// 3. Finds the airport closest to current location (FindClosestAirport())
/// 4. If an airport-stream is in reach copies its data into `curr`
bool COMChannel::FetchUrlForFrequ ()
{
    // ask LiveATC, fills readBuf
    // put together the url
    char url[100];
    snprintf(url, sizeof(url), LIVE_ATC_URL, curr->frequString.c_str());
    if (!HttpGet(url, readBuf)) {
        // HTTP went wrong, clear this channel for now so we don't try again without the user doing someting
        curr->StopAndClear();
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
    curr->CopyFrom(apIter->second);
    return true;
}

// parse readBuffer and fill mapAirportStream
void COMChannel::ParseForAirportStreams ()
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
LiveATCDataMapTy::iterator COMChannel::FindClosestAirport()
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
         iter++)
    {
        // if we don't know the airports position yet
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
        } else {
            LOG_MSG(logDEBUG, DBG_AP_NOT_FOUND, iter->first.c_str());
        }
    }
    
    if (closestAirport != mapAirportStream.end())
        LOG_MSG(logDEBUG,DBG_AP_CLOSEST,closestAirport->first.c_str(), closestDist_nm)
//    else
//        LOG_MSG(logDEBUG,DBG_AP_NO_CLOSEST,closestDist_nm)

    return closestAirport;
}


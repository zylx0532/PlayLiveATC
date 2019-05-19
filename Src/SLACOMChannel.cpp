//
//  SLACOMChannel.cpp
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

#include "SwitchLiveATC.h"

#if IBM
#include <shellapi.h>
#else
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#endif
#if LIN
#include <wait.h>
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
    StopStreamAsync(true);
    StopStreamAsync(false);
    frequ = 0;
    frequString.clear();
    curr = LiveATCDataTy();
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

void COMChannel::StartStreamAsync ()
{
    // we don't need control over the thread, it lasts for 2s only
    futVlcStart = std::async(std::launch::async, &COMChannel::StartStream, this);
}

void COMChannel::StopStreamAsync (bool bPrev)
{
    // we don't need control over the thread, it lasts for 2s only
    if (IsPlaying(bPrev))
        futVlcStart = std::async(std::launch::async, &COMChannel::StopStream, this, bPrev);
}

// check distance and then don't change anything, stop the channel, or switch over to another radio
void COMChannel::CheckComDistance ()
{
    // If we are playing then check distance to current station
    if (IsPlaying(false)) {
        positionTy posPlane = dataRefs.GetUsersPlanePos();
        // and stop playing if too far out
        if (posPlane.dist(currPos) / M_per_NM > dataRefs.GetMaxRadioDist()) {
            SHOW_MSG(logINFO, MSG_AP_OUT_OF_REACH, curr.streamName.c_str());
            StopStreamAsync(false);
        }
    } else {
        // find closest airport in mapAirportStream
        const AirportStreamMapTy::iterator apIter = FindClosestAirport();
        if (apIter != mapAirportStream.end()) {
            // found an airport, use that data from now on
            curr = apIter->second;
            StartStreamAsync();
        }
    }
}

void COMChannel::StartStream ()
{
    // find a new URL of a stream to play -> playUrl
    if (!FetchUrlForFrequ())
        return;
    
    // tell the world we work on a frequency
    SHOW_MSG(logINFO, MSG_COM_IS_NOW, idx+1, frequString.c_str(), curr.streamName.c_str());
    
    // make sure there is no stream running at the moment
    // TODO: Let old stream run until desync is over
    StopStream(false);

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
        vlcPid = exeVlc.hProcess;
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
        LOG_MSG(logDEBUG, DBG_FORK_PID, vlcPid, curr.playUrl.c_str());
        
        // wait for 2 seconds and then check if the child already terminated
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // expected is that the child is still running!
        int stat = 0;
        if (waitpid(vlcPid, &stat, WNOHANG) != 0) {
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

void COMChannel::StopStream (bool bPrev)
{
#if IBM
    // work on either normal or next process
    HANDLE& pid = bPrev ? vlcPrev : vlcPid;
    if (pid) {
        // we send WM_QUIT, just in case...seems to clean up tray icon at least
        HWND hwndVLC = FindMainWindow(pid);
        if (hwndVLC) {
            PostThreadMessageA(GetWindowThreadProcessId(hwndVLC, NULL), WM_QUIT, 0, 0);
            // can WaitForSingleObject do this wait "better"?
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        
        // this is not nice...but VLC doesn't react properly to WM_QUIT or WM_DESTROY: The window's gone, but not the process.
        TerminateProcess(pid, 0);
        pid = NULL;
    }
#else
    // work on either normal or next process
    int& pid = bPrev ? vlcPrev : vlcPid;
    if (pid > 0) {
        // process still running?
        int stat = 0;
        if (waitpid(pid, &stat, WNOHANG) <= 0) {
            LOG_MSG(logDEBUG, DBG_KILL_PID, pid, curr.playUrl.c_str());
            kill(pid, SIGTERM);
            // cleanup zombies (10 attempts with 100ms pause)
            for (int i = 0;
                 waitpid(pid, &stat, WNOHANG) == 0 && i < 10;
                 i++)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
//                LOG_MSG(logDEBUG, "Waited %d. time for pid %d (%s)", i, pid, playUrl.c_str());
            }
        }
    }
    pid = 0;
#endif
}


// static function: stop still running VLC instances
// (synchronously! Expected to be called during deactivation/shutdown only)
void COMChannel::StopAll()
{
    for (COMChannel& chn: gChn) {
        chn.StopStream(true);
        chn.StopStream(false);
    }
}


// compute complete parameter string for VLC
std::string COMChannel::GetVlcParams()
{
    std::string params = dataRefs.GetVLCParams();
    // FIXME: Actual desync time!!!
    str_replace(params, PARAM_REPL_DESYNC, "0");
    str_replace(params, PARAM_REPL_URL, curr.playUrl);
    return params;
}

//
// MARK: Determine new stream URL to play
//

bool COMChannel::FetchUrlForFrequ ()
{
    // ask LiveATC, fills readBuf
    if (!QueryLiveATC())
        return false;
    
    // parse this response -> fill mapAirportStream
    ParseForAirportStreams();
    if (mapAirportStream.empty())
        return false;
    
    // find closest airport in mapAirportStream
    const AirportStreamMapTy::iterator apIter = FindClosestAirport();
    if (apIter == mapAirportStream.end())
        return false;
    
    // found an airport, use that data from now on
    curr = apIter->second;
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
        static std::regex reIcao ( R"#(<tr><td><strong>ICAO: </strong>(\w\w\w\w)<strong>)#" );
        static std::regex reName ( R"#(<td bgcolor="lightblue"><strong>(.+?)</strong>)#" );
        static std::regex reStat ( R"#(<tr><td><strong>Feed Status:</strong> <font color=\\?"\w+\\?"><strong>(\w+)</strong>)#" );
        static std::regex reUrl  ( R"#(<a href="(.+?)" onClick=)#" );

        if (!std::regex_search(apSec, m, reIcao))       { LOG_MSG(logWARN, WARN_RE_ICAO, "airport ICAO"); continue; }
        LiveATCDataTy streamData = { m[1].str() };
        
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
        AirportStreamMapTy::iterator mapIter = mapAirportStream.find(streamData.airportIcao);
        if (mapIter != mapAirportStream.end())
        {
            // only replace if current find is better.
            // Stream is considered better if there are less lines in the
            // 'facilities' table, i.e. the stream is more specific to the
            // searched frequency
            if (streamData.nFacilities < mapIter->second.nFacilities)
            {
                LOG_MSG(logDEBUG, DBG_REPL_STREAM, streamData.dbgOut().c_str());
                mapIter->second = std::move(streamData);
            }
        }
        else
        {
            LOG_MSG(logDEBUG, DBG_ADDING_STREAM, streamData.dbgOut().c_str());
            mapAirportStream.emplace(streamData.airportIcao, std::move(streamData));
        }
    }
}


// find closest airport in mapAirportStream
COMChannel::AirportStreamMapTy::iterator COMChannel::FindClosestAirport()
{
    // Sanity check...there must be any for us to find one
    if (mapAirportStream.empty())
        return mapAirportStream.end();
    
    // plane's position
    const positionTy planePos = dataRefs.GetUsersPlanePos();
    
    // loop airports in mapAirportStream and for each of it
    // determine distance to plane, remember the closest airport
    AirportStreamMapTy::iterator closestAirport = mapAirportStream.end();
    double closestDist_nm = dataRefs.GetMaxRadioDist();    // with this init we will not consider airports father away
    for (AirportStreamMapTy::iterator iter = mapAirportStream.begin();
         iter != mapAirportStream.end();
         iter++)
    {
        // find that airport in X-Plane's nav database
        float lat = NAN, lon = NAN, alt_m = NAN;
        XPLMNavRef apRef = XPLMFindNavAid(NULL, iter->first.c_str(), NULL, NULL, NULL, xplm_Nav_Airport);
        if (apRef) {
            XPLMGetNavAidInfo(apRef, NULL, &lat, &lon, &alt_m, NULL, NULL, NULL, NULL, NULL);
            const positionTy apPos (lat, lon, alt_m);
            // check distance to airport and if it is the closest seen so far
            double dist_nm = planePos.dist(apPos) / M_per_NM;
            if (dist_nm < closestDist_nm) {
                closestDist_nm = dist_nm;
                closestAirport = iter;
                currPos = apPos;
            }
        } else {
            LOG_MSG(logDEBUG, DBG_AP_NOT_FOUND, iter->first.c_str());
        }
    }
    
    if (closestAirport != mapAirportStream.end())
        LOG_MSG(logDEBUG,DBG_AP_CLOSEST,closestAirport->first.c_str(), closestDist_nm)
    else
        LOG_MSG(logDEBUG,DBG_AP_NO_CLOSEST,closestDist_nm)

    return closestAirport;
}

//
// MARK: Networking: Query LiveATC web page
//

// Disable revocation list? (if we need that once we'll always need it)
bool COMChannel::bDisableRevocationList = false;

// Find frequency on LiveATC, just dumps the entire response into readBuf
// This function blocks! Idea is to call it in a thread like with std::async
bool COMChannel::QueryLiveATC ()
{
    char curl_errtxt[CURL_ERROR_SIZE];
    char url[100];
    
    // put together the url
    snprintf(url, sizeof(url), LIVE_ATC_URL, frequString.c_str());
    LOG_MSG(logDEBUG, DBG_QUERY_URL, url);
    
    // initialize the CURL handle
    CURL *pCurl = curl_easy_init();
    if (!pCurl) {
        LOG_MSG(logERR,ERR_CURL_EASY_INIT);
        return "";
    }
    
    // prepare the handle with the right options
    readBuf.clear();
    readBuf.reserve(READ_BUF_INIT_SIZE);
    curl_easy_setopt(pCurl, CURLOPT_ERRORBUFFER, curl_errtxt);
    curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, CB_StoreAll);
    curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(pCurl, CURLOPT_USERAGENT, HTTP_USER_AGENT);
    curl_easy_setopt(pCurl, CURLOPT_URL, url);
    if (bDisableRevocationList)
        curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
    
    // perform the HTTP get request
    CURLcode cc = CURLE_OK;
    if ( (cc=curl_easy_perform(pCurl)) != CURLE_OK )
    {
        // problem with querying revocation list?
        if (strstr(curl_errtxt, ERR_CURL_REVOKE_MSG)) {
            // try not to query revoke list
            bDisableRevocationList = true;
            curl_easy_setopt(pCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NO_REVOKE);
            LOG_MSG(logWARN, ERR_CURL_DISABLE_REV_QU, LIVE_ATC_DOMAIN);
            // and just give it another try
            cc = curl_easy_perform(pCurl);
        }
        
        // if (still) error, then log error
        if (cc != CURLE_OK)
            LOG_MSG(logERR, ERR_CURL_NO_LIVEATC, cc, curl_errtxt);
    }
    
    // if CURL was OK also get HTTP response code
    long httpResponse = 0;
    if (cc == CURLE_OK) {
        curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &httpResponse);
        // not HTTP_OK?
        if (httpResponse != HTTP_OK)
            LOG_MSG(logERR, ERR_CURL_HTTP_RESP, url, httpResponse);
    }
    
    // cleanup CURL handle
    curl_easy_cleanup(pCurl);
    pCurl = NULL;
    
    // successful?
    return cc == CURLE_OK && httpResponse == HTTP_OK;
}

// This static CURL callback just collects all data. We analyse it after fetching all
size_t COMChannel::CB_StoreAll(char *ptr, size_t, size_t nmemb, void* userdata)
{
    // copy buffer to our readBuf
    COMChannel& me = *reinterpret_cast<COMChannel*>(userdata);
    me.readBuf.append(ptr, nmemb);
    
    // all consumed
    return nmemb;
}

//
//  DataRefs.cpp
//  SwitchLiveATC

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

//
// MARK: X-Plane Datarefs
//
const char* DATA_REFS_XP[] = {
    // XP standard
    "sim/cockpit2/radios/actuators/com1_frequency_hz_833",  // int    y    hertz    Com radio 1 frequency, hz, supports 8.3 khz spacing
    "sim/cockpit2/radios/actuators/com2_frequency_hz_833",  // int    y    hertz    Com radio 2 frequency, hz, supports 8.3 khz spacing
    "sim/cockpit2/radios/actuators/audio_selection_com1",   // int    y    boolean    is com1 selected for listening
    "sim/cockpit2/radios/actuators/audio_selection_com2",   // int    y    boolean    is com2 selected for listening
    "sim/flightmodel/position/latitude",                    // user's plane's position
    "sim/flightmodel/position/longitude",
    "sim/flightmodel/position/elevation",
    "sim/flightmodel/position/true_theta",
    "sim/flightmodel/position/true_phi",
    "sim/flightmodel/position/true_psi",
    "sim/flightmodel/position/hpath",
    "sim/flightmodel/position/true_airspeed",
    "sim/flightmodel/failures/onground_any",
    // LiveTraffic
    "livetraffic/cfg/aircrafts_displayed",
    "livetraffic/cfg/fd_buf_period",
};

static_assert(sizeof(DATA_REFS_XP) / sizeof(DATA_REFS_XP[0]) == CNT_DATAREFS_XP,
    "dataRefsXP and DATA_REFS_XP[] differ in number of elements");

//
// MARK: SwitchLiveATC Command Refs
//
struct cmdRefDescrTy {
    const char* cmdName;
    const char* cmdDescr;
} CMD_REFS_SLA[] = {
    {"SwitchLiveATC/Toggle_ActOn_COM1", "Toggle acting on COM1 frequency change"},
    {"SwitchLiveATC/Toggle_ActOn_COM2", "Toggle acting on COM2 frequency change"},
};

static_assert(sizeof(CMD_REFS_SLA) / sizeof(CMD_REFS_SLA[0]) == CNT_CMDREFS_SLA,
              "cmdRefsLT and CMD_REFS_LT[] differ in number of elements");

//
//MARK: Constructor - just plain variable init, no API calls
//
DataRefs::DataRefs ( logLevelTy initLogLevel ) :
iLogLevel (initLogLevel)
{
    // override log level in Beta and DEBUG cases
    // (config file is read later, that may reduce the level again)
#ifdef DEBUG
    iLogLevel = logDEBUG;
#else
    if ( SLA_BETA_VER_LIMIT )
        iLogLevel = logDEBUG;
#endif
    
    // Clear the dataRefs arrays
    memset ( adrXP, 0, sizeof(adrXP));
}

// Find and register dataRefs
bool DataRefs::Init ()
{
    // XP System Path
    char aszPath[512];
    XPLMGetSystemPath ( aszPath );
    XPSystemPath = aszPath;
    
    // Directory Separator provided by XP
    DirSeparator = XPLMGetDirectorySeparator();
    
    // my own plugin path
    pluginID = XPLMGetMyID();
    aszPath[0] = 0;
    XPLMGetPluginInfo(pluginID, NULL, aszPath, NULL, NULL);
    PluginPath = aszPath;
    LOG_ASSERT(!PluginPath.empty());
    
    // PluginPath is now something like "...:Resources:plugins:SwitchLiveATC:64:mac.xpl"
    // we now reduce the path to the beginning of the plugin:
    // remove the file name
    std::string::size_type pos = PluginPath.rfind(DirSeparator);
    LOG_ASSERT(pos != std::string::npos);
    PluginPath.erase(pos);
    // remove the 64 subdirectory, but leave the separator at the end
    pos = PluginPath.rfind(DirSeparator);
    LOG_ASSERT(pos != std::string::npos);
    PluginPath.erase(pos+1);
    
    // find XP dataRefs
    if (!FindDataRefs(true))
        return false;
    
    // register all self-provided commands, errors aren't critical here
    RegisterCommands();

    // pre-fill VLC path with a good guess
    if (existsFile(CFG_PATH_DEFAULT))
        VLCPath = CFG_PATH_DEFAULT;

    // read configuration file if any
    if (!LoadConfigFile())
        return false;
    
    // make sure there is some parameters, won't work without
    if (VLCParams.empty())
        VLCParams = CFG_PARAMS_DEFAULT;
    
    return true;
}

// "Late init" means to be called from first flight-loop callback
// by that time also plugin-provided dataRefs should be accessible
bool DataRefs::LateInit ()
{
    // find plugin's dataRefs
    return FindDataRefs(false);
}

// Unregister (destructor would be too late for reasonable API calls)
void DataRefs::Stop ()
{}

// find dataRefs, distinguish between early (XP standard only) and later (other plugins)
bool DataRefs::FindDataRefs (bool bEarly)
{
    // Fetch all XP-provided resp. other-plugin's data refs and verify if OK
    const int from =    bEarly ? 0              : DR_FIRST_LT_DR;
    const int to =      bEarly ? DR_FIRST_LT_DR : CNT_DATAREFS_XP;
    
    for (int i = from; i < to; i++ )
    {
        if ( (adrXP[i] = XPLMFindDataRef (DATA_REFS_XP[i])) == NULL )
        {
            // XP standard stuff must exist
            if (bEarly) {
                LOG_MSG(logFATAL,ERR_DATAREF_FIND,DATA_REFS_XP[i]);
                return false;
            }
            // other plugin's stuff is optional
            LOG_MSG(logDEBUG,ERR_DATAREF_FIND,DATA_REFS_XP[i]);
        }
    }
    
    // getting here means success
    return true;
}

// Register commands the plugin provides to XP
bool DataRefs::RegisterCommands()
{
    bool bRet = true;
    // loop over all data ref definitions
    for (int i=0; i < CNT_CMDREFS_SLA; i++)
    {
        // register command
        if ( (cmdSLA[i] =
              XPLMCreateCommand(CMD_REFS_SLA[i].cmdName,
                                CMD_REFS_SLA[i].cmdDescr)) == NULL )
        { LOG_MSG(logERR,ERR_CREATE_COMMAND,CMD_REFS_SLA[i].cmdName); bRet = false; }
    }
    return bRet;
}

//
// MARK: Configurations and Data Access
//

// return user's plane pos
positionTy DataRefs::GetUsersPlanePos() const
{
    using namespace std::chrono;
    positionTy pos
    (
     XPLMGetDatad(adrXP[DR_PLANE_LAT]),
     XPLMGetDatad(adrXP[DR_PLANE_LON]),
     XPLMGetDatad(adrXP[DR_PLANE_ELEV]),
     // system time in microseconds
     double(duration_cast<microseconds>(system_clock::now().time_since_epoch()).count())
     // divided by 1000000 to create seconds with fractionals
     / 1000000.0,
     XPLMGetDataf(adrXP[DR_PLANE_HEADING]),
     XPLMGetDataf(adrXP[DR_PLANE_PITCH]),
     XPLMGetDataf(adrXP[DR_PLANE_ROLL]),
     XPLMGetDatai(adrXP[DR_PLANE_ONGRND]) ? positionTy::GND_ON : positionTy::GND_OFF
     );
    
    // make invalid pos invalid
    if (pos.lat() < -75 || pos.lat() > 75)
    pos.lat() = NAN;
    
    return pos;
}

//
// MARK: Access to LiveTraffic
//

std::string DataRefs::GetLTStatusText () const
{
    if (!adrXP[DR_LT_AIRCRAFTS_DISPLAYED]) return LT_UNAVAILABLE;
    return IsLTActive() ? LT_ACTIVE : LT_INACTIVE;
}

bool DataRefs::IsLTActive () const
{
    return (// could we map the dataRefs at all?
            adrXP[DR_LT_AIRCRAFTS_DISPLAYED] &&
            // is LiveTraffic activated?
            XPLMGetDatai(adrXP[DR_LT_AIRCRAFTS_DISPLAYED]) != 0);
}

int DataRefs::GetLTBufPeriod () const
{
    return (// could we map the dataRefs at all?
            adrXP[DR_LT_FD_BUF_PERIOD] ?
            // if so then return LT's buffering period, otherweise 0
            XPLMGetDatai(adrXP[DR_LT_FD_BUF_PERIOD]) : 0);
}

//
// MARK: Actual Current Observations
//

// consider given COM only if configured to do so and check for selection
bool DataRefs::ConsiderCom(int idx) const
{
    return
    ShallActOnCom(idx) &&
    (!ShallRespectAudioSelect() || IsComSel(idx));
}

/// @return actual current audio desync period in seconds, >= 0
long DataRefs::GetDesyncPeriod () const
{
    int ret = GetManualDesync();
    if (ShallDesyncWithLTDelay() && IsLTActive())
        ret += GetLTBufPeriod();
    return ret > 0 ? ret : 0;
}



//
// MARK: Config File
//

bool DataRefs::LoadConfigFile()
{
    // which conversion to do with the (older) version of the config file?
    enum cfgFileConvE { CFG_NO_CONV=0 } conv = CFG_NO_CONV;
    
    // open a config file
    std::string sFileName (GetXPSystemPath() + PATH_CONFIG_FILE);
    std::ifstream fIn (sFileName);
    if (!fIn) {
        // if there is no config file just return...that's no problem, we use defaults
        if (errno == ENOENT)
            return true;
        
        // something else happened
		char sErr[SERR_LEN];
		strerror_s(sErr, sizeof(sErr), errno);
		LOG_MSG(logERR, ERR_CFG_FILE_OPEN_IN,
                sFileName.c_str(), sErr);
        return false;
    }
    
    // *** VERSION ***
    // first line is supposed to be the version - and we know of exactly one:
    // read entire line
    std::vector<std::string> ln;
    std::string lnBuf;
    if (!safeGetline(fIn, lnBuf) ||                     // read a line
        (ln = str_tokenize(lnBuf, " ")).size() != 2 ||  // split into two words
        ln[0] != SWITCH_LIVE_ATC)                          // 1. is SwitchLiveATC
    {
        LOG_MSG(logERR, ERR_CFG_FILE_VER, sFileName.c_str(), lnBuf.c_str());
        return false;
    }
    
    // 2. is version / test for only know current version
    if (ln[1] == SLA_CFG_VERSION)            conv = CFG_NO_CONV;
    else {
        SHOW_MSG(logERR, ERR_CFG_FILE_VER, sFileName.c_str(), lnBuf.c_str());
        return false;
    }
    
    // *** Config Entries ***
    // then follow the config entries
    // supposingly they are just 'name <space> value'
    int errCnt = 0;
    while (fIn && errCnt <= ERR_CFG_FILE_MAXWARN) {
        // read line and break into tokens, delimited by spaces
        safeGetline(fIn, lnBuf);
        
        // ignore empty lines
        if (lnBuf.empty())
            continue;
        LOG_MSG(logDEBUG, lnBuf.c_str());
        
        // otherwise should be 2 tokens
        ln = str_tokenize(lnBuf, " ");        
        if (ln.size() < 2) {
            // wrong number of words in that line
            LOG_MSG(logWARN,ERR_CFG_FILE_WORDS, sFileName.c_str(), lnBuf.c_str());
            errCnt++;
            continue;
        }
        
        // did read a name and a value?
        const std::string& sCfgName = ln[0];
        const std::string& sVal     = ln[1];
        long lVal = 0;      // try getting an integer value from it, if possible
        try { lVal = std::stol(sVal); }
        catch (...) {}
        const bool bVal = lVal != 0;
        const std::string sRestOfLine = lnBuf.substr(lnBuf.find(' ')+1);
        
        // assign values appropriately
        
        // toggle "act on COM#"
        for (int i = 0; i < COM_CNT; i++) {
            char buf[50];
            snprintf(buf,sizeof(buf),CFG_TOGGLE_COM,i+1);
            if (sCfgName == buf) {
                bActOnCom[i] = bVal;
                break;
            }
        }
        
        // other entries
             if (sCfgName == CFG_VLC_PATH)          VLCPath = sRestOfLine;
        else if (sCfgName == CFG_VLC_PARAMS)        VLCParams = sRestOfLine;
        else if (sCfgName == CFG_RESPECT_COM_SEL)   bRespectAudioSelect = bVal;
        else if (sCfgName == CFG_LT_DESYNC_BUF)     bDesyncLiveTrafficDelay = bVal;
        else if (sCfgName == CFG_DESYNC_MANUAL_ADJ) desyncManual = (int)lVal;
        else if (sCfgName == CFG_PREV_WHILE_DESYNC) bPrevFrequRunsTilDesync = bVal;
        else if (sCfgName == CFG_MAX_RADIO_DIST)    maxRadioDist = (int)lVal;
        else if (sCfgName == CFG_LOG_LEVEL)         iLogLevel = logLevelTy(lVal);
        else if (sCfgName == CFG_MSG_AREA_LEVEL)    iMsgAreaLevel = logLevelTy(lVal);
    }

    // problem was not just eof?
    if (!fIn && !fIn.eof()) {
        char sErr[SERR_LEN];
        strerror_s(sErr, sizeof(sErr), errno);
        LOG_MSG(logERR, ERR_CFG_FILE_READ,
                sFileName.c_str(), sErr);
        return false;
    }
    
    // close file
    fIn.close();

    // too many warnings?
    if (errCnt > ERR_CFG_FILE_MAXWARN) {
        LOG_MSG(logERR, ERR_CFG_FILE_READ,
                sFileName.c_str(), ERR_CFG_FILE_TOOMANY);
        return false;
    }
    
    // looks like success
    return true;
}

bool DataRefs::SaveConfigFile()
{
    // open an output config file
    std::string sFileName (GetXPSystemPath() + PATH_CONFIG_FILE);
    std::ofstream fOut (sFileName, std::ios_base::out | std::ios_base::trunc);
    if (!fOut) {
		char sErr[SERR_LEN];
		strerror_s(sErr, sizeof(sErr), errno);
		SHOW_MSG(logERR, ERR_CFG_FILE_OPEN_OUT,
                 sFileName.c_str(), sErr);
        return false;
    }
    
    // *** VERSION ***
    // save application and version first...maybe we need to know it in
    // future versions for conversion efforts - who knows?
    fOut << SWITCH_LIVE_ATC << ' ' << SLA_CFG_VERSION << '\n';
    
    // *** Config Entries ***
    
    // toggle "act on COM#"
    for (int i = 0; i < COM_CNT; i++) {
        char buf[50];
        snprintf(buf,sizeof(buf),CFG_TOGGLE_COM,i+1);
        fOut << buf << ' ' << bActOnCom[i] << '\n';
    }
    
    // other entries
    if (!VLCPath.empty())
        fOut << CFG_VLC_PATH        << ' ' << VLCPath                   << '\n';
    if (!VLCParams.empty())
        fOut << CFG_VLC_PARAMS      << ' ' << VLCParams                 << '\n';
    fOut << CFG_RESPECT_COM_SEL     << ' ' << bRespectAudioSelect       << '\n';
    fOut << CFG_LT_DESYNC_BUF       << ' ' << bDesyncLiveTrafficDelay   << '\n';
    fOut << CFG_DESYNC_MANUAL_ADJ   << ' ' << desyncManual              << '\n';
    fOut << CFG_PREV_WHILE_DESYNC   << ' ' << bPrevFrequRunsTilDesync   << '\n';
    fOut << CFG_MAX_RADIO_DIST      << ' ' << maxRadioDist              << '\n';
    fOut << CFG_LOG_LEVEL           << ' ' << iLogLevel                 << '\n';
    fOut << CFG_MSG_AREA_LEVEL      << ' ' << iMsgAreaLevel             << '\n';
    
    // some error checking towards the end
    if (!fOut) {
        char sErr[SERR_LEN];
        strerror_s(sErr, sizeof(sErr), errno);
        SHOW_MSG(logERR, ERR_CFG_FILE_WRITE,
                 sFileName.c_str(), sErr);
        fOut.close();
        return false;
    }

    // flush (which we explicitely did not do for each line for performance reasons) and close
    fOut.flush();
    fOut.close();
        
    return true;
}

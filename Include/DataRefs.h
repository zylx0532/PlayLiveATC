//
//  DataRefs.h
//  PlayLiveATC

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

#ifndef DataRefs_h
#define DataRefs_h

// in PlayLiveATC.xpp:
void MenuUpdateCheckmarks();

//
// MARK: DataRefs
//

// XP standard and LiveTraffic Datarefs being accessed
enum dataRefsXP_LT {
    // XP standard
    DR_XP_RADIO_COM1_FREQ = 0,          ///< sim/cockpit/radios/com1_freq_hz
    DR_XP_RADIO_COM2_FREQ,              ///< sim/cockpit/radios/com2_freq_hz
    DR_XP_RADIO_COM1_STANDBY_FREQ,      ///< sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833
    DR_XP_RADIO_COM2_STANDBY_FREQ,      ///< sim/cockpit2/radios/actuators/com2_standby_frequency_hz_833
    DR_XP_RADIO_COM1_SEL,               ///< sim/cockpit2/radios/actuators/audio_selection_com1
    DR_XP_RADIO_COM2_SEL,               ///< sim/cockpit2/radios/actuators/audio_selection_com2
    DR_PLANE_LAT,                       ///< user's plane's position
    DR_PLANE_LON,
    DR_PLANE_ELEV,
    DR_PLANE_PITCH,
    DR_PLANE_ROLL,
    DR_PLANE_HEADING,
    DR_PLANE_TRACK,
    DR_PLANE_TRUE_AIRSPEED,
    DR_PLANE_ONGRND,
    // X-Plane 11 only
    DR_XP_ATIS_ENABLED,                 ///< sim/atc/atis_enabled
    DR_VR_ENABLED,                      ///< sim/graphics/VR/enabled
    // LiveTraffic
    DR_LT_AIRCRAFTS_DISPLAYED,          ///< is LiveTraffic active?
    DR_LT_FD_BUF_PERIOD,                ///< LiveTraffic's buffering period
    
    /// always last, number of elements
    CNT_DATAREFS_XP
};

constexpr dataRefsXP_LT DR_FIRST_XP11_DR = DR_XP_ATIS_ENABLED;
constexpr dataRefsXP_LT DR_FIRST_LT_DR = DR_LT_AIRCRAFTS_DISPLAYED;

/// PlayLiveATC commands to be offered
enum cmdRefsPLA {
    CR_MONITOR_COM1 = 0,                ///< Monitor change of COM1
    CR_MONITOR_COM2,                    ///< Monitor change of COM2
    
    /// always last, number of elements
    CNT_CMDREFS_PLA
};

/// number of Frequencies to listen to
constexpr int COM_CNT = 2;


class DataRefs
{
//MARK: DataRefs
protected:
    XPLMDataRef adrXP[CNT_DATAREFS_XP];         ///< array of XP data refs to read from
public:
    XPLMCommandRef cmdPLA[CNT_CMDREFS_PLA];     ///< array of commands PLA offers
#ifdef DEBUG
    bool bSimVREntered = false;                 ///< for me to simulate some aspects of VR
#endif

//MARK: Provided Data, i.e. global variables
protected:
    XPLMPluginID pluginID       = 0;            ///< my own plugin id
    logLevelTy iLogLevel        = logWARN;      ///< logging level
    logLevelTy iMsgAreaLevel    = logINFO;      ///< message are level
    std::string XPSystemPath;                   ///< path to X-Plane's main directory
    std::string PluginPath;                     ///< path to plugin directory
    std::string DirSeparator;                   ///< directory separation character
    
    bool bActOnCom[COM_CNT] = {true,true};      ///< which frequency to act upon?
    bool bRespectAudioSelect = false;           ///< only play VLC stream for selected radio
#if !(IBM)
    std::string VLCPluginPath;                  ///< Path to VLC plugins
#endif
    std::string audioDev;                       ///< VLC audio output device id
    int iVolume = 100;                          ///< volume VLC play at (0-100)
    bool bMute = false;                         ///< temporarily muted? (not stored in config file)
    bool bDesyncLiveTrafficDelay = true;        ///< audio-desync with LiveTraffic's delay?
    int desyncManual = -10;                     ///< [s] (additional) manual audio-desync
    bool bPrevFrequRunsTilDesync = true;        ///< have the previous radio frequency continue till new one reaches desync period
    bool bPreBufferStandbyFrequ = true;         ///< Pre-buffer stand-by frequency once it has been changed
    bool bAtisPreferLiveATC = true;             ///< if playing a LiveATC-ATIS-stream suppress XP's output (XP11 only)
    int maxRadioDist = 300;                     ///< [nm] max distance a radio can be received
    
//MARK: Constructor
public:
    DataRefs ( logLevelTy initLogLevel );       ///< Constructor doesn't do much
    bool Init();                                ///< Early init DataRefs, return "OK?"
    bool LateInit();                            ///< Late init like other plugin's dataRefs
    void Stop();                                ///< unregister what's needed

    inline XPLMPluginID GetMyPluginId() const { return pluginID; }

protected:
    bool FindDataRefs (bool bEarly);
    bool RegisterCommands();

//MARK: DataRef access
public:
    // Log Level
    void SetLogLevel ( logLevelTy ll )  { iLogLevel = ll; }
    void SetMsgAreaLevel ( logLevelTy ll )  { iMsgAreaLevel = ll; }
    inline logLevelTy GetLogLevel()     { return iLogLevel; }
    inline logLevelTy GetMsgAreaLevel()         { return iMsgAreaLevel; }

    // Configuration
    inline bool ShallActOnCom(int idx) const { return 0<=idx&&idx<COM_CNT ? bActOnCom[idx] : false; }
    inline void SetActOnCom(int idx, bool bEnable) { if (0<=idx&&idx<COM_CNT) {bActOnCom[idx] = bEnable; MenuUpdateCheckmarks();} }
    void ToggleActOnCom(int idx) { SetActOnCom(idx,!ShallActOnCom(idx)); }
    inline bool ShallRespectAudioSelect() const { return bRespectAudioSelect; }
    void SetRespectAudioSelect (bool b) { bRespectAudioSelect = b; }
    
#if !(IBM)
    inline const std::string& GetVLCPath() const { return VLCPluginPath; }
    void SetVLCPath (const std::string newPath) { VLCPluginPath = newPath; }
#endif

    std::string GetAudioDev() const { return audioDev; }    ///< Audio Device ID
    void SetAudioDev(const std::string& dev) { audioDev = dev; }
    int GetVolume() const { return iVolume; }       ///< Volume
    void SetVolume(int iNewVolume);                 ///< sets new volume, also applies it to current playback
    bool IsMuted() const { return bMute; }          ///< Currently muted?
    void Mute(bool bDoMute = true);                 ///< (Un)Mute, also applies it to current playback
    
    /// Prefer LiveATC's ATIS over XP's ATIS
    bool PreferLiveATCAtis () const { return bAtisPreferLiveATC; }
    /// Set ATIS channel preference
    void SetPreferLiveATCAtis (bool bPrefLiveATC) { bAtisPreferLiveATC = bPrefLiveATC; }
    /// Tell XP our ATIS preference
    void EnableXPsATIS (bool bEnable);

    // Desync
    inline bool ShallDesyncWithLTDelay () const { return bDesyncLiveTrafficDelay; }
    void SetDesyncWithLTDelay (bool b) { bDesyncLiveTrafficDelay = b; }
    inline bool ShallRunPrevFrequTillDesync () const { return bPrevFrequRunsTilDesync; }
    void SetRunPrevFrequTillDesync (bool b) { bPrevFrequRunsTilDesync = b; }
    int GetManualDesync () const { return desyncManual; }
    void SetManualDesync (int i) { desyncManual = i; }
    
    /// Shall we pre-buffer the stand-by frequency?
    inline bool ShallPreBufferStandbyFrequ () const { return bPreBufferStandbyFrequ; }
    /// Set pre-buffering of stand-by frequency
    inline void SetPreBufferSTandbyFrequ (bool b) { bPreBufferStandbyFrequ = b; }
    
    // specific access
    inline int   GetComFreq(int idx) const  { return 0<=idx&&idx<COM_CNT ? XPLMGetDatai(adrXP[int(DR_XP_RADIO_COM1_FREQ)+idx]) : 0; }
    inline int   GetComStandbyFreq(int idx) const  { return 0<=idx&&idx<COM_CNT ? XPLMGetDatai(adrXP[int(DR_XP_RADIO_COM1_STANDBY_FREQ)+idx]) : 0; }
    inline int   IsComSel(int idx) const    { return 0<=idx&&idx<COM_CNT ? XPLMGetDatai(adrXP[int(DR_XP_RADIO_COM1_SEL)+idx]) : 0; }
    positionTy GetUsersPlanePos() const;
    int GetMaxRadioDist () const { return maxRadioDist; }
    void SetMaxRadioDist (int i) { maxRadioDist = i; }
    
    inline bool  IsVREnabled() const            { return
#ifdef DEBUG
        bSimVREntered ? true :                  // simulate some aspects of VR
#endif
        adrXP[DR_VR_ENABLED] ? XPLMGetDatai(adrXP[DR_VR_ENABLED]) != 0 : false; }  // for XP10 compatibility we accept not having this dataRef

    // LiveTraffic specifics
    std::string GetLTStatusText () const;
    bool IsLTActive () const;
    int GetLTBufPeriod () const;

    /// Should this COM channel be muted because not active?
    bool ShallMuteCom(int idx) const;
    long GetDesyncPeriod () const;      ///< audio desync time in seconds
    
    // Get XP System Path
    inline std::string GetXPSystemPath() const  { return XPSystemPath; }
    inline std::string GetPluginPath()   const  { return PluginPath; }
    inline std::string GetDirSeparator() const  { return DirSeparator; }
    
    // Load/save config file
    bool LoadConfigFile();
    bool SaveConfigFile();
};

#endif /* DataRefs_h */

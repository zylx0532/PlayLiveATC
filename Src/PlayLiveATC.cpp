//
//  Switch Live ATC Plugin
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

#include "PlayLiveATC.h"

#if IBM
#include <objbase.h>        // for CoInitializeEx
#endif

 //
// MARK: Globals
//

// DataRefs, Settings, and other global stuff
DataRefs dataRefs(VERSION_BETA ? logDEBUG : logWARN);
// Settings Dialog
LTSettingsUI settingsUI;

//
// MARK: Menu
//

enum menuItems {
    MENU_ID_PlayLiveATC = 0,
    MENU_ID_TOGGLE_COM1,
    MENU_ID_TOGGLE_COM2,
    MENU_ID_VOLUME_UP,
    MENU_ID_VOLUME_DOWN,
    MENU_ID_MUTE,
    MENU_ID_SUB_AUDIO_DEVICE,
    MENU_ID_SETTINGS_UI,
    MENU_ID_HELP,
#ifdef DEBUG
    MENU_ID_RELOAD_PLUGINS,
#endif
    CNT_MENU_ID                     // always last, number of elements
};

// ID of the "PlayLiveATC" menu within the plugin menu; items numbers of its menu items
XPLMMenuID menuID = 0;
int aMenuItems[CNT_MENU_ID];

/// ID of the "Output Device" submenu within the PlayLiveATC menu
XPLMMenuID menuIDOutputDev = 0;

// set checkmarks according to current settings
void MenuUpdateCheckmarks()
{
    XPLMCheckMenuItem(menuID, aMenuItems[MENU_ID_TOGGLE_COM1],
        dataRefs.ShallActOnCom(0) ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    XPLMCheckMenuItem(menuID, aMenuItems[MENU_ID_TOGGLE_COM2],
        dataRefs.ShallActOnCom(1) ? xplm_Menu_Checked : xplm_Menu_Unchecked);

    // checkmarks for the audio device selection
    for (int i = 0;
        i < gVLCOutputDevs.size();
        i++)
    {
        // place a check mark if this is the current device
        XPLMCheckMenuItem(menuIDOutputDev, i,
            dataRefs.GetAudioDev() == gVLCOutputDevs[i].device() ? xplm_Menu_Checked : xplm_Menu_NoCheck);
    }
}

//
// MARK: Audio Device menu
//

/// Menu handler for audio device selection
/// @param iRef Is actually a pointer to a string with the device id
void MenuHandlerAudioDevices(void * /*mRef*/, void * iRef)
{
    if (iRef) {
        const char* devId = reinterpret_cast<const char*>(iRef);
        dataRefs.SetAudioDev(devId);
        COMChannel::SetAllAudioDevice(devId);
        MenuUpdateCheckmarks();
    }
}

/// (Re)creates the sub menu for selection of the audio output device
bool MenuAudioDevices()
{
    // update the list of audio devices
    COMChannel::UpdateVLCOutputDevices();

    // if the submenu doesn't exist yet create it
    if (!menuIDOutputDev) {
        aMenuItems[MENU_ID_SUB_AUDIO_DEVICE] =
            XPLMAppendMenuItem(menuID, MENU_AUDIO_DEVICE, NULL, 1);
        menuIDOutputDev = XPLMCreateMenu(MENU_AUDIO_DEVICE, menuID,
            aMenuItems[MENU_ID_SUB_AUDIO_DEVICE], MenuHandlerAudioDevices, NULL);
        if (!menuIDOutputDev) { LOG_MSG(logERR, ERR_CREATE_MENU, MENU_AUDIO_DEVICE); return false; }
    }
    else {
        // remove any existing menu items
        XPLMClearAllMenuItems(menuIDOutputDev);
    }

    // add one menu item per available output device
    // using the description as item name and the device id as item reference
    if (gVLCOutputDevs.empty()) {
        XPLMAppendMenuItem(menuIDOutputDev, MENU_NO_DEVICE, NULL, 1);
    }
    else {
        for (const VLC::AudioOutputDeviceDescription& dev : gVLCOutputDevs) {
            // add a menu item for the device
            XPLMAppendMenuItem(menuIDOutputDev,
                dev.description().c_str(),
                (void*)dev.device().c_str(), 1);
        }
    }

    // set the checkmarks
    MenuUpdateCheckmarks();

    return true;
}

//
// MARK: PlayLiveATC menu
//

// Callback called by XP, so this is an entry point into the plugin
void MenuHandlerCB(void * /*mRef*/, void * iRef)
{
    // top level exception handling
    try {
        // act based on menu id
        switch (reinterpret_cast<unsigned long long>(iRef)) {
        case MENU_ID_TOGGLE_COM1:
            dataRefs.ToggleActOnCom(0);
            break;
        case MENU_ID_TOGGLE_COM2:
            dataRefs.ToggleActOnCom(1);
            break;
        case MENU_ID_SETTINGS_UI:
            settingsUI.Show();
            settingsUI.Center();
            break;
        case MENU_ID_HELP:
            OpenURL(HELP_URL);
            break;
#ifdef DEBUG
        case MENU_ID_RELOAD_PLUGINS:
            XPLMReloadPlugins();
            break;
#endif
        }
    }
    catch (const std::exception& e) {
        LOG_MSG(logERR, ERR_TOP_LEVEL_EXCEPTION, e.what());
        // otherwise ignore
    }
    catch (...) {
        // ignore
    }
}

// register my menu items for the plugin during init
bool MenuRegisterItems ()
{
    // clear menu array
    memset(aMenuItems, 0, sizeof(aMenuItems));
    
    // Create menu item and menu
    aMenuItems[MENU_ID_PlayLiveATC] = XPLMAppendMenuItem(XPLMFindPluginsMenu(), SWITCH_LIVE_ATC, NULL, 1);
    menuID = XPLMCreateMenu(SWITCH_LIVE_ATC, XPLMFindPluginsMenu(), aMenuItems[MENU_ID_PlayLiveATC], MenuHandlerCB, NULL);
    if ( !menuID ) { LOG_MSG(logERR,ERR_CREATE_MENU, SWITCH_LIVE_ATC); return false; }
    
    // Toggle acting upon change of which COM frequency?
    aMenuItems[MENU_ID_TOGGLE_COM1] =
    LT_AppendMenuItem(menuID, MENU_TOGGLE_COM1, (void *)MENU_ID_TOGGLE_COM1,
                      dataRefs.cmdPLA[CR_MONITOR_COM1]);
    aMenuItems[MENU_ID_TOGGLE_COM2] =
    LT_AppendMenuItem(menuID, MENU_TOGGLE_COM2, (void *)MENU_ID_TOGGLE_COM2,
                      dataRefs.cmdPLA[CR_MONITOR_COM2]);

    // Audio Device sub menu
    MenuAudioDevices();
    
    // Show Settings UI
    aMenuItems[MENU_ID_SETTINGS_UI] =
    XPLMAppendMenuItem(menuID, MENU_SETTINGS_UI, (void *)MENU_ID_SETTINGS_UI,1);
    
    // Help
    aMenuItems[MENU_ID_HELP] =
    XPLMAppendMenuItem(menuID, MENU_HELP, (void *)MENU_ID_HELP,1);
    
#ifdef DEBUG
    // Separator
    XPLMAppendMenuSeparator(menuID);
    
    // Reload Plugins
    aMenuItems[MENU_ID_RELOAD_PLUGINS] =
    XPLMAppendMenuItem(menuID, MENU_RELOAD_PLUGINS, (void *)MENU_ID_RELOAD_PLUGINS,1);
#endif
    
    // check for errors
    for (int item: aMenuItems) {
        if ( item<0 ) {
            LOG_MSG(logERR,ERR_APPEND_MENU_ITEM);
            return false;
        }
    }

    // update the checkmarks so that selected options are marked
    MenuUpdateCheckmarks();

    // Success
    LOG_MSG(logDEBUG,DBG_MENU_CREATED)
    return true;
}

//
// MARK: Commands
//

// this one is for commands that correspond directly to menu commands
// they just call the menu...
// (that is kinda contrary to what XPLMAppendMenuItemWithCommand intends to do...but the menu handler is there anyway and has even more)

struct cmdMenuMap {
    cmdRefsPLA cmd;
    menuItems menu;
} CMD_MENU_MAP[] = {
    { CR_MONITOR_COM1, MENU_ID_TOGGLE_COM1 },
    { CR_MONITOR_COM2, MENU_ID_TOGGLE_COM2 },
};

int CommandHandlerMenuItems (XPLMCommandRef       /*inCommand*/,
                             XPLMCommandPhase     inPhase,
                             void *               inRefcon) // contains menuItems
{
    if (inPhase == xplm_CommandBegin)
        MenuHandlerCB(NULL, inRefcon);
    return 1;
}

bool RegisterCommandHandlers ()
{
    for (cmdMenuMap i: CMD_MENU_MAP)
        XPLMRegisterCommandHandler(dataRefs.cmdPLA[i.cmd],
                                   CommandHandlerMenuItems,
                                   1, (void*)i.menu);
    return true;
}

//
// MARK: Flight loop callbacks
//

// callback called every second to identify COM frequency changes
float PLAFlightLoopCB (float, float, int, void*)
{
    
    // loop over all COM radios we support
    for (COMChannel& chn: gChn) {
        const int idx = chn.GetIdx();
        // should we actually _act_ on that COM radio?
        if (dataRefs.ShallActOnCom(idx)) {
            // yes, consider this COM radio
            chn.RegularMaintenance();
        } else {
            // do _not_ consider this COM
            // stop if it is running
            if (chn.IsDefined())
                chn.ClearChannel();
        }
        
    }
    
    // if there is no ATIS channel playing then make sure XP can play ATIS
    if (!COMChannel::AnyATISPlaying())
        dataRefs.EnableXPsATIS(true);

    // every minute update the list of audio output devices
    static int callCount = 0;
    if (++callCount >= 60) {
        callCount = 0;
        MenuAudioDevices();
    }
    
    // call me again in a second
    return 1.0f;
}


// a one-time callback for initialization stuff after all plugins loaded and initialized
float PLAOneTimeCB (float, float, int, void*)
{
    // fetch dataRefs we might need
    if (dataRefs.LateInit()) {
        // start the regular callback
        XPLMRegisterFlightLoopCallback(PLAFlightLoopCB, PLA_STARTUP_DELAY, NULL);
    }
    
    // don't call me again
    return 0;
}

//
// MARK: Plugin Main Functions
//

PLUGIN_API int XPluginStart(
							char *		outName,
							char *		outSig,
							char *		outDesc)
{
    // tell X-Plane who we are
	strcpy_s(outName, 255, SWITCH_LIVE_ATC);
	strcpy_s(outSig,  255, PLUGIN_SIGNATURE);
	strcpy_s(outDesc, 255, PLUGIN_DESCRIPTION);
    
    // use native paths, i.e. Posix style (as opposed to HFS style)
    // https://developer.x-plane.com/2014/12/mac-plugin-developers-you-should-be-using-native-paths/
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS",1);
    
    // init our version number
    // (also outputs the "PlayLiveATC ... starting up" log message)
    if (!InitFullVersion ()) { DestroyWindow(); return 0; }
    
    // init DataRefs
    if (!dataRefs.Init()) { DestroyWindow(); return 0; }
    
    // register commands
    if (!RegisterCommandHandlers()) { DestroyWindow(); return 0; }
    
    // create menu
    if (!MenuRegisterItems()) { DestroyWindow(); return 0; }
    
#if IBM
    // Windows: Recommended before calling ShellExecuteA
    // https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-shellexecutea
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#endif
    
    // global init libcurl
    CURLcode resCurl = curl_global_init (CURL_GLOBAL_ALL);
    if ( resCurl != CURLE_OK )
    {
        // something went wrong
        LOG_MSG(logFATAL,ERR_CURL_INIT,curl_easy_strerror(resCurl));
        DestroyWindow();
        return 0;
    }
    
    // Success
    return 1;
}

PLUGIN_API void	XPluginStop(void)
{
    // Cleanup dataRef registration
    dataRefs.Stop();
    // Save config
    dataRefs.SaveConfigFile();

    DestroyWindow();
}

// Registers a one-time callback, which will be called once all of XP
// is set up and running, i.e. we actually see scenery.
// This is safe for registering other plugin's dataRefs and actually start working.
PLUGIN_API int  XPluginEnable(void)
{
    // Warn about beta and debug versions
    if constexpr (VERSION_BETA)
        SHOW_MSG(logWARN, BETA_LIMITED_VERSION, PLA_BETA_VER_LIMIT_TXT);
#ifdef DEBUG
    SHOW_MSG(logWARN, DBG_DEBUG_BUILD);
#endif
    
    // Initialize all VLC instances
    COMChannel::InitAllVLC();

    // Set initial audio output device as read from configuration
    MenuAudioDevices();
    COMChannel::SetAllAudioDevice(dataRefs.GetAudioDev());
    
    // start the actual processing
    XPLMRegisterFlightLoopCallback(PLAOneTimeCB, -1, NULL);
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    // make sure all children are stopped and cleanp up
    COMChannel::CleanupAllVLC();
    
    // cleanup
    XPLMUnregisterFlightLoopCallback(PLAOneTimeCB, NULL);
    XPLMUnregisterFlightLoopCallback(PLAFlightLoopCB, NULL);
    LOG_MSG(logMSG, MSG_DISABLED);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam)
{
    // we only process msgs from X-Plane
    if (inFrom != XPLM_PLUGIN_XPLANE)
        return;
    
#ifdef DEBUG
    // for me not having a VR rig I do some basic testing with sending XPLM_MSG_AIRPLANE_COUNT_CHANGED
    if (inMsg == XPLM_MSG_AIRPLANE_COUNT_CHANGED) {
        dataRefs.bSimVREntered = !dataRefs.bSimVREntered;
        inMsg = dataRefs.bSimVREntered ? XPLM_MSG_ENTERED_VR : XPLM_MSG_EXITING_VR;
    }
#endif
    
    switch (inMsg) {
            // *** entering VR mode ***
        case XPLM_MSG_ENTERED_VR:
            // move eligible windows into VR
            break;
            
            // *** existing from VR mode ***
        case XPLM_MSG_EXITING_VR:
            // move eligible windows out of VR
            break;
    }
    
}


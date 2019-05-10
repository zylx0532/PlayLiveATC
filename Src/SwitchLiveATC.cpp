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

#include "SwitchLiveATC.h"

#if IBM
#include <objbase.h>        // for CoInitializeEx
#endif

 //
// MARK: Globals
//

// DataRefs, Settings, and other global stuff
DataRefs dataRefs(logWARN);
// Settings Dialog
LTSettingsUI settingsUI;

//
// MARK: Menu
//

enum menuItems {
    MENU_ID_SWITCHLIVEATC = 0,
    MENU_ID_TOGGLE_COM1,
    MENU_ID_TOGGLE_COM2,
    MENU_ID_SETTINGS_UI,
    MENU_ID_HELP,
#ifdef DEBUG
    MENU_ID_RELOAD_PLUGINS,
#endif
    CNT_MENU_ID                     // always last, number of elements
};

// ID of the "SwitchLiveATC" menu within the plugin menu; items numbers of its menu items
XPLMMenuID menuID = 0;
int aMenuItems[CNT_MENU_ID];

// Callback called by XP, so this is an entry point into the plugin
void MenuHandlerCB(void * /*mRef*/, void * iRef)
{
    // LiveTraffic top level exception handling
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
    } catch (const std::exception& e) {
        LOG_MSG(logERR, ERR_TOP_LEVEL_EXCEPTION, e.what());
        // otherwise ignore
    } catch (...) {
        // ignore
    }
}

// set checkmarks according to current settings
void MenuUpdateCheckmarks()
{
    XPLMCheckMenuItem(menuID,aMenuItems[MENU_ID_TOGGLE_COM1],
                      dataRefs.ShallActOnCom(0) ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    XPLMCheckMenuItem(menuID,aMenuItems[MENU_ID_TOGGLE_COM2],
                      dataRefs.ShallActOnCom(1) ? xplm_Menu_Checked : xplm_Menu_Unchecked);
}

// register my menu items for the plugin during init
bool MenuRegisterItems ()
{
    // clear menu array
    memset(aMenuItems, 0, sizeof(aMenuItems));
    
    // Create menu item and menu
    aMenuItems[MENU_ID_SWITCHLIVEATC] = XPLMAppendMenuItem(XPLMFindPluginsMenu(), SWITCH_LIVE_ATC, NULL, 1);
    menuID = XPLMCreateMenu(SWITCH_LIVE_ATC, XPLMFindPluginsMenu(), aMenuItems[MENU_ID_SWITCHLIVEATC], MenuHandlerCB, NULL);
    if ( !menuID ) { LOG_MSG(logERR,ERR_CREATE_MENU,MENU_ID_SWITCHLIVEATC); return false; }
    
    // Toggle acting upon change of which COM frequency?
    aMenuItems[MENU_ID_TOGGLE_COM1] =
    XPLMAppendMenuItem(menuID, MENU_TOGGLE_COM1, (void *)MENU_ID_TOGGLE_COM1,1);
    aMenuItems[MENU_ID_TOGGLE_COM2] =
    XPLMAppendMenuItem(menuID, MENU_TOGGLE_COM2, (void *)MENU_ID_TOGGLE_COM2,1);
    MenuUpdateCheckmarks();
    
    // Show Settings UI
    aMenuItems[MENU_ID_SETTINGS_UI] =
    XPLMAppendMenuItem(menuID, MENU_SETTINGS_UI, (void *)MENU_ID_SETTINGS_UI,1);
    
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
    cmdRefsSLA cmd;
    menuItems menu;
} CMD_MENU_MAP[] = {
    { CR_TOGGLE_ACT_COM1, MENU_ID_TOGGLE_COM1 },
    { CR_TOGGLE_ACT_COM2, MENU_ID_TOGGLE_COM2 },
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
        XPLMRegisterCommandHandler(dataRefs.cmdSLA[i.cmd],
                                   CommandHandlerMenuItems,
                                   1, (void*)i.menu);
    return true;
}

//
// MARK: Flight loop callbacks
//

struct ComFrequTy {
protected:
    int frequ = 0;             // stable, currently tuned frequncy [Hz as returned by XP]
    std::string frequString;   // frequncy as string in format ###.###
    
public:
    int getFrequ() const                   { return frequ; }
    std::string getFrequString() const     { return frequString; }
    
    // if current differs from stable
    // THEN we consider it a change to a new stable frequency
    bool doChange(int _curr)
    {
        if (_curr != frequ) {
            char buf[20];
            frequ = _curr;
            snprintf(buf, sizeof(buf), "%d.%03d", frequ / 1000, frequ % 1000);
            frequString = buf;
            return true;
        } else {
            return false;
        }
    }
    
} gFrequ[COM_CNT];

// callback called every second to identify COM frequency changes
float SLAFlightLoopCB (float, float, int, void*)
{
    // loop over all COM radios we support
    for (int idx = 0; idx < COM_CNT; idx++) {
        // get current frequency and check if this is considered a change
        if (gFrequ[idx].doChange(dataRefs.GetComFreq(idx)) &&
            // and shall we actually _act_ on that change?
            dataRefs.ShallActOnCom(idx))
        {
            SHOW_MSG(logINFO, MSG_COM_CHANGE_DETECT, idx+1,
                     gFrequ[idx].getFrequString().c_str());
            RVPlayStream(gFrequ[idx].getFrequString() == "118.000" ?
                         "https://www.liveatc.net/play/kjfk_twr.pls" :
                         "https://www.liveatc.net/play/ksfo_twr.pls");
        }
            
    }
    
    // call me again in s scond
    return 1.0f;
}


// a one-time callback for initialization stuff after all plugins loaded and initialized
float SLAOneTimeCB (float, float, int, void*)
{
    // fetch dataRefs we might need
    if (dataRefs.LateInit()) {
        // start the regular callback
        XPLMRegisterFlightLoopCallback(SLAFlightLoopCB, 1, NULL);
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
    // (also outputs the "LiveTraffic ... starting up" log message)
    if (!InitFullVersion ()) { DestroyWindow(); return 0; }
    
    // init DataRefs
    if (!dataRefs.Init()) { DestroyWindow(); return 0; }
    
    // create menu
    if (!MenuRegisterItems()) { DestroyWindow(); return 0; }
    
    // create Commands
    RegisterCommandHandlers();
    
#if IBM
    // Windows: Recommended before calling ShellExecuteA
    // https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-shellexecutea
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#endif
    
    // Success
    return 1;
}

PLUGIN_API void	XPluginStop(void)
{
    // Cleanup dataRef registration
    dataRefs.Stop();
    DestroyWindow();
}

// Registers a one-time callback, which will be called once all of XP
// is set up and running, i.e. we actually see scenery.
// This is safe for registering other plugin's dataRefs and actually start working.
PLUGIN_API int  XPluginEnable(void)
{
    // Warn about beta and debug versions
    if constexpr (VERSION_BETA)
        SHOW_MSG(logWARN, BETA_LIMITED_VERSION, SLA_BETA_VER_LIMIT_TXT);
#ifdef DEBUG
    SHOW_MSG(logWARN, DBG_DEBUG_BUILD);
#endif
    
    // start the actual processing
    XPLMRegisterFlightLoopCallback(SLAOneTimeCB, -1, NULL);
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    // make sure all children are stopped
    RVStopAll();
    
    // cleanup
    XPLMUnregisterFlightLoopCallback(SLAOneTimeCB, NULL);
    XPLMUnregisterFlightLoopCallback(SLAFlightLoopCB, NULL);
    LOG_MSG(logMSG, MSG_DISABLED);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam)
{ }


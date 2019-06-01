//
//  SettingsUI.cpp
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

#include "PlayLiveATC.h"

//
//MARK: LTSettingsUI
//

LTSettingsUI::LTSettingsUI () :
widgetIds(nullptr)
{}


LTSettingsUI::~LTSettingsUI()
{
    // just in case...
    Disable();
}

//
//MARK: Window Structure
// Basics | Advanced
//

// indexes into the below definition array, must be kept in synch with the same
enum UI_WIDGET_IDX_T {
    UI_MAIN_WND     = 0,
    // Buttons to select 'tabs'
    UI_BTN_BASICS,
    UI_BTN_ADVANCED,
    
    // [?] Help
    UI_BTN_HELP,
    
    // "Basics" tab
    UI_BASICS_SUB_WND,
    UI_BASICS_BTN_COM1,
    UI_BASICS_CAP_COM1_STATUS,
    UI_BASICS_BTN_COM2,
    UI_BASICS_CAP_COM2_STATUS,
    UI_BASICS_BTN_PLAY_IF_SELECTED,
    
    UI_BASICS_CAP_LIVE_TRAFFIC,
    UI_BASICS_BTN_LT_USE_BUF_PERIOD,
    UI_BASICS_BTN_KEEP_PREV_WHILE_DESYNC,
    UI_BASICS_CAP_DESYNC_ADJUST,
    UI_BASICS_TXT_DESYNC_ADJUST,
    
    UI_BASICS_CAP_VERSION_TXT,
    UI_BASICS_CAP_VERSION,
    UI_BASICS_CAP_BETA_LIMIT,

    // "Advanced" tab
    UI_ADVCD_SUB_WND,
    UI_ADVCD_CAP_LOGLEVEL,
    UI_ADVCD_BTN_LOG_FATAL,
    UI_ADVCD_BTN_LOG_ERROR,
    UI_ADVCD_BTN_LOG_WARNING,
    UI_ADVCD_BTN_LOG_INFO,
    UI_ADVCD_BTN_LOG_DEBUG,
    UI_ADVCD_CAP_MSGAREA_LEVEL,
    UI_ADVCD_BTN_MSGAREA_FATAL,
    UI_ADVCD_BTN_MSGAREA_ERROR,
    UI_ADVCD_BTN_MSGAREA_WARNING,
    UI_ADVCD_BTN_MSGAREA_INFO,

    UI_ADVCD_CAP_PATH_TO_VLC,
    UI_ADVCD_TXT_PATH_TO_VLC,
    UI_ADVCD_CAP_VALIDATE_PATH,
    
    UI_ADVCD_CAP_VLC_PARAMS,
    UI_ADVCD_TXT_VLC_PARAMS,
    
    UI_ADVCD_CAP_MAX_RADIO_DIST,
    UI_ADVCD_TXT_MAX_RADIO_DIST,

    // always last: number of UI elements
    UI_NUMBER_OF_ELEMENTS
};

// for ease of definition coordinates start at (0|0)
// window will be centered shortly before presenting it
TFWidgetCreate_t SETTINGS_UI[] =
{
    {   0,   0, 400, 330, 0, "PlayLiveATC Settings", 1, NO_PARENT, xpWidgetClass_MainWindow, {xpProperty_MainWindowHasCloseBoxes, 1, xpProperty_MainWindowType,xpMainWindowStyle_Translucent,0,0} },
    // Buttons to select 'tabs'
    {  10,  30,  65,  10, 1, "Basics",              0, UI_MAIN_WND, xpWidgetClass_Button, {xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0, 0,0} },
    {  75,  30,  65,  10, 1, "Advanced",            0, UI_MAIN_WND, xpWidgetClass_Button, {xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0, 0,0} },
    // Push button for help
    { 360,  30,  30,  10, 1, "?",                   0, UI_MAIN_WND, xpWidgetClass_Button, {xpProperty_ButtonBehavior, xpButtonBehaviorPushButton,  0,0, 0,0} },
    // "Basics" tab
    {  10,  50, -10, -10, 0, "Basics",              0, UI_MAIN_WND, xpWidgetClass_SubWindow, {0,0, 0,0, 0,0} },
    {  10,  10,  10,  10, 1, "Watch COM1 frequency",0, UI_BASICS_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox, 0,0} },
    { 170,   8,  -5,  10, 1, "",                    0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  10,  25,  10,  10, 1, "Watch COM2 frequency",0, UI_BASICS_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox, 0,0} },
    { 170,  23,  -5,  10, 1, "",                    0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  10,  50,  10,  10, 1, "Play only if COM radio is selected",0, UI_BASICS_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox, 0,0} },

    {   5, 100,  -5,  10, 1, "LiveTraffic integration (%s):",0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  10, 120,  10,  10, 1, "Delay audio by LiveTraffic's buffering period",0, UI_BASICS_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox, 0,0} },
    {  10, 135,  10,  10, 1, "Continue previous frequ. while waiting for buffering",0, UI_BASICS_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox, 0,0} },
    {   5, 150, 195,  10, 1, "Audio delay adjustment:",0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    { 200, 150,  50,  15, 1, "",                    0, UI_BASICS_SUB_WND, xpWidgetClass_TextField, {xpProperty_MaxCharacters,4, 0,0, 0,0} },

    {   5, -15,  -5,  10, 1, "Version",             0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  50, -15, 200,  10, 1, "",                    0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    { 200, -15,  -5,  10, 1, "",                    0, UI_BASICS_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    // "Advanced" tab
    {  10,  50, -10, -10, 0, "Advanced",            0, UI_MAIN_WND, xpWidgetClass_SubWindow, {0,0,0,0,0,0} },
    {   5,  10,  -5,  10, 1, "Log Level:",          0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  80,  10,  10,  10, 1, "Fatal",               0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 140,  10,  10,  10, 1, "Error",               0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 200,  10,  10,  10, 1, "Warning",             0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 270,  10,  10,  10, 1, "Info",                0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 320,  10,  10,  10, 1, "Debug",               0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    {   5,  30,  -5,  10, 1, "Msg Area:",           0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  80,  30,  10,  10, 1, "Fatal",               0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 140,  30,  10,  10, 1, "Error",               0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 200,  30,  10,  10, 1, "Warning",             0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    { 270,  30,  10,  10, 1, "Info",                0, UI_ADVCD_SUB_WND, xpWidgetClass_Button, {xpProperty_ButtonType, xpRadioButton, xpProperty_ButtonBehavior, xpButtonBehaviorRadioButton, 0,0} },
    
    {   5,  60,  -5,  10, 1, "VLC plugins path:",   0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  10,  75,  -5,  15, 1, "",                    0, UI_ADVCD_SUB_WND, xpWidgetClass_TextField, {0,0, 0,0, 0,0} },
    {   5,  90,  -5,  10, 1, "",                    0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    
    {   5, 110,  -5,  10, 1, "VLC Parameters (leave empty to return to defaults):",0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    {  10, 125,  -5,  15, 1, "",                    0, UI_ADVCD_SUB_WND, xpWidgetClass_TextField, {0,0, 0,0, 0,0} },

    {   5, 155, 195,  10, 1, "Max radio distance: [nm]", 0, UI_ADVCD_SUB_WND, xpWidgetClass_Caption, {0,0, 0,0, 0,0} },
    { 200, 155,  50,  15, 1, "",                    0, UI_ADVCD_SUB_WND, xpWidgetClass_TextField, {xpProperty_MaxCharacters,3, 0,0, 0,0} },
};

constexpr int NUM_WIDGETS = sizeof(SETTINGS_UI)/sizeof(SETTINGS_UI[0]);

static_assert(UI_NUMBER_OF_ELEMENTS == NUM_WIDGETS,
              "UI_WIDGET_IDX_T and SETTINGS_UI[] differ in number of elements!");

// creates the main window and all its widgets based on the above
// definition array
void LTSettingsUI::Enable()
{
    if (!isEnabled()) {
        // array, which receives ids of all created widgets
        widgetIds = new XPWidgetID[NUM_WIDGETS];
        LOG_ASSERT(widgetIds);
        memset(widgetIds, 0, sizeof(XPWidgetID)*NUM_WIDGETS );

        // create all widgets, i.e. the entire window structure, but keep invisible
        if (!TFUCreateWidgetsEx(SETTINGS_UI, NUM_WIDGETS, NULL, widgetIds))
        {
            SHOW_MSG(logERR,ERR_WIDGET_CREATE);
            return;
        }
        setId(widgetIds[0]);        // register in base class for message handling
        
        // some widgets with objects
        subBasics.setId(widgetIds[UI_BASICS_SUB_WND]);
        subAdvcd.setId(widgetIds[UI_ADVCD_SUB_WND]);

        // organise the tab button group
        tabGrp.Add({
            widgetIds[UI_BTN_BASICS],
            widgetIds[UI_BTN_ADVANCED],
        });
        tabGrp.SetChecked(widgetIds[UI_BTN_BASICS]);
        HookButtonGroup(tabGrp);
        
        // *** Basic ***
        btnBasicsCom[0].setId(widgetIds[UI_BASICS_BTN_COM1]);
        btnBasicsCom[1].setId(widgetIds[UI_BASICS_BTN_COM2]);
        btnPlayIfSelected.setId(widgetIds[UI_BASICS_BTN_PLAY_IF_SELECTED]);
        btnPlayIfSelected.SetChecked(dataRefs.ShallRespectAudioSelect());
        
        // LiveTraffic integration / audio desync
        capLTIntegration.setId(widgetIds[UI_BASICS_CAP_LIVE_TRAFFIC]);
        capLTIntegFormatStr = capLTIntegration.GetDescriptor();
        btnLTUseBufPeriod.setId(widgetIds[UI_BASICS_BTN_LT_USE_BUF_PERIOD]);
        btnLTUseBufPeriod.SetChecked(dataRefs.ShallDesyncWithLTDelay());
        btnKeepPrevWhileDesync.setId(widgetIds[UI_BASICS_BTN_KEEP_PREV_WHILE_DESYNC]);
        btnKeepPrevWhileDesync.SetChecked(dataRefs.ShallRunPrevFrequTillDesync());
        txtDesyncAdjust.setId(widgetIds[UI_BASICS_TXT_DESYNC_ADJUST]);
        txtDesyncAdjust.tfFormat = TFTextFieldWidget::TFF_NEG_DIGITS;
        txtDesyncAdjust.SetDescriptor(dataRefs.GetManualDesync());
        
        // COM1/2 status
        capCOM1Status.setId(widgetIds[UI_BASICS_CAP_COM1_STATUS]);
        capCOM2Status.setId(widgetIds[UI_BASICS_CAP_COM2_STATUS]);

        // version number
        XPSetWidgetDescriptor(widgetIds[UI_BASICS_CAP_VERSION],
                              PLA_VERSION_FULL);
        if constexpr (VERSION_BETA)
        {
            char betaLimit[100];
            snprintf(betaLimit,sizeof(betaLimit), BETA_LIMITED_VERSION,PLA_BETA_VER_LIMIT_TXT);
            XPSetWidgetDescriptor(widgetIds[UI_BASICS_CAP_BETA_LIMIT],
                                  betaLimit);
        }
        
        // *** Advanced ***
        logLevelGrp.Add({
            widgetIds[UI_ADVCD_BTN_LOG_DEBUG],      // index 0 equals logDEBUG, which is also 0
            widgetIds[UI_ADVCD_BTN_LOG_INFO],       // ...
            widgetIds[UI_ADVCD_BTN_LOG_WARNING],
            widgetIds[UI_ADVCD_BTN_LOG_ERROR],
            widgetIds[UI_ADVCD_BTN_LOG_FATAL],      // index 4 equals logFATAL, which is also 4
        });
        HookButtonGroup(logLevelGrp);
        
        msgAreaLevelGrp.Add({
            widgetIds[UI_ADVCD_BTN_MSGAREA_INFO],       // index 0 is logINFO, which is 1
            widgetIds[UI_ADVCD_BTN_MSGAREA_WARNING],
            widgetIds[UI_ADVCD_BTN_MSGAREA_ERROR],
            widgetIds[UI_ADVCD_BTN_MSGAREA_FATAL],      // index 4 equals logFATAL, which is also 4
        });
        HookButtonGroup(msgAreaLevelGrp);
        
        txtPathToVLC.setId(widgetIds[UI_ADVCD_TXT_PATH_TO_VLC]);
        txtPathToVLC.SetDescriptor(dataRefs.GetVLCPath());
        capValidatePath.setId(widgetIds[UI_ADVCD_CAP_VALIDATE_PATH]);

        txtVLCParams.setId(widgetIds[UI_ADVCD_TXT_VLC_PARAMS]);
        txtVLCParams.SetDescriptor(dataRefs.GetVLCParams());
        txtMaxRadioDist.setId(widgetIds[UI_ADVCD_TXT_MAX_RADIO_DIST]);
        txtMaxRadioDist.tfFormat = TFTextFieldWidget::TFF_DIGITS;
        txtMaxRadioDist.SetDescriptor(dataRefs.GetMaxRadioDist());

        // set current values
        UpdateValues();

        // center the UI
        Center();
    }
}

void LTSettingsUI::Disable()
{
    if (isEnabled()) {
        // remove widgets and free memory
        XPDestroyWidget(*widgetIds, 1);
        delete widgetIds;
        widgetIds = nullptr;
    }
}

// make sure I'm created before first use
void LTSettingsUI::Show (bool bShow)
{
    if (bShow)              // create before use
        Enable();
    TFWidget::Show(bShow);  // show/hide
    
#ifdef XPLM301
    // make sure we are in the right window mode
    if (GetWndMode() != GetDefaultWndOpenMode()) {      // only possible in XP11
        if (GetDefaultWndOpenMode() == TF_MODE_VR)
            SetWindowPositioningMode(xplm_WindowVR, -1);
        else
            SetWindowPositioningMode(xplm_WindowPositionFree, -1);	
    }
#endif
}

// update state of some buttons from dataRef
void LTSettingsUI::UpdateValues ()
{
    char buf[100];
    
    // *** Basics ***
    
    // are COMs selected?
    for (int idx = 0; idx < COM_CNT; idx++)
        btnBasicsCom[idx].SetChecked(dataRefs.ShallActOnCom(idx));
    
    // LiveTraffic's status
    snprintf(buf, sizeof(buf), capLTIntegFormatStr.c_str(),
             dataRefs.GetLTStatusText().c_str());
    capLTIntegration.SetDescriptor(buf);
    
    // full status of COM1/2 channels
    capCOM1Status.SetDescriptor(gChn[0].Summary());
    capCOM2Status.SetDescriptor(gChn[1].Summary());

    // *** Advanced ***
    logLevelGrp.SetCheckedIndex(dataRefs.GetLogLevel());
    msgAreaLevelGrp.SetCheckedIndex(dataRefs.GetMsgAreaLevel() - 1);
}

// capture entry into text fields like "path to VLC"
bool LTSettingsUI::MsgTextFieldChanged (XPWidgetID textWidget, std::string text)
{
    // *** Basic ***
    // audio desync adjust
    if (txtDesyncAdjust == textWidget) {
        dataRefs.SetManualDesync(std::stoi(txtDesyncAdjust.GetDescriptor()));
        return true;
    }
    
    // *** Advanced ***
    if (txtPathToVLC == textWidget) {
        ValidateUpdateVLCPath(text);
        // processed
        return true;
    }
    
    if (txtVLCParams == textWidget) {
        if (text.empty()) {                 // empty input == 'return to default'
            dataRefs.SetVLCParams(CFG_PARAMS_DEFAULT);
            txtVLCParams.SetDescriptor(CFG_PARAMS_DEFAULT);
        }
        else {
            dataRefs.SetVLCParams(text);
        }
        return true;
    }
    
    // max radio distance
    if (txtMaxRadioDist == textWidget) {
        dataRefs.SetMaxRadioDist(std::stoi(txtMaxRadioDist.GetDescriptor()));
        return true;
    }
    
    // not ours
    return TFMainWindowWidget::MsgTextFieldChanged(textWidget, text);
}


// writes current values out into config file
bool LTSettingsUI::MsgHidden (XPWidgetID hiddenWidget)
{
    if (hiddenWidget == *this) {        // only if it was me who got hidden
        // then only save the config file
        dataRefs.SaveConfigFile();
    }
    // pass on in class hierarchy
    return TFMainWindowWidget::MsgHidden(hiddenWidget);
}


// handles show/hide of 'tabs', values of logging level
bool LTSettingsUI::MsgButtonStateChanged (XPWidgetID buttonWidget, bool bNowChecked)
{
    // first pass up the class hierarchy to make sure the button groups are handled correctly
    bool bRet = TFMainWindowWidget::MsgButtonStateChanged(buttonWidget, bNowChecked);
    
    // *** Tab Groups ***
    // if the button is one of our tab buttons show/hide the appropriate subwindow
    if (widgetIds[UI_BTN_BASICS] == buttonWidget) { subBasics.Show(bNowChecked); return true; }
    else if (widgetIds[UI_BTN_ADVANCED] == buttonWidget) { subAdvcd.Show(bNowChecked); return true; }

    // *** Basics ***
    
    // Save value for "watch COM#"
    for (int idx = 0; idx < COM_CNT; idx++) {
        if (btnBasicsCom[idx] == buttonWidget) {
            dataRefs.SetActOnCom(idx, bNowChecked);
            return true;
        }
    }
    
    // Play only if selected
    if (btnPlayIfSelected == buttonWidget) { dataRefs.SetRespectAudioSelect(bNowChecked); return true; }
    // Audio delay as per LT's buffering period
    if (btnLTUseBufPeriod == buttonWidget) { dataRefs.SetDesyncWithLTDelay(bNowChecked); return true; }
    // Play only if selected
    if (btnKeepPrevWhileDesync == buttonWidget) { dataRefs.SetRunPrevFrequTillDesync(bNowChecked); return true; }

    // *** Advanced ***
    // if any of the log-level radio buttons changes we set log-level accordingly
    if (bNowChecked && logLevelGrp.isInGroup(buttonWidget))
    {
        dataRefs.SetLogLevel(logLevelTy(logLevelGrp.GetCheckedIndex()));
        return true;
    }
    if (bNowChecked && msgAreaLevelGrp.isInGroup(buttonWidget))
    {
        dataRefs.SetMsgAreaLevel(logLevelTy(msgAreaLevelGrp.GetCheckedIndex() + 1));
        return true;
    }
    
    return bRet;
}

// push buttons
bool LTSettingsUI::MsgPushButtonPressed (XPWidgetID buttonWidget)
{
    // *** Help ***
    if (widgetIds[UI_BTN_HELP] == buttonWidget)
    {
        // open help
        OpenURL(HELP_URL_SETTINGS);
        return true;
    }
    
    // we don't know that button...
    return TFMainWindowWidget::MsgPushButtonPressed(buttonWidget);
}

// update state of some buttons from dataRef every second
bool LTSettingsUI::TfwMsgMain1sTime ()
{
    TFMainWindowWidget::TfwMsgMain1sTime();
    UpdateValues();
    return true;
}

// validate and update VLCPath
void LTSettingsUI::ValidateUpdateVLCPath (const std::string newPath)
{
    // isn't this a change?
    if (dataRefs.GetVLCPath() == newPath)
        return;
    
    // Check if the given path is an existing directory
    // TODO: Better validation for an actual VLC plugin directory.
    if (newPath.empty())
        capValidatePath.SetDescriptor(MSG_VLC_NO_PATH);
    else if (isDirectory(newPath))
        capValidatePath.SetDescriptor(MSG_VLC_PATH_VERIFIED);
    else
        capValidatePath.SetDescriptor(MSG_VLC_PATH_NO_DIR);
    
    // no matter what, we safe the result and try to re-init VLC
    dataRefs.SetVLCPath(newPath);
    if (!COMChannel::InitAllVLC())
        capValidatePath.SetDescriptor(capValidatePath.GetDescriptor() + MSG_VLC_INIT_FAILED);
}

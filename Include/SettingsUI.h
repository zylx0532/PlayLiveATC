//
//  SettingsUI.h
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

#ifndef SettingsUI_h
#define SettingsUI_h

#define MSG_VLC_NO_PATH             "No path to validate"
#define MSG_VLC_PATH_VERIFIED       "Path seems to be valid."
#define MSG_VLC_PATH_NO_FILE        "Path validation FAILED, not a file. Saved anyway."

//
// Settings UI Main window
//
class LTSettingsUI : public TFMainWindowWidget
{
protected:
    XPWidgetID* widgetIds;              // all widget ids in the dialog
    TFButtonGroup tabGrp;               // button group to switch 'tabs'
    // sub-windows ('tabs')
    TFWidget subBasics, subAdvcd;
    
    // Basics tab
    TFButtonWidget btnBasicsCom[COM_CNT]; // toggle act on COM1/2
    TFTextFieldWidget txtPathToVLC;
    TFWidget capValidatePath;
    TFTextFieldWidget txtVLCParams;

    // Advanced tab
    TFButtonGroup logLevelGrp;          // radio buttons to select logging level
    TFButtonGroup msgAreaLevelGrp;      // radio buttons to select msg area level

public:
    LTSettingsUI();
    ~LTSettingsUI();
    
    // (de)register widgets (not in constructor to be able to use global variable)
    void Enable();
    void Disable();
    bool isEnabled () const { return widgetIds && *widgetIds; }
    
    // first creates the structure, then shows the window
    virtual void Show (bool bShow = true);
    // update shown values from dataRefs
    void UpdateValues ();

protected:
    // capture entry into 'filter for transponder hex code' field
    virtual bool MsgTextFieldChanged (XPWidgetID textWidget, std::string text);
    // writes current values out into config file
    virtual bool MsgHidden (XPWidgetID hiddenWidget);

    // handles show/hide of 'tabs'
    virtual bool MsgButtonStateChanged (XPWidgetID buttonWidget, bool bNowChecked);
    virtual bool MsgPushButtonPressed (XPWidgetID buttonWidget);

    // update states of buttons/fields, that can change elsewhere
    virtual bool TfwMsgMain1sTime ();
    
    // validate and update VLCPath
    void ValidateUpdateVLCPath (const std::string newPath);
};

#endif /* SettingsUI_h */

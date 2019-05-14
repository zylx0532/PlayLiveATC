//
//  Constants.h
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

#ifndef Constants_h
#define Constants_h

//
// MARK: Version Information (CHANGE VERSION HERE)
//
constexpr float VERSION_NR = 0.01f;
constexpr bool VERSION_BETA = true;
extern float verXPlaneOrg;          // version on X-Plane.org
extern int verDateXPlaneOrg;        // and its date

//MARK: Window Position
#define WIN_WIDTH       400         // window width
#define WIN_ROW_HEIGHT   20         // height of line of text
#define WIN_FROM_TOP    250
#define WIN_FROM_RIGHT    0

constexpr int WIN_TIME_DISPLAY=8;       // duration of displaying a message windows
constexpr std::chrono::milliseconds WIN_TIME_REMAIN(500); // time to keep the msg window after last message

//MARK: Unit conversions
constexpr int SEC_per_M     = 60;       // 60 seconds per minute
constexpr int SEC_per_H     = 3600;     // 3600 seconds per hour
constexpr int H_per_D       = 24;       // 24 hours per day
constexpr int M_per_D       = 1440;     // 24*60 minutes per day
constexpr int SEC_per_D     = SEC_per_H * H_per_D;        // seconds per day


//MARK: Version Information
extern char SLA_VERSION[];               // like "1.0"
extern char SLA_VERSION_FULL[];          // like "1.0.181231" with last digits being build date
extern char HTTP_USER_AGENT[];          // like "LiveTraffic/1.0"
extern time_t SLA_BETA_VER_LIMIT;        // BETA versions are limited
extern char SLA_BETA_VER_LIMIT_TXT[];
#define BETA_LIMITED_VERSION    "BETA limited to %s"
#define BETA_LIMITED_EXPIRED    "BETA-Version limited to %s has EXPIRED -> SHUTTING DOWN!"
constexpr int SLA_NEW_VER_CHECK_TIME = 48;   // [h] between two checks of a new

//MARK: Text Constants
#define SWITCH_LIVE_ATC         "SwitchLiveATC"
#define SLA_CFG_VERSION         "1.0"        // current version of config file format
#define PLUGIN_SIGNATURE        "TwinFan.plugin.SwitchLiveATC"
#define PLUGIN_DESCRIPTION      "Switch LiveATC channel based on COM radios."

//MARK: Menu Items
#define MENU_TOGGLE_COM1        "Act on COM1 change"
#define MENU_TOGGLE_COM2        "Act on COM2 change"
#define MENU_SETTINGS_UI        "Settings..."
#define MENU_HELP               "Help"
#ifdef DEBUG
#define MENU_RELOAD_PLUGINS     "Reload all Plugins (Caution!)"
#endif

//MARK: Config File Entries
#define CFG_TOGGLE_COM          "ActOnChangeOfCOM%d"
#define CFG_VLC_PATH            "VLCPath"
#define CFG_VLC_PARAMS          "VLCParams"

#if IBM                         // Windows default path and parameters
#define CFG_PATH_DEFAULT        "c:\\Program Files (x86)\\VideoLAN\\VLC\\vlc.exe"
#define CFG_PARAMS_DEFAULT      "--qt-start-minimized --audio-desync={desync} {url}"
#elif LIN                       // Linux default path and parameters
#define CFG_PATH_DEFAULT        "/usr/bin/vlc"
#define CFG_PARAMS_DEFAULT      "-Idummy --audio-desync={desync} {url}"
#else                           // Mac OS default path and parameters
#define CFG_PATH_DEFAULT        "/Applications/VLC.app/Contents/MacOS/VLC"
#define CFG_PARAMS_DEFAULT      "-Idummy --audio-desync={desync} {url}"
#endif

#define PARAM_REPL_DESYNC       "{desync}"
#define PARAM_REPL_URL          "{url}"

#define CFG_LOG_LEVEL           "LogLevel"
#define CFG_MSG_AREA_LEVEL      "MsgAreaLevel"

//MARK: Help URLs
// TODO: Set real URLs
#define HELP_URL                "https://twinfan.gitbook.io/livetraffic/SwitchLiveATC"
#define HELP_URL_SETTINGS       "https://twinfan.gitbook.io/livetraffic/SwitchLiveATC/Settings"

//MARK: File Paths
// these are under X-Plane's root dir
#define PATH_CONFIG_FILE        "Output/preferences/SwitchLiveATC.prf"

//MARK: Error Texsts
constexpr long HTTP_OK =            200;
constexpr long HTTP_BAD_REQUEST =   400;
constexpr long HTTP_NOT_FOUND =     404;
constexpr long HTTP_NOT_AVAIL =     503;        // "Service not available"
constexpr int CH_MAC_ERR_CNT =      5;          // max number of tolerated errors, afterwards invalid channel
constexpr int SERR_LEN = 100;                   // size of buffer for IO error texts (strerror_s)
constexpr int ERR_CFG_FILE_MAXWARN = 5;     // maximum number of warnings while reading config file, then: dead
#define ERR_APPEND_MENU_ITEM    "Could not append a menu item"
#define ERR_CREATE_MENU         "Could not create menu %s"
#define ERR_CURL_INIT           "Could not initialize CURL: %s"
#define ERR_CURL_EASY_INIT      "Could not initialize easy CURL"
#define ERR_CURL_PERFORM        "%s: Could not get network data: %d - %s"
#define ERR_CURL_NOVERCHECK     "Could not browse X-Plane.org for version info: %d - %s"
#define ERR_CURL_HTTP_RESP      "%s: HTTP response is not OK but %ld"
#define ERR_CURL_REVOKE_MSG     "revocation"                // appears in error text if querying revocation list fails
#define ERR_CURL_DISABLE_REV_QU "%s: Querying revocation list failed - have set CURLSSLOPT_NO_REVOKE and am trying again"
#define ERR_DATAREF_FIND        "Could not find DataRef/CmdRef: %s"
#define ERR_CREATE_COMMAND      "Could not create command %s"
#define ERR_TOP_LEVEL_EXCEPTION "Caught top-level exception! %s"
#define ERR_CFG_FILE_OPEN_OUT   "Could not create config file '%s': %s"
#define ERR_CFG_FILE_WRITE      "Could not write into config file '%s': %s"
#define ERR_CFG_FILE_OPEN_IN    "Could not open '%s': %s"
#define ERR_CFG_FILE_VER        "Config file '%s' first line: Unsupported format or version: %s"
#define ERR_CFG_FILE_VER_UNEXP  "Config file '%s' first line: Unexpected version %s, expected %s...trying to continue"
#define ERR_CFG_FILE_IGNORE     "Ignoring unkown entry '%s' from config file '%s'"
#define ERR_CFG_FILE_WORDS      "Expected two words (key, value) in config file '%s', line '%s': ignored"
#define ERR_CFG_FILE_READ       "Could not read from '%s': %s"
#define ERR_CFG_LINE_READ       "Could not read from file '%s', line %d: %s"
#define ERR_CFG_FILE_TOOMANY    "Too many warnings"
#define ERR_CFG_FILE_VALUE      "%s: Could not convert '%s' to a number: %s"
#define ERR_CFG_FORMAT          "Format mismatch in '%s', line %d: %s"
#define ERR_CFG_VAL_INVALID     "Value invalid in '%s', line %d: %s"
#define ERR_WIDGET_CREATE       "Could not create widget required for settings UI"

// MARK: Processing Info
#define MSG_STARTUP             SWITCH_LIVE_ATC " %s starting up..."
#define MSG_DISABLED            SWITCH_LIVE_ATC " disabled"
#define MSG_COM_CHANGE_DETECT   "COM%d is now %s"

//MARK: Debug Texts
#define DBG_MENU_CREATED        "Menu created"
#define DBG_WND_CREATED_UNTIL   "Created window, display until total running time %.2f, for text: %s"
#define DBG_WND_DESTROYED       "Window destroyed"
#define DBG_SLA_MAIN_INIT       "SwitchLiveATC initialized"
#define DBG_SLA_MAIN_ENABLE     "SwitchLiveATC enabled"
#define DBG_SENDING_HTTP        "%s: Sending HTTP: %s"
#define DBG_RECEIVED_BYTES      "%s: Received %ld characters"
#ifdef DEBUG
#define DBG_DEBUG_BUILD         "DEBUG BUILD with additional run-time checks and no optimizations"
#endif

#endif /* Constants_h */

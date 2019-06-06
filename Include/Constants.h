//
//  Constants.h
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

#ifndef Constants_h
#define Constants_h

//
// MARK: Version Information (CHANGE VERSION HERE)
//
constexpr float VERSION_NR = 0.01f;
constexpr bool VERSION_BETA = true;

//MARK: Window Position
#define WIN_WIDTH       400         // window width
#define WIN_ROW_HEIGHT   20         // height of line of text
#define WIN_FROM_TOP    250
#define WIN_FROM_RIGHT    0

constexpr int WIN_TIME_DISPLAY=8;       // duration of displaying a message windows
constexpr std::chrono::milliseconds WIN_TIME_REMAIN(500); // time to keep the msg window after last message

//MARK: Unit conversions
constexpr int M_per_NM      = 1852;     // meter per 1 nautical mile = 1/60 of a lat degree
constexpr double M_per_FT   = 0.3048;   // meter per 1 foot
constexpr int M_per_KM      = 1000;
constexpr double KT_per_M_per_S = 1.94384;  // 1m/s = 1.94384kt
constexpr int SEC_per_M     = 60;       // 60 seconds per minute
constexpr int SEC_per_H     = 3600;     // 3600 seconds per hour
constexpr int H_per_D       = 24;       // 24 hours per day
constexpr int M_per_D       = 1440;     // 24*60 minutes per day
constexpr int SEC_per_D     = SEC_per_H * H_per_D;        // seconds per day
constexpr double Ms_per_FTm = M_per_FT / SEC_per_M;     //1 m/s = 196.85... ft/min
constexpr double PI         = 3.1415926535897932384626433832795028841971693993751;
constexpr double EARTH_D_M  = 6371.0 * 2 * 1000;    // earth diameter in meter
constexpr double JAN_FIRST_2019 = 1546344000;   // 01.01.2019
constexpr double HPA_STANDARD   = 1013.25;      // air pressure
constexpr double INCH_STANDARD  = 2992.126;
constexpr double HPA_per_INCH   = HPA_STANDARD/INCH_STANDARD;
// The pressure drops approximately by 11.3 Pa per meter in first 1000 meters above sea level.
constexpr double PA_per_M       = 11.3;         // https://en.wikipedia.org/wiki/Barometric_formula
// ft altitude diff per hPa change
constexpr double FT_per_HPA     = (100/PA_per_M)/M_per_FT;

constexpr double SIMILAR_TS_INTVL = 3;          // seconds: Less than that difference and position-timestamps are considered "similar" -> positions are merged rather than added additionally
constexpr double MDL_ALT_MIN =         -1500;   // [ft] minimum allowed altitude
constexpr double MDL_ALT_MAX =          60000;  // [ft] maximum allowed altitude

//MARK: Version Information
extern char PLA_VERSION[];               // like "1.0"
extern char PLA_VERSION_FULL[];          // like "1.0.181231" with last digits being build date
extern char HTTP_USER_AGENT[];          // like "PlayLiveATC/1.0"
extern time_t PLA_BETA_VER_LIMIT;        // BETA versions are limited
extern char PLA_BETA_VER_LIMIT_TXT[];
#define BETA_LIMITED_VERSION    "BETA limited to %s"
#define BETA_LIMITED_EXPIRED    "BETA-Version limited to %s has EXPIRED -> SHUTTING DOWN! Get an up-to-date version from X-Plane.org."
constexpr int PLA_NEW_VER_CHECK_TIME = 48;   // [h] between two checks of a new

constexpr float PLA_STARTUP_DELAY = 5.0f;    // [s] before starting to check for COM changes, gives LT time to startup first

//MARK: Text Constants
#define SWITCH_LIVE_ATC         "PlayLiveATC"
#define PLA_CFG_VERSION         "1.0"        // current version of config file format
#define PLUGIN_SIGNATURE        "TwinFan.plugin.PlayLiveATC"
#define PLUGIN_DESCRIPTION      "Switch LiveATC channel based on COM radios."
#define LT_UNAVAILABLE          "not available"
#define LT_INACTIVE             "installed, but inactive"
#define LT_ACTIVE               "active, displaying aircrafts"

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
#define CFG_RESPECT_COM_SEL     "PlayComSelected"
#define CFG_VOLUME              "Volume"
#define CFG_VLC_PATH            "VLCPath"

#if IBM                         // Windows default path and parameters
#define CFG_PATH_DEFAULT        "c:\\Program Files\\VideoLAN\\VLC\\plugins"
#elif LIN                       // Linux default path and parameters
#define CFG_PATH_DEFAULT        "/usr/lib/vlc/plugins"
#else                           // Mac OS default path and parameters
#define CFG_PATH_DEFAULT        "/Applications/VLC.app/Contents/MacOS/plugins"
#endif

#define CFG_LT_DESYNC_BUF       "LTDesyncByBufPeriod"
#define CFG_DESYNC_MANUAL_ADJ   "DesyncManualAdjust"
#define CFG_PREV_WHILE_DESYNC   "ContPrevFrequWhileDesync"
#define CFG_MAX_RADIO_DIST      "MaxRadioDist"

#define CFG_LOG_LEVEL           "LogLevel"
#define CFG_MSG_AREA_LEVEL      "MsgAreaLevel"

//MARK: Help URLs
// TODO: Set real URLs
#define HELP_URL                "https://twinfan.gitbook.io/livetraffic/PlayLiveATC"
#define HELP_URL_SETTINGS       "https://twinfan.gitbook.io/livetraffic/PlayLiveATC/Settings"

//MARK: File Paths
// these are under X-Plane's root dir
#define PATH_CONFIG_FILE        "Output/preferences/PlayLiveATC.prf"

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
#define ERR_Y_PROBE             "Y Probe returned %d at %s"

// MARK: Processing Info
#define MSG_STARTUP             SWITCH_LIVE_ATC " %s starting up..."
#define MSG_DISABLED            SWITCH_LIVE_ATC " disabled"

//MARK: Debug Texts
#define DBG_MENU_CREATED        "Menu created"
#define DBG_WND_CREATED_UNTIL   "Created window, display until total running time %.2f, for text: %s"
#define DBG_WND_DESTROYED       "Window destroyed"
#define DBG_PLA_MAIN_INIT       "PlayLiveATC initialized"
#define DBG_PLA_MAIN_ENABLE     "PlayLiveATC enabled"
#define DBG_SENDING_HTTP        "%s: Sending HTTP: %s"
#define DBG_RECEIVED_BYTES      "%s: Received %ld characters"
#ifdef DEBUG
#define DBG_DEBUG_BUILD         "DEBUG BUILD with additional run-time checks and no optimizations"
#endif

#endif /* Constants_h */

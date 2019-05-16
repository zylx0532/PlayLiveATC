//
//  SwitchLiveATC.h
//  SwitchLiveATC
//

/*
 * Copyright (c) 2018, Birger Hoppe
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

#ifndef SwitchLiveATC_h
#define SwitchLiveATC_h

#if !defined(XPLM200) || !defined(XPLM210)
#error This is made to be compiled at least against the XPLM210 SDK (X-plane 10.30 and above)
#endif

// MARK: Includes
// Standard C
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <ctime>
#include <cassert>
// Standard C++
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <fstream>
#include <future>
#include <regex>

// Windows
#if IBM
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// we prefer std::max/min of <algorithm>
#undef max
#undef min
#endif

// Open GL
#if LIN
#include <GL/gl.h>
#elif __GNUC__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// CURL
// #define CURL_STATICLIB if linking with a static CURL lib!
#include "curl/curl.h"              // for CURL*

// C++

// X-Plane SDK
#include "XPLMDataAccess.h"
#include "XPLMMenus.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"

// SwitchLiveATC Includes
#include "TFWidgets.h"
#include "Constants.h"
#include "Utilities.h"
#include "TextIO.h"
#include "DataRefs.h"
#include "SettingsUI.h"
#include "SLAFindLiveATC.h"
#include "SLARunVLC.h"

// Global variables
extern DataRefs dataRefs;           // in SwitchLiveATC.cpp

// Global functions
bool InitFullVersion ();

#endif /* SwitchLiveATC_h */

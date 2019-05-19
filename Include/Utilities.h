//
//  Utilities.h
//  SwitchLiveATC
//
// TCommon function that can in the long run be shared between plugins
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

#ifndef Utilities_h
#define Utilities_h

// MARK: URL support

void OpenURL  (const std::string url);

// MARK: File & Path helpers

// read a text line no matter what line ending
std::istream& safeGetline(std::istream& is, std::string& t);

// does a file path exist?
bool existsFile (const std::string& filename);

#if IBM
// returns text for the given error code, per default taken from GetLastError
std::string GetErrorStr(DWORD err = -1);

// https://stackoverflow.com/a/21767578
HWND FindMainWindow(HANDLE hProcess);
#endif

// MARK: String helpers

// separates string into tokens
std::vector<std::string> str_tokenize (const std::string s,
                                       const std::string tokens,
                                       bool bSkipEmpty = true);

// replace 'find' with 'replace', returns number of repacements
int str_replace (std::string& s,
                 const std::string& find,
                 const std::string& repl);

// MARK: Misc

// comparing 2 doubles for near-equality
bool dequal ( const double d1, const double d2 );

// MARK: Compiler differences

#if APL == 1 || LIN == 1
// not quite the same but close enough for our purposes
inline void strcpy_s(char * dest, size_t destsz, const char * src)
{ strncpy(dest, src, destsz); dest[destsz-1]=0; }
inline void strcat_s(char * dest, size_t destsz, const char * src)
{ strncat(dest, src, destsz - strlen(dest) - 1); }

// these simulate the VC++ version, not the C standard versions!
inline struct tm *gmtime_s(struct tm * result, const time_t * time)
{ return gmtime_r(time, result); }
inline struct tm *localtime_s(struct tm * result, const time_t * time)
{ return localtime_r(time, result); }

#endif

#if APL == 1
// XCode/Linux don't provide the _s functions, not even with __STDC_WANT_LIB_EXT1__ 1
inline int strerror_s( char *buf, size_t bufsz, int errnum )
{ return strerror_r(errnum, buf, bufsz); }
#endif
#if LIN == 1
inline int strerror_s( char *buf, size_t bufsz, int errnum )
{ strcpy_s(buf,bufsz,strerror(errnum)); return 0; }
#endif

#endif /* Utilities_h */

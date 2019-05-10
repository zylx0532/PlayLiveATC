//
//  Utilities.cpp
//
// Common function that can in the long run be shared between plugins

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

#include <sys/stat.h>

#if IBM
#include <shellapi.h>           // for ShellExecuteA
#endif


//
// MARK: URL/Help support
//

void OpenURL  (const std::string url)
{
#if IBM
    // Windows implementation: ShellExecuteA
    // https://docs.microsoft.com/en-us/windows/desktop/api/shellapi/nf-shellapi-shellexecutea
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif LIN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    // Unix uses xdg-open, package xdg-utils, pre-installed at least on Ubuntu
    (void)system((std::string("xdg-open ") + url).c_str());
#pragma GCC diagnostic pop
#else
    // Mac use standard system/open
    system((std::string("open ") + url).c_str());
    // Code that causes warning goes here
#endif
}

//
// MARK: File & Path helpers
//

// read a text line no matter what line ending
// https://stackoverflow.com/a/6089413
std::istream& safeGetline(std::istream& is, std::string& t)
{
    t.clear();
    
    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.
    
    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();
    
    for(;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
                return is;
            case '\r':
                if(sb->sgetc() == '\n')
                    sb->sbumpc();
                return is;
            case std::streambuf::traits_type::eof():
                // Also handle the case when the last line has no line ending
                if(t.empty())
                    is.setstate(std::ios::eofbit);
                return is;
            default:
                t += (char)c;
        }
    }
}

// does a file path exist? (in absence of <filesystem>, which XCode refuses to ship
// https://stackoverflow.com/a/51301928
bool existsFile (const std::string& filename) {
    struct stat buffer;
    return (stat (filename.c_str(), &buffer) == 0);
}

#if IBM
// returns text for the given error code, per default taken from GetLastError
std::string GetErrorStr(DWORD err)
{
    char errTxt[100] = { 0 };
    if (err == -1)
        err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, errTxt, sizeof(errTxt), NULL);
    return std::string(errTxt);
}

// find (good guess of a) main window of a process
// https://stackoverflow.com/a/21767578
struct wndEnumData {
    unsigned long process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0; // && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam)
{
    wndEnumData& data = *(wndEnumData*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !is_main_window(handle))
        return TRUE;
    data.window_handle = handle;
    return FALSE;
}

HWND FindMainWindow(HANDLE hProcess)
{
    wndEnumData data;
    data.process_id = GetProcessId(hProcess);
    data.window_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}


#endif

// separates string into tokens
std::vector<std::string> str_tokenize (const std::string s,
                                       const std::string tokens,
                                       bool bSkipEmpty)
{
    std::vector<std::string> v;
    
    // find all tokens before the last
    size_t b = 0;                                   // begin
    for (size_t e = s.find_first_of(tokens);        // end
         e != std::string::npos;
         b = e+1, e = s.find_first_of(tokens, b))
    {
        if (!bSkipEmpty || e != b)
            v.emplace_back(s.substr(b, e-b));
    }
    
    // add the last one: the remainder of the string (could be empty!)
    v.emplace_back(s.substr(b));
    
    return v;
}


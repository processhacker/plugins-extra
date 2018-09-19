# DPI Awareness Extras plugin
* Adds a "DPI awareness extended" column to the process tree which
  1. Shows additional info about DPI awareness
  2. Works for Windows versions before 8.1 (before GetProcessDpiAwareness was introduced)
  
## Flags
Flags are shown in parenthesis after the awareness. They consists of a letter followed by a status
which is one of
  * `+` The flag is on
  * `-` The flag is off
  * `?` The status is unknown
  
Currently the following flags are shown
  * `F` The DPI awareness is forced by the system, may be due to compatibility settings or desktop-composition being disabled.
  
## Known limitations
On Windows 8.1+ the DPI awareness of processes on other sessions/window stations (possibly even desktops) are always shown as Unaware, this is due to a limitation of GetProcessDpiAwareness.
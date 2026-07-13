// =============================================================================
// ScreenSaverHost.h  -  Window + message-loop management for the .scr modes
// -----------------------------------------------------------------------------
// Implements the three screensaver run modes:
//   * Fullscreen (/s) - one borderless top-most window per monitor, each driving
//                       its own Engine; exits on any mouse move / click / key.
//   * Preview    (/p) - a child window rendered inside the Settings preview pane.
//   * Config     (/c) - handed off to the Settings dialog (SettingsDialog.cpp).
// High-DPI aware; uses a frame limiter to keep CPU low while supporting high
// refresh rates and VRR.
// =============================================================================
#pragma once
#include "Common.h"
#include "Settings.h"

namespace nyx {

// Runs the fullscreen saver across all monitors. Returns on user input.
int RunFullscreen(HINSTANCE hInst, const Settings& settings);

// Runs the small preview inside the Windows Settings dialog.
int RunPreview(HINSTANCE hInst, HWND parent, const Settings& settings);

// Defined in SettingsDialog.cpp.
int RunConfig(HINSTANCE hInst, HWND parent);

} // namespace nyx

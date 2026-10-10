//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: Dark theme for Hammer++: system colors, Windows dark mode, control colors
//
//=============================================================================//

#pragma once

// Call on the GUI thread before the first window is created, outside the loader lock.
// Does nothing when hppxdark.ini next to the DLL has [DarkTheme] Enabled=0
void DarkTheme_Init();

// Hooks the system colors of newly loaded modules before they create windows
void DarkTheme_OnModuleLoaded();

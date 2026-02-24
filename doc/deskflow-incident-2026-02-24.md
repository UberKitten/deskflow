# Deskflow incident notes (2026-02-24)

## Symptom
- While controlling the Windows server from macOS, pointer motion stopped moving the remote cursor, but clicks still landed in the same spot. Screen switching became impossible until the client was stopped.

## Log lines observed
Server log highlights:
- `dropped bogus delta motion: +1719,-717` (repeated)
- `locked by "Left Button"` / `locked by mouse buttonID: 1`
- `switched to desk ""` + `desktop is inaccessible`

Client log highlights:
- `WARNING: cursor may not be visible`

## Source mapping
- **Bogus delta drop**
  - `MSWindowsScreen::onMouseMove`
  - `src/lib/platform/MSWindowsScreen.cpp` (around the "bogusZoneSize" check)
  - The guard drops deltas roughly equal to (center -> edge), which matches 1719/717 deltas on a 3440x1440 layout.
- **Mouse-button lock**
  - `MSWindowsScreen::isAnyMouseButtonDown`
  - `src/lib/platform/MSWindowsScreen.cpp`
  - `Screen::isLockedToScreen` uses this to block switching while a button is held.
- **Secure desktop transition**
  - `MSWindowsDesks::checkDesk`
  - `src/lib/platform/MSWindowsDesks.cpp`
  - Logs "desktop is inaccessible" when the current desktop (UAC/login) can’t be opened; key/button sync is skipped.
- **Cursor visibility warning (client)**
  - `OSXScreen::logCursorVisibility`
  - `src/lib/platform/OSXScreen.mm`

## Hypothesis
A secure-desktop/UAC switch caused us to miss a mouse-up or fail cursor warps. That left stale button state (`Left Button`) and produced repeated large deltas (warp-distance) that were dropped as bogus, effectively freezing remote motion while still sending clicks.

## Fixes implemented
1) **Use real-time button state to prevent stale locks**
   - `MSWindowsScreen::isAnyMouseButtonDown` now queries `GetKeyState` for all mouse buttons instead of relying solely on cached `m_buttons`.
2) **Fallback warping when controlling a secondary screen**
   - `MSWindowsScreen::warpCursorNoFlush` now applies the SetCursorPos failure fallback when `!m_isOnScreen` (primary controlling secondary), not only on non-primary screens.

## Files changed
- `src/lib/platform/MSWindowsScreen.cpp`
- `doc/deskflow-incident-2026-02-24.md`

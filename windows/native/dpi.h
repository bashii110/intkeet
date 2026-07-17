#pragma once
// Per-monitor DPI v2 helpers (01-SRD.md FR-24/FR-25). Pure functions
// where possible so dpi_test.cpp can exercise the scaling math without
// creating a real window (02-ARCHITECTURE.md §11).
//
// The process-wide manifest declaration itself
// (<dpiAwareness>PerMonitorV2</dpiAwareness>) lives in
// windows/runner/main.exe.manifest, which is part of the Flutter-
// generated runner scaffold — Flutter's default Windows runner manifest
// already declares PerMonitorV2, so no change is needed there; this file
// only covers the runtime scaling math and query helpers.

#include <windows.h>

#include <cstdint>

namespace ai_overlay {

constexpr double kBaselineDpi = 96.0;

// Returns the DPI for `hwnd` via GetDpiForWindow (Windows 10 1607+),
// documented at:
// https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-getdpiforwindow
// Falls back to kBaselineDpi if the API is unavailable (shouldn't happen
// given this project's minimum OS target, 01-SRD.md §7, but never
// silently divide by zero).
UINT GetWindowDpiSafe(HWND hwnd);

inline double DpiToScaleFactor(UINT dpi) {
  return dpi > 0 ? static_cast<double>(dpi) / kBaselineDpi : 1.0;
}

// Scales a logical-pixel dimension to physical pixels for the given DPI.
inline int32_t ScaleForDpi(int32_t logical_value, UINT dpi) {
  return static_cast<int32_t>(logical_value * DpiToScaleFactor(dpi));
}

// Converts a size from one DPI to another — used when a window moves
// between two differently-scaled monitors and needs to keep its logical
// (not physical) size constant, matching the Phase 1 gate requirement
// ("moving overlay between differently-scaled monitors keeps it crisp,
// correctly sized").
inline int32_t RescaleDimension(int32_t value, UINT from_dpi, UINT to_dpi) {
  if (from_dpi == 0) return value;
  return static_cast<int32_t>(
      (static_cast<int64_t>(value) * to_dpi) / static_cast<int64_t>(from_dpi));
}

}  // namespace ai_overlay

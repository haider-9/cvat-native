#pragma once

namespace cvat {

// Applies the theme (system, light or dark), the interface font and the
// icon colours from Settings. Call once at startup and again after the
// settings are saved. The first call records the system style and palette,
// so switching back to "system" restores them exactly.
void applyAppearance();

} // namespace cvat

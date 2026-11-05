#pragma once

// The frame metrics of a window with a hidden title bar, as plain arithmetic so that
// it can be tested anywhere (tests/window_top_border_inset_test.cpp). No <windows.h>.
//
// Windows 11 draws one thin border around the whole window, the client area included;
// earlier versions draw the frame only where the window still has non-client area.
// Handing the whole top caption band to the content (WM_NCCALCSIZE) therefore takes the
// top border with it on Windows 10, while the left, right and bottom borders stay
// (window_manager#547): the window shows three sides of a frame and no top. Keeping one
// pixel of the top sizing frame with the system leaves the compositor the non-client
// area it needs to draw that border.

#include <cstdint>

namespace nativeapi {
namespace frame_metrics {

// The first Windows 11 build. From here on the compositor draws the window border
// uniformly around the whole window, client area included, and a kept strip of the
// top sizing frame would only move the content down.
constexpr uint32_t kBuildUniformWindowBorder = 22000;

// Height of the top sizing frame left to the system while the title bar is hidden, in
// physical pixels, for an OS `build` number. The visible border is the outermost pixel
// of that frame at every scale, so one pixel is all the compositor needs; the sizing
// frame inside it grows with the DPI but is invisible. The content stops at the same
// line because it is laid out in the client area. A build of 0 (a failed version query)
// takes the pre-Windows 11 path: keeping the strip is harmless on Windows 11's
// successors but losing the border is visible on Windows 10. Maximized and full screen
// windows keep no strip at any build; their call sites do not apply this inset.
inline int TopBorderInsetForBuild(uint32_t build) {
  return build < kBuildUniformWindowBorder ? 1 : 0;
}

}  // namespace frame_metrics
}  // namespace nativeapi

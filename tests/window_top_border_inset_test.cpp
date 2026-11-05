// The top border inset of a hidden title bar (src/platform/windows/frame_metrics_windows.h):
// builds before Windows 11 keep one pixel of the top sizing frame so that the compositor
// draws the top border there (window_manager#547, nativeapi-core#76). Plain arithmetic on
// the OS build number; no window, no input, runs on every platform and every runner -
// unlike the window itself, whose geometry test can only exercise the path of the OS it
// runs on.
#include "../src/platform/windows/frame_metrics_windows.h"

#include <cstdio>

using namespace nativeapi::frame_metrics;

static int failures = 0;

static void Check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  failures += !ok;
}

// Act + assert helper: the inset one build number must produce.
static void CheckInset(unsigned build, int expected, const char* what) {
  const int inset = TopBorderInsetForBuild(build);
  std::printf("%s build %u: inset %d, expected %d\n", inset == expected ? "  ok " : "  BAD",
              build, inset, expected);
  Check(inset == expected, what);
}

// Windows 10 and Server builds draw the frame only on non-client area: the strip stays.
static void test_top_border_inset_pre_windows11_builds_keep_one_pixel() {
  // Arrange: shipped builds from Windows 8.1 through Server 2022, all < 22000.
  // Act + Assert
  CheckInset(9600, 1, "Windows 8.1 (9600) keeps the strip");
  CheckInset(14393, 1, "Windows 10 1607 (14393) keeps the strip");
  CheckInset(17763, 1, "Windows 10 1809 / Server 2019 (17763) keeps the strip");
  CheckInset(19041, 1, "Windows 10 2004 (19041) keeps the strip");
  CheckInset(19045, 1, "Windows 10 22H2 (19045) keeps the strip");
  CheckInset(20348, 1, "Server 2022 (20348) keeps the strip");
}

// Windows 11 draws the border over the client area itself: no strip, the content stays
// at the top edge.
static void test_top_border_inset_windows11_builds_keep_none() {
  // Arrange + Act + Assert: every shipped Windows 11 generation, all >= 22000.
  CheckInset(22000, 0, "Windows 11 21H2 (22000) keeps no strip");
  CheckInset(22621, 0, "Windows 11 22H2 (22621) keeps no strip");
  CheckInset(22631, 0, "Windows 11 23H2 (22631) keeps no strip");
  CheckInset(26100, 0, "Windows 11 24H2 / Server 2025 (26100) keeps no strip");
  CheckInset(26300, 0, "Windows 11 insider (26300) keeps no strip");
}

// The switch happens exactly at the first Windows 11 build.
static void test_top_border_inset_boundary_switches_at_22000() {
  // Arrange + Act + Assert
  Check(kBuildUniformWindowBorder == 22000, "the boundary constant is the first Windows 11 build");
  CheckInset(21999, 1, "one build below the boundary keeps the strip");
  CheckInset(22000, 0, "the boundary build itself keeps no strip");
}

// A failed RtlGetVersion reports 0; that must not silently drop the border on the
// Windows 10 path (see the note in frame_metrics_windows.h).
static void test_top_border_inset_unknown_build_falls_back_to_one_pixel() {
  // Arrange + Act + Assert
  CheckInset(0, 1, "an unknown build (0) keeps the strip");
}

int main() {
  test_top_border_inset_pre_windows11_builds_keep_one_pixel();
  test_top_border_inset_windows11_builds_keep_none();
  test_top_border_inset_boundary_switches_at_22000();
  test_top_border_inset_unknown_build_falls_back_to_one_pixel();
  std::printf("%s\n", failures ? "FAILED" : "OK");
  return failures ? 1 : 0;
}

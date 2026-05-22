#pragma once
#include <string>

namespace ui
{

// Pads str on the right with spaces until it occupies the given number of
// terminal columns. If str is already wider, it is truncated to fit.
std::string PadRight(const std::string& str, int width);

// Centers str within the given number of terminal columns by padding both
// sides with spaces. If str is already wider, it is truncated to fit.
std::string CenterText(const std::string& str, int width);

} // namespace ui

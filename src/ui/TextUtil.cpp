#include "TextUtil.hpp"
#include <cstdint>
#include <string_view>

namespace ui
{

namespace
{

// Decodes one UTF-8 code point starting at pos and advances pos past it.
// On a malformed lead byte, advances one byte and returns U+FFFD.
uint32_t Utf8Decode(std::string_view str, size_t& pos)
{
	unsigned char byte = static_cast<unsigned char>(str[pos]);
	uint32_t cp;
	int len;
	if      (byte < 0x80)           { cp = byte;        len = 1; }
	else if ((byte & 0xE0) == 0xC0) { cp = byte & 0x1F; len = 2; }
	else if ((byte & 0xF0) == 0xE0) { cp = byte & 0x0F; len = 3; }
	else if ((byte & 0xF8) == 0xF0) { cp = byte & 0x07; len = 4; }
	else                            { ++pos; return 0xFFFD; }
	for (int j = 1; j < len && pos + j < str.size(); ++j)
		cp = (cp << 6) | (static_cast<unsigned char>(str[pos + j]) & 0x3F);
	pos += static_cast<size_t>(len);
	return cp;
}

// True if the code point occupies two terminal columns (CJK, emoji, etc.).
bool IsWide(uint32_t cp)
{
	return (cp >= 0x1100 && cp <= 0x115F) ||
	       (cp == 0x2329 || cp == 0x232A) ||
	       (cp >= 0x2E80 && cp <= 0x303E) ||
	       (cp >= 0x3040 && cp <= 0x33FF) ||
	       (cp >= 0x3400 && cp <= 0x4DBF) ||
	       (cp >= 0x4E00 && cp <= 0xA4CF) ||
	       (cp >= 0xAC00 && cp <= 0xD7AF) ||
	       (cp >= 0xF900 && cp <= 0xFAFF) ||
	       (cp >= 0xFE10 && cp <= 0xFE19) ||
	       (cp >= 0xFE30 && cp <= 0xFE4F) ||
	       (cp >= 0xFF00 && cp <= 0xFF60) ||
	       (cp >= 0xFFE0 && cp <= 0xFFE6) ||
	       (cp >= 0x1F300 && cp <= 0x1F9FF) ||
	       (cp >= 0x20000 && cp <= 0x2FFFD) ||
	       (cp >= 0x30000 && cp <= 0x3FFFD);
}

// Total terminal column width of a UTF-8 string.
int Utf8DisplayWidth(std::string_view str)
{
	int width = 0;
	size_t pos = 0;
	while (pos < str.size())
		width += IsWide(Utf8Decode(str, pos)) ? 2 : 1;
	return width;
}

// Truncates str to at most cols terminal columns, never splitting a code point.
std::string Utf8TruncateToCols(std::string_view str, int cols)
{
	int width = 0;
	size_t pos = 0;
	while (pos < str.size())
	{
		size_t charStart = pos;
		int charWidth = IsWide(Utf8Decode(str, pos)) ? 2 : 1;
		if (width + charWidth > cols)
			return std::string(str.substr(0, charStart));
		width += charWidth;
	}
	return std::string(str);
}

} // namespace

std::string PadRight(const std::string& str, int width)
{
	int displayWidth = Utf8DisplayWidth(str);
	if (displayWidth >= width)
		return Utf8TruncateToCols(str, width);
	return str + std::string(static_cast<size_t>(width - displayWidth), ' ');
}

std::string CenterText(const std::string& str, int width)
{
	int len = Utf8DisplayWidth(str);
	if (len >= width)
		return Utf8TruncateToCols(str, width);
	int leftPad = (width - len) / 2;
	int rightPad = width - len - leftPad;
	return std::string(static_cast<size_t>(leftPad), ' ') + str + std::string(static_cast<size_t>(rightPad), ' ');
}

} // namespace ui

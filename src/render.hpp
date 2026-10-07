// Markdown to wrapped, styled text lines for the MarkFar preview.
#pragma once

#include <string>
#include <vector>

namespace markfar {

// Style flags of a run of text; the plugin turns them into Far colours.
enum : unsigned
{
	S_STRONG  = 1u << 0,
	S_EM      = 1u << 1,
	S_CODE    = 1u << 2,   // inline code and code blocks without a known language
	S_LINK    = 1u << 3,
	S_DIM     = 1u << 4,   // URLs, frames, quote bars, front matter
	S_DEL     = 1u << 5,
	S_MARK    = 1u << 6,
	S_H1      = 1u << 7,   // headings level 1-2
	S_H       = 1u << 8,   // headings level 3-6
	S_TITLE   = 1u << 9,   // admonition titles, table headers
};

struct Run { int start, end; unsigned style; };   // columns [start, end)

struct Rendered
{
	std::vector<std::wstring> lines;
	std::vector<std::vector<Run>> runs;   // per line
	std::vector<int> sourceLine;          // per line: 0-based line in the source
	std::vector<int> headings;            // rendered line numbers of headings
};

struct RenderOptions
{
	int width = 80;          // columns available for text
	bool wrap = true;
	bool showUrls = true;
};

// Maps a fenced-code language tag to a Colorer type that markfar.hrc knows,
// or returns an empty string.
std::wstring ColorerType(std::wstring lang);

Rendered Render(const std::wstring& source, const RenderOptions& options);

} // namespace markfar

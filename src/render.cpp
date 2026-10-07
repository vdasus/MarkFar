// Markdown to wrapped, styled text lines for the MarkFar preview.
// md4c parses the source (UTF-16 build); Builder lays the blocks out to the
// window width with hanging indents and records which source line each
// rendered line comes from.

#include "render.hpp"

#define MD4C_USE_UTF16
#include "../third_party/md4c/md4c.h"

#include <algorithm>
#include <cwchar>
#include <map>

namespace markfar {

std::wstring ColorerType(std::wstring lang)
{
	const size_t cut = lang.find_first_of(L" \t{,");
	if (cut != std::wstring::npos) lang.resize(cut);
	for (auto& ch : lang) ch = static_cast<wchar_t>(std::towlower(ch));
	static const std::map<std::wstring, std::wstring> types = {
		{L"sql", L"sql"}, {L"tsql", L"sql"}, {L"plsql", L"sql"},
		{L"cs", L"csharp"}, {L"csharp", L"csharp"}, {L"c#", L"csharp"},
		{L"py", L"python"}, {L"python", L"python"},
		{L"js", L"jScript"}, {L"javascript", L"jScript"}, {L"jsx", L"jScript"}, {L"mjs", L"jScript"},
		{L"ts", L"jScript"}, {L"typescript", L"jScript"}, {L"tsx", L"jScript"},
		{L"json", L"json"}, {L"jsonc", L"json"},
		{L"ps", L"powershell"}, {L"ps1", L"powershell"}, {L"powershell", L"powershell"}, {L"pwsh", L"powershell"},
		{L"yaml", L"yaml"}, {L"yml", L"yaml"},
		{L"xml", L"xml"}, {L"xaml", L"xml"}, {L"csproj", L"xml"}, {L"svg", L"xml"},
		{L"html", L"html"}, {L"htm", L"html"},
		{L"sh", L"shell"}, {L"bash", L"shell"}, {L"shell", L"shell"}, {L"zsh", L"shell"},
		{L"cpp", L"cpp"}, {L"c++", L"cpp"}, {L"cc", L"cpp"}, {L"cxx", L"cpp"}, {L"hpp", L"cpp"},
		{L"c", L"c"}, {L"h", L"c"},
		{L"java", L"java"}, {L"css", L"css"},
		{L"bat", L"Batch"}, {L"cmd", L"Batch"}, {L"batch", L"Batch"},
		{L"md", L"markdown"}, {L"markdown", L"markdown"},
		{L"diff", L"diff"}, {L"patch", L"diff"},
		{L"go", L"go"}, {L"golang", L"go"},
	};
	const auto it = types.find(lang);
	return it == types.end() ? std::wstring() : it->second;
}

namespace {

constexpr int NoWrap = 1 << 20;

struct Seg { std::wstring text; unsigned style; int src; };
using Segs = std::vector<Seg>;
struct Line { Segs segs; int src = -1; };

int Width(const Segs& segs)
{
	int w = 0;
	for (const auto& s : segs) w += static_cast<int>(s.text.size());
	return w;
}

std::wstring Attr(const MD_ATTRIBUTE& a) { return a.text ? std::wstring(a.text, a.size) : std::wstring(); }

std::wstring Entity(const std::wstring& e)
{
	static const std::map<std::wstring, std::wstring> named = {
		{L"&amp;", L"&"}, {L"&lt;", L"<"}, {L"&gt;", L">"}, {L"&quot;", L"\""},
		{L"&apos;", L"'"}, {L"&nbsp;", L" "}, {L"&copy;", L"©"}, {L"&mdash;", L"—"}, {L"&ndash;", L"–"},
	};
	if (const auto it = named.find(e); it != named.end()) return it->second;
	if (e.size() > 3 && e[1] == L'#')
	{
		const bool hex = e[2] == L'x' || e[2] == L'X';
		const unsigned long cp = std::wcstoul(e.c_str() + (hex ? 3 : 2), nullptr, hex ? 16 : 10);
		if (cp > 0 && cp < 0x10000) return std::wstring(1, static_cast<wchar_t>(cp));
	}
	return e;
}

// Splits segments into units (runs without whitespace) and places them on
// lines of at most `avail` columns.
std::vector<Line> Layout(const Segs& segs, int avail)
{
	struct Piece { std::wstring text; unsigned style; int src; };
	struct Unit { std::vector<Piece> pieces; bool spaceBefore = false; bool breakBefore = false; unsigned spaceStyle = 0; };

	std::vector<Unit> units;
	bool space = false, brk = false, glue = false;
	unsigned spaceStyle = 0;
	for (const auto& s : segs)
	{
		size_t i = 0;
		while (i < s.text.size())
		{
			const wchar_t ch = s.text[i];
			if (ch == L'\n') { brk = true; space = false; glue = false; ++i; continue; }
			if (ch == L' ' || ch == L'\t') { space = true; spaceStyle = s.style; glue = false; ++i; continue; }
			size_t j = i;
			while (j < s.text.size() && s.text[j] != L' ' && s.text[j] != L'\t' && s.text[j] != L'\n') ++j;
			Piece p{s.text.substr(i, j - i), s.style, s.src};
			if (glue && !units.empty()) units.back().pieces.push_back(std::move(p));
			else
			{
				Unit u;
				u.spaceBefore = space;
				u.breakBefore = brk;
				u.spaceStyle = spaceStyle & s.style;
				u.pieces.push_back(std::move(p));
				units.push_back(std::move(u));
			}
			space = brk = false;
			glue = true;
			i = j;
		}
	}

	std::vector<Line> out;
	Line cur;
	int len = 0;
	auto newline = [&] { out.push_back(std::move(cur)); cur = Line{}; len = 0; };
	auto put = [&](const std::wstring& text, unsigned style, int src)
	{
		if (cur.src < 0) cur.src = src;
		if (!cur.segs.empty() && cur.segs.back().style == style) cur.segs.back().text += text;
		else cur.segs.push_back({text, style, src});
		len += static_cast<int>(text.size());
	};

	for (const auto& u : units)
	{
		int w = 0;
		for (const auto& p : u.pieces) w += static_cast<int>(p.text.size());
		if (u.breakBefore && (len > 0 || !out.empty())) newline();
		const int need = (u.spaceBefore && len > 0 ? 1 : 0) + w;
		if (len > 0 && len + need > avail) newline();
		if (u.spaceBefore && len > 0) put(L" ", u.spaceStyle, u.pieces.front().src);
		for (const auto& p : u.pieces)
		{
			std::wstring rest = p.text;
			while (len + static_cast<int>(rest.size()) > avail && avail > 0)
			{
				const int room = avail - len;
				if (room <= 0) { newline(); continue; }
				put(rest.substr(0, room), p.style, p.src);
				rest.erase(0, room);
				newline();
			}
			if (!rest.empty()) put(rest, p.style, p.src);
		}
	}
	if (!cur.segs.empty() || out.empty()) out.push_back(std::move(cur));
	return out;
}

struct Container
{
	enum Kind { Quote, List, Item, Note } kind;
	explicit Container(Kind k) : kind(k) {}
	bool tight = false;          // List
	bool ordered = false;        // List
	unsigned next = 1;           // List: next item number
	bool started = false;        // List: first item has begun
	bool blankBefore = true;     // List: blank line before the first item
	std::wstring marker;         // Item
	bool markerShown = false;    // Item
	bool firstChild = true;      // Item: no block started yet
};

class Builder
{
public:
	Builder(const std::wstring& source, const RenderOptions& options) : src_(source), opt_(options)
	{
		starts_.push_back(0);
		for (size_t i = 0; i < source.size(); ++i)
			if (source[i] == L'\n') starts_.push_back(i + 1);
	}

	Rendered Build()
	{
		size_t body = FrontMatter();
		MD_PARSER parser{};
		parser.abi_version = 0;
		parser.flags = MD_DIALECT_GITHUB | MD_FLAG_WIKILINKS | MD_FLAG_HIGHLIGHT;
		parser.enter_block = [](MD_BLOCKTYPE t, void* d, void* u) { return static_cast<Builder*>(u)->EnterBlock(t, d); };
		parser.leave_block = [](MD_BLOCKTYPE t, void* d, void* u) { return static_cast<Builder*>(u)->LeaveBlock(t, d); };
		parser.enter_span = [](MD_SPANTYPE t, void* d, void* u) { return static_cast<Builder*>(u)->EnterSpan(t, d); };
		parser.leave_span = [](MD_SPANTYPE t, void* d, void* u) { return static_cast<Builder*>(u)->LeaveSpan(t, d); };
		parser.text = [](MD_TEXTTYPE t, const MD_CHAR* s, MD_SIZE n, void* u) { return static_cast<Builder*>(u)->Text(t, s, n); };
		md_parse(src_.data() + body, static_cast<MD_SIZE>(src_.size() - body), &parser, this);
		if (out_.lines.empty()) Emit({}, 0);
		return std::move(out_);
	}

private:
	enum class Mode { None, Para, Implicit, Heading, Code, Html, Cell };

	const std::wstring& src_;
	RenderOptions opt_;
	std::vector<size_t> starts_;
	Rendered out_;
	int cur_ = 0;                       // last source line seen
	std::vector<Container> stack_;
	Mode mode_ = Mode::None;
	Segs segs_;                         // inline content of the open leaf block
	std::vector<unsigned> spans_;       // style of each open span
	std::vector<std::wstring> hrefs_;   // open link targets
	unsigned level_ = 0;                // heading level
	bool noBlank_ = false;              // suppress the blank line before the next block
	// code and HTML blocks
	std::wstring lang_;
	std::vector<std::pair<std::wstring, int>> code_;
	// tables
	struct Cell { Segs segs; MD_ALIGN align; bool head; };
	std::vector<std::vector<Cell>> rows_;
	bool inHead_ = false;
	MD_ALIGN align_ = MD_ALIGN_DEFAULT;

	int Avail() const { return (opt_.wrap ? opt_.width : NoWrap) - PrefixWidth(); }
	int FrameWidth() const { return std::max(opt_.width - PrefixWidth(), 10); }

	int LineOf(const wchar_t* p)
	{
		if (p < src_.data() || p >= src_.data() + src_.size()) return cur_;
		const size_t off = static_cast<size_t>(p - src_.data());
		cur_ = static_cast<int>(std::upper_bound(starts_.begin(), starts_.end(), off) - starts_.begin()) - 1;
		return cur_;
	}

	unsigned SpanStyle() const
	{
		unsigned s = 0;
		for (unsigned x : spans_) s |= x;
		return s;
	}

	int PrefixWidth() const
	{
		int w = 0;
		for (const auto& c : stack_)
			if (c.kind == Container::Quote || c.kind == Container::Note) w += 2;
			else if (c.kind == Container::Item) w += static_cast<int>(c.marker.size());
		return w;
	}

	Segs Prefix(bool markers)
	{
		Segs p;
		for (auto& c : stack_)
		{
			if (c.kind == Container::Quote || c.kind == Container::Note)
				p.push_back({L"│ ", c.kind == Container::Note ? S_TITLE : S_DIM, cur_});
			else if (c.kind == Container::Item)
			{
				if (markers && !c.markerShown) { p.push_back({c.marker, S_STRONG, cur_}); c.markerShown = true; }
				else p.push_back({std::wstring(c.marker.size(), L' '), 0, cur_});
			}
		}
		return p;
	}

	void Emit(Segs body, int src)
	{
		Segs line = Prefix(true);
		line.insert(line.end(), body.begin(), body.end());
		std::wstring text;
		std::vector<Run> runs;
		for (const auto& s : line)
		{
			const int start = static_cast<int>(text.size());
			text += s.text;
			if (s.style && !s.text.empty())
			{
				if (!runs.empty() && runs.back().end == start && runs.back().style == s.style) runs.back().end = static_cast<int>(text.size());
				else runs.push_back({start, static_cast<int>(text.size()), s.style});
			}
		}
		out_.lines.push_back(std::move(text));
		out_.runs.push_back(std::move(runs));
		out_.sourceLine.push_back(src < 0 ? cur_ : src);
	}

	void Blank()
	{
		Segs bars;
		for (const auto& c : stack_)
			if (c.kind == Container::Quote || c.kind == Container::Note)
				bars.push_back({L"│", c.kind == Container::Note ? S_TITLE : S_DIM, cur_});
			else if (c.kind == Container::Item)
				bars.push_back({std::wstring(c.marker.size(), L' '), 0, cur_});
		while (!bars.empty() && bars.back().style == 0) bars.pop_back();
		std::wstring text;
		std::vector<Run> runs;
		for (size_t i = 0; i < bars.size(); ++i)
		{
			if (i) text += L' ';
			const int start = static_cast<int>(text.size());
			text += bars[i].text;
			if (bars[i].style) runs.push_back({start, static_cast<int>(text.size()), bars[i].style});
		}
		out_.lines.push_back(std::move(text));
		out_.runs.push_back(std::move(runs));
		out_.sourceLine.push_back(cur_);
	}

	// Called when a block is about to produce output: emits the blank line
	// that separates it from the previous block.
	void StartBlock()
	{
		bool blank = true;
		if (!stack_.empty() && stack_.back().kind == Container::Item)
		{
			Container& item = stack_.back();
			Container& list = stack_[stack_.size() - 2];
			if (item.firstChild)
			{
				item.firstChild = false;
				blank = list.started ? !list.tight : list.blankBefore;
				list.started = true;
			}
			else blank = !list.tight;
		}
		if (noBlank_) { blank = false; noBlank_ = false; }
		if (blank && !out_.lines.empty()) Blank();
	}

	void EmitLines(const std::vector<Line>& lines)
	{
		for (const auto& l : lines) Emit(l.segs, l.src);
	}

	void FlushInline()
	{
		if (mode_ != Mode::Implicit && mode_ != Mode::Para) return;
		if (!segs_.empty())
		{
			StartBlock();
			EmitLines(Layout(segs_, Avail()));
		}
		segs_.clear();
		mode_ = Mode::None;
	}

	void Add(std::wstring text, unsigned style, int src)
	{
		if (mode_ == Mode::None) mode_ = Mode::Implicit;
		segs_.push_back({std::move(text), style, src});
	}

	void FrameLine(const std::wstring& label, int src)
	{
		const int w = FrameWidth();
		std::wstring t = label.empty() ? std::wstring() : L"── " + label + L" ";
		if (static_cast<int>(t.size()) < w) t += std::wstring(w - t.size(), L'─');
		Emit({{t, S_DIM, src}}, src);
	}

	void EmitCode(bool html)
	{
		while (!code_.empty() && code_.back().first.empty() && code_.back().second < 0) code_.pop_back();
		StartBlock();
		const std::wstring type = html ? std::wstring() : ColorerType(lang_);
		const unsigned style = html ? S_DIM : (type.empty() ? S_CODE : 0);
		const int first = code_.empty() ? cur_ : code_.front().second;
		if (!html) FrameLine(type.empty() ? (lang_.empty() ? L"code" : lang_) : type, first);
		const int avail = Avail();
		for (auto& [line, src] : code_)
		{
			std::wstring rest;
			for (wchar_t ch : line)
				if (ch == L'\t') rest.append(4 - rest.size() % 4, L' ');
				else if (ch != L'\r') rest += ch;
			bool firstPart = true;
			do
			{
				const int room = std::max(firstPart ? avail : avail - 2, 1);
				size_t take = std::min(rest.size(), static_cast<size_t>(room));
				if (take < rest.size())
				{
					// prefer to break after a space in the second half of the line
					const size_t sp = rest.rfind(L' ', take - 1);
					if (sp != std::wstring::npos && sp + 1 > take / 2) take = sp + 1;
				}
				std::wstring part = rest.substr(0, take);
				rest.erase(0, part.size());
				Segs segs;
				if (!firstPart) segs.push_back({L"» ", S_DIM, src});
				segs.push_back({part, style, src});
				Emit(segs, src);
				firstPart = false;
			} while (!rest.empty());
		}
		if (!html) FrameLine(L"", code_.empty() ? cur_ : code_.back().second);
		code_.clear();
		lang_.clear();
	}

	void EmitTable()
	{
		StartBlock();
		size_t n = 0;
		for (const auto& r : rows_) n = std::max(n, r.size());
		if (n == 0) return;
		std::vector<int> colw(n, 1);
		for (const auto& r : rows_)
			for (size_t i = 0; i < r.size(); ++i) colw[i] = std::max(colw[i], Width(r[i].segs));
		const int sep = 3;
		int total = sep * static_cast<int>(n - 1);
		for (int w : colw) total += w;
		if (opt_.wrap)
			while (total > Avail())
			{
				auto widest = std::max_element(colw.begin(), colw.end());
				if (*widest <= 6) break;
				--*widest;
				--total;
			}

		for (size_t r = 0; r < rows_.size(); ++r)
		{
			std::vector<std::vector<Line>> cells(n);
			size_t height = 1;
			for (size_t i = 0; i < n; ++i)
			{
				Segs segs = i < rows_[r].size() ? rows_[r][i].segs : Segs{};
				if (i < rows_[r].size() && rows_[r][i].head)
					for (auto& s : segs) s.style |= S_TITLE | S_STRONG;
				cells[i] = Layout(segs, colw[i]);
				height = std::max(height, cells[i].size());
			}
			for (size_t k = 0; k < height; ++k)
			{
				Segs line;
				int src = -1;
				for (size_t i = 0; i < n; ++i)
				{
					Segs cell = k < cells[i].size() ? cells[i][k].segs : Segs{};
					if (src < 0 && k < cells[i].size()) src = cells[i][k].src;
					const int pad = colw[i] - Width(cell);
					const MD_ALIGN a = i < rows_[r].size() ? rows_[r][i].align : MD_ALIGN_DEFAULT;
					const int left = a == MD_ALIGN_RIGHT ? pad : a == MD_ALIGN_CENTER ? pad / 2 : 0;
					if (left > 0) line.push_back({std::wstring(left, L' '), 0, src});
					line.insert(line.end(), cell.begin(), cell.end());
					if (i + 1 < n)
					{
						if (pad - left > 0) line.push_back({std::wstring(pad - left, L' '), 0, src});
						line.push_back({L" │ ", S_DIM, src});
					}
				}
				Emit(line, src);
			}
			if (r == 0 && !rows_[0].empty() && rows_[0][0].head)
			{
				std::wstring rule;
				for (size_t i = 0; i < n; ++i)
				{
					rule += std::wstring(colw[i], L'─');
					if (i + 1 < n) rule += L"─┼─";
				}
				Emit({{rule, S_DIM, out_.sourceLine.back()}}, out_.sourceLine.back());
			}
		}
		rows_.clear();
	}

	size_t FrontMatter()
	{
		auto lineAt = [&](size_t i)
		{
			const size_t b = starts_[i];
			size_t e = i + 1 < starts_.size() ? starts_[i + 1] : src_.size();
			while (e > b && (src_[e - 1] == L'\n' || src_[e - 1] == L'\r')) --e;
			return src_.substr(b, e - b);
		};
		if (starts_.size() < 2 || lineAt(0) != L"---") return 0;
		for (size_t i = 1; i < starts_.size(); ++i)
		{
			const std::wstring l = lineAt(i);
			if (l != L"---" && l != L"...") continue;
			for (size_t k = 0; k <= i; ++k) code_.push_back({lineAt(k), static_cast<int>(k)});
			EmitCode(true);
			cur_ = static_cast<int>(i);
			return i + 1 < starts_.size() ? starts_[i + 1] : src_.size();
		}
		return 0;
	}

	int EnterBlock(MD_BLOCKTYPE type, void* detail)
	{
		if (type != MD_BLOCK_DOC && type != MD_BLOCK_TBODY && type != MD_BLOCK_THEAD && type != MD_BLOCK_TR
			&& type != MD_BLOCK_TH && type != MD_BLOCK_TD)
			FlushInline();
		switch (type)
		{
		case MD_BLOCK_QUOTE:
			StartBlock();
			noBlank_ = true;
			stack_.push_back(Container(Container::Quote));
			break;
		case MD_BLOCK_ADMONITION:
		{
			StartBlock();
			stack_.push_back(Container(Container::Note));
			std::wstring title = Attr(static_cast<MD_BLOCK_ADMONITION_DETAIL*>(detail)->type);
			if (!title.empty()) title[0] = static_cast<wchar_t>(std::towupper(title[0]));
			Emit({{title, S_TITLE | S_STRONG, cur_}}, cur_);
			noBlank_ = true;
			break;
		}
		case MD_BLOCK_UL:
		case MD_BLOCK_OL:
		case MD_BLOCK_FOOTNOTE_DEF_SECTION:
		{
			Container list(Container::List);
			if (type == MD_BLOCK_UL) list.tight = static_cast<MD_BLOCK_UL_DETAIL*>(detail)->is_tight != 0;
			else if (type == MD_BLOCK_OL)
			{
				const auto* d = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
				list.tight = d->is_tight != 0;
				list.ordered = true;
				list.next = d->start;
			}
			else
			{
				StartBlock();
				FrameLine(L"", cur_);
				noBlank_ = true;
				list.tight = true;
			}
			list.blankBefore = !(!stack_.empty() && stack_.back().kind == Container::Item
				&& stack_[stack_.size() - 2].tight);
			stack_.push_back(list);
			break;
		}
		case MD_BLOCK_LI:
		case MD_BLOCK_FOOTNOTE_DEF:
		{
			Container& list = stack_.back();
			Container item(Container::Item);
			if (type == MD_BLOCK_FOOTNOTE_DEF)
				item.marker = L"[" + std::to_wstring(static_cast<MD_BLOCK_FOOTNOTE_DEF_DETAIL*>(detail)->id) + L"] ";
			else
			{
				item.marker = list.ordered ? std::to_wstring(list.next++) + L". " : L"• ";
				const auto* d = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
				if (d->is_task) item.marker += d->task_mark == L' ' ? L"[ ] " : L"[x] ";
			}
			stack_.push_back(item);
			break;
		}
		case MD_BLOCK_HR:
			StartBlock();
			FrameLine(L"", cur_);
			break;
		case MD_BLOCK_H:
			mode_ = Mode::Heading;
			level_ = static_cast<MD_BLOCK_H_DETAIL*>(detail)->level;
			break;
		case MD_BLOCK_CODE:
			mode_ = Mode::Code;
			lang_ = Attr(static_cast<MD_BLOCK_CODE_DETAIL*>(detail)->lang);
			code_.clear();
			break;
		case MD_BLOCK_HTML:
			mode_ = Mode::Html;
			code_.clear();
			break;
		case MD_BLOCK_P:
			mode_ = Mode::Para;
			break;
		case MD_BLOCK_TABLE:
			rows_.clear();
			break;
		case MD_BLOCK_THEAD: inHead_ = true; break;
		case MD_BLOCK_TBODY: inHead_ = false; break;
		case MD_BLOCK_TR: rows_.emplace_back(); break;
		case MD_BLOCK_TH:
		case MD_BLOCK_TD:
			mode_ = Mode::Cell;
			segs_.clear();
			align_ = static_cast<MD_BLOCK_TD_DETAIL*>(detail)->align;
			break;
		default: break;
		}
		return 0;
	}

	int LeaveBlock(MD_BLOCKTYPE type, void*)
	{
		switch (type)
		{
		case MD_BLOCK_QUOTE:
		case MD_BLOCK_ADMONITION:
		case MD_BLOCK_UL:
		case MD_BLOCK_OL:
		case MD_BLOCK_FOOTNOTE_DEF_SECTION:
			FlushInline();
			stack_.pop_back();
			break;
		case MD_BLOCK_LI:
		case MD_BLOCK_FOOTNOTE_DEF:
			FlushInline();
			if (stack_.back().firstChild) StartBlock();   // empty item: still show its marker
			if (!stack_.back().markerShown) Emit({}, cur_);
			stack_.pop_back();
			break;
		case MD_BLOCK_P:
			FlushInline();
			break;
		case MD_BLOCK_H:
		{
			StartBlock();
			const unsigned style = (level_ <= 2 ? S_H1 : S_H) | S_STRONG;
			for (auto& s : segs_) s.style |= style;
			const auto lines = Layout(segs_, Avail());
			out_.headings.push_back(static_cast<int>(out_.lines.size()));
			int widest = 0;
			for (const auto& l : lines) widest = std::max(widest, Width(l.segs));
			EmitLines(lines);
			if (level_ <= 2)
				Emit({{std::wstring(std::max(widest, 1), level_ == 1 ? L'═' : L'─'), S_H1, -1}}, lines.back().src);
			segs_.clear();
			mode_ = Mode::None;
			break;
		}
		case MD_BLOCK_CODE: EmitCode(false); mode_ = Mode::None; break;
		case MD_BLOCK_HTML: EmitCode(true); mode_ = Mode::None; break;
		case MD_BLOCK_TH:
		case MD_BLOCK_TD:
			rows_.back().push_back({segs_, align_, type == MD_BLOCK_TH || inHead_});
			segs_.clear();
			mode_ = Mode::None;
			break;
		case MD_BLOCK_TABLE: EmitTable(); break;
		default: break;
		}
		return 0;
	}

	int EnterSpan(MD_SPANTYPE type, void* detail)
	{
		unsigned style = 0;
		switch (type)
		{
		case MD_SPAN_EM: case MD_SPAN_U: style = S_EM; break;
		case MD_SPAN_STRONG: style = S_STRONG; break;
		case MD_SPAN_CODE: case MD_SPAN_LATEXMATH: case MD_SPAN_LATEXMATH_DISPLAY: style = S_CODE; break;
		case MD_SPAN_DEL: style = S_DEL; break;
		case MD_SPAN_MARK: style = S_MARK; break;
		case MD_SPAN_WIKILINK: style = S_LINK; break;
		case MD_SPAN_A:
		{
			const auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
			hrefs_.push_back(d->is_autolink ? std::wstring() : Attr(d->href));
			style = S_LINK;
			break;
		}
		case MD_SPAN_IMG:
			hrefs_.push_back(Attr(static_cast<MD_SPAN_IMG_DETAIL*>(detail)->src));
			Add(L"[image: ", S_DIM, cur_);
			style = S_DIM;
			break;
		case MD_SPAN_FOOTNOTE_REF:
			Add(L"[" + std::to_wstring(static_cast<MD_SPAN_FOOTNOTE_REF_DETAIL*>(detail)->id) + L"]", S_LINK, cur_);
			break;
		default: break;
		}
		spans_.push_back(style);
		return 0;
	}

	int LeaveSpan(MD_SPANTYPE type, void*)
	{
		spans_.pop_back();
		if (type == MD_SPAN_IMG) Add(L"]", S_DIM, cur_);
		if (type == MD_SPAN_A || type == MD_SPAN_IMG)
		{
			const std::wstring href = hrefs_.back();
			hrefs_.pop_back();
			if (opt_.showUrls && !href.empty()) Add(L" (" + href + L")", S_DIM, cur_);
		}
		return 0;
	}

	int Text(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size)
	{
		const int src = LineOf(text);
		std::wstring s(text, size);
		if (mode_ == Mode::Code || mode_ == Mode::Html)
		{
			if (code_.empty()) code_.push_back({L"", src});
			for (wchar_t ch : s)
				if (ch == L'\n') code_.push_back({L"", -1});
				else
				{
					if (code_.back().second < 0) code_.back().second = src;
					code_.back().first += ch;
				}
			return 0;
		}
		switch (type)
		{
		case MD_TEXT_BR: Add(L"\n", 0, src); break;
		case MD_TEXT_SOFTBR: Add(L" ", SpanStyle(), src); break;
		case MD_TEXT_NULLCHAR: Add(L"�", SpanStyle(), src); break;
		case MD_TEXT_ENTITY: Add(Entity(s), SpanStyle(), src); break;
		case MD_TEXT_HTML: Add(s, S_DIM, src); break;
		default: Add(s, SpanStyle(), src); break;
		}
		return 0;
	}
};

} // namespace

Rendered Render(const std::wstring& source, const RenderOptions& options)
{
	return Builder(source, options).Build();
}

} // namespace markfar

// MarkFar: Markdown preview for Far Manager 3.
// Spike build: opens a fixed sample in a locked editor to prove that MarkFar's
// own colours and Colorer's code-block colours work together (docs/002).

#include <windows.h>
#include <plugin.hpp>
#include <farcolor.hpp>

#include <map>
#include <string>
#include <vector>

namespace {

// {68FB4233-2F10-4C2F-8B08-14DDD4EE68F5}
constexpr GUID MainGuid = {0x68fb4233, 0x2f10, 0x4c2f, {0x8b, 0x08, 0x14, 0xdd, 0xd4, 0xee, 0x68, 0xf5}};
// {23D3C8BC-7DEB-477B-B623-A6B4BAB2FF2D}
constexpr GUID MenuGuid = {0x23d3c8bc, 0x7deb, 0x477b, {0xb6, 0x23, 0xa6, 0xb4, 0xba, 0xb2, 0xff, 0x2d}};

// ponytail: priority picked for the spike; tune once Colorer's own priority is known.
constexpr uintptr_t ColorPriority = EDITOR_COLOR_NORMAL_PRIORITY + 1;

PluginStartupInfo Far;
FarStandardFunctions Fsf;

struct LineColor { intptr_t line, start, end; FarColor color; };

std::map<std::wstring, std::vector<LineColor>> Pending;  // temp file -> colours to apply
std::map<intptr_t, std::vector<LineColor>> Views;         // editor id -> colours applied

std::wstring ToLower(std::wstring s) { CharLowerBuffW(s.data(), static_cast<DWORD>(s.size())); return s; }

FarColor EditorText()
{
	FarColor c{};
	Far.AdvControl(&MainGuid, ACTL_GETCOLOR, COL_EDITORTEXT, &c);
	return c;
}

FarColor Accent(FarColor base, int index, FARCOLORFLAGS style)
{
	base.Flags = (base.Flags & ~FCF_FG_INDEX) | FCF_FG_INDEX | style;
	base.ForegroundColor = 0xFF000000 | index;
	return base;
}

int WindowWidth()
{
	SMALL_RECT r{};
	Far.AdvControl(&MainGuid, ACTL_GETFARRECT, 0, &r);
	return r.Right - r.Left + 1;
}

// Splits text into lines of at most width characters at spaces; a continued
// line starts with prefix.
void Wrap(const std::wstring& text, size_t width, const std::wstring& prefix, std::vector<std::wstring>& out)
{
	std::wstring rest = text;
	bool first = true;
	while (true)
	{
		std::wstring lead = first ? L"" : prefix;
		size_t room = width > lead.size() ? width - lead.size() : 1;
		if (rest.size() <= room) { out.push_back(lead + rest); return; }
		size_t cut = rest.rfind(L' ', room);
		if (cut == std::wstring::npos || cut == 0) cut = room;
		out.push_back(lead + rest.substr(0, cut));
		rest = rest.substr(cut);
		if (!rest.empty() && rest[0] == L' ') rest.erase(0, 1);
		first = false;
	}
}

std::wstring SpikeText(const std::wstring& source, size_t width, std::vector<LineColor>& colors)
{
	std::vector<std::wstring> lines;
	const FarColor base = EditorText();

	lines.push_back(L"MarkFar spike");
	colors.push_back({0, 0, 12, Accent(base, 14, FCF_FG_BOLD)});
	lines.push_back(std::wstring(13, L'═'));
	colors.push_back({1, 0, 12, Accent(base, 14, FCF_NONE)});
	lines.push_back(L"");
	Wrap(L"Source: " + source + L". The heading above is coloured by MarkFar; the SQL block "
		L"below is coloured by Colorer through markfar.hrc. Its long SELECT line is wrapped "
		L"to the window width, and the continuation must stay coloured as SQL.", width, L"", lines);
	lines.push_back(L"");

	const intptr_t bold = static_cast<intptr_t>(lines.size());
	lines.push_back(L"Bold, italic and struck-out text use Far's font styles.");
	colors.push_back({bold, 0, 3, Accent(base, 15, FCF_FG_BOLD)});
	colors.push_back({bold, 6, 11, Accent(base, 15, FCF_FG_ITALIC)});
	colors.push_back({bold, 17, 26, Accent(base, 8, FCF_FG_STRIKEOUT)});
	lines.push_back(L"");

	lines.push_back(L"── sql " + std::wstring(width > 8 ? width - 8 : 4, L'─'));
	Wrap(L"SELECT c.Id, c.Name, c.Email, p.PolicyNumber, p.StartDate, p.EndDate, p.Premium "
		L"FROM Customers c JOIN Policies p ON p.CustomerId = c.Id WHERE p.Status = 'Active' "
		L"AND p.EndDate > GETDATE() ORDER BY p.EndDate; -- comment", width, L"» ", lines);
	lines.push_back(L"  -- a short comment line");
	lines.push_back(L"UPDATE Policies SET Status = 'Expired' WHERE EndDate < GETDATE();");
	lines.push_back(std::wstring(width > 1 ? width - 1 : 1, L'─'));
	lines.push_back(L"");

	// Glyph test: the heading underline (═) did not show in the first spike run.
	lines.push_back(L"Glyph test, four lines: ═ plain, ═ yellow, ─ yellow, = yellow:");
	lines.push_back(std::wstring(13, L'═'));
	for (wchar_t ch : {L'═', L'─', L'='})
	{
		colors.push_back({static_cast<intptr_t>(lines.size()), 0, 12, Accent(base, 14, FCF_NONE)});
		lines.push_back(std::wstring(13, ch));
	}
	lines.push_back(L"");
	lines.push_back(L"End of sample.");

	std::wstring text;
	for (auto& l : lines) { text += l; text += L"\r\n"; }
	return text;
}

std::wstring TempPath(const std::wstring& source)
{
	wchar_t dir[MAX_PATH];
	GetTempPathW(MAX_PATH, dir);
	std::wstring folder = std::wstring(dir) + L"MarkFar";
	CreateDirectoryW(folder.c_str(), nullptr);
	const size_t slash = source.find_last_of(L"\\/");
	return folder + L"\\" + source.substr(slash == std::wstring::npos ? 0 : slash + 1) + L".mfview";
}

bool WriteUtf8(const std::wstring& path, const std::wstring& text)
{
	const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	std::string bytes(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), bytes.data(), size, nullptr, nullptr);
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	const bool ok = WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
	CloseHandle(h);
	return ok;
}

void OpenPreview(const std::wstring& source)
{
	std::vector<LineColor> colors;
	// Far's editor keeps one column for the cursor past the end of a line.
	const std::wstring text = SpikeText(source, static_cast<size_t>(WindowWidth() - 1), colors);
	const std::wstring temp = TempPath(source);
	if (!WriteUtf8(temp, text))
	{
		const wchar_t* msg[] = {L"MarkFar", L"Cannot write the preview file:", temp.c_str()};
		Far.Message(&MainGuid, nullptr, FMSG_WARNING | FMSG_MB_OK, nullptr, msg, 3, 0);
		return;
	}
	Pending[ToLower(temp)] = std::move(colors);
	const std::wstring title = L"MarkFar: " + source;
	Far.Editor(temp.c_str(), title.c_str(), 0, 0, -1, -1,
		EF_NONMODAL | EF_IMMEDIATERETURN | EF_LOCKED | EF_DELETEONLYFILEONCLOSE | EF_DISABLEHISTORY | EF_DISABLESAVEPOS,
		1, 1, CP_UTF8);
}

std::wstring Unquote(std::wstring s)
{
	while (!s.empty() && s.front() == L' ') s.erase(0, 1);
	while (!s.empty() && s.back() == L' ') s.pop_back();
	if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') s = s.substr(1, s.size() - 2);
	return s;
}

std::wstring CurrentPanelFile()
{
	const intptr_t dirSize = Far.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, nullptr);
	std::vector<char> dirBuf(dirSize);
	auto* dir = reinterpret_cast<FarPanelDirectory*>(dirBuf.data());
	dir->StructSize = sizeof(FarPanelDirectory);
	Far.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, dirSize, dir);

	const intptr_t itemSize = Far.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, nullptr);
	std::vector<char> itemBuf(itemSize);
	FarGetPluginPanelItem get{sizeof(FarGetPluginPanelItem), static_cast<size_t>(itemSize), reinterpret_cast<PluginPanelItem*>(itemBuf.data())};
	Far.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &get);

	std::wstring path = dir->Name;
	if (!path.empty() && path.back() != L'\\') path += L'\\';
	return path + get.Item->FileName;
}

std::wstring CurrentEditorFile()
{
	const intptr_t size = Far.EditorControl(-1, ECTL_GETFILENAME, 0, nullptr);
	std::wstring name(size, L'\0');
	Far.EditorControl(-1, ECTL_GETFILENAME, size, name.data());
	name.resize(wcslen(name.c_str()));
	return name;
}

void ApplyColors(intptr_t id, const std::vector<LineColor>& colors)
{
	for (const auto& c : colors)
	{
		EditorColor ec{sizeof(EditorColor), c.line, 0, c.start, c.end, ColorPriority, ECF_NONE, c.color, MainGuid};
		Far.EditorControl(id, ECTL_ADDCOLOR, 0, &ec);
	}
}

} // namespace

extern "C" {

void WINAPI GetGlobalInfoW(GlobalInfo* info)
{
	info->StructSize = sizeof(GlobalInfo);
	info->MinFarVersion = FARMANAGERVERSION;
	info->Version = MAKEFARVERSION(0, 1, 0, 0, VS_ALPHA);
	info->Guid = MainGuid;
	info->Title = L"MarkFar";
	info->Description = L"Markdown preview";
	info->Author = L"vdasus";
}

void WINAPI SetStartupInfoW(const PluginStartupInfo* info)
{
	Far = *info;
	Fsf = *info->FSF;
	Far.FSF = &Fsf;
}

void WINAPI GetPluginInfoW(PluginInfo* info)
{
	static const wchar_t* menu[] = {L"MarkFar"};
	info->StructSize = sizeof(PluginInfo);
	info->Flags = PF_EDITOR;
	info->PluginMenu.Guids = &MenuGuid;
	info->PluginMenu.Strings = menu;
	info->PluginMenu.Count = 1;
	info->CommandPrefix = L"markfar";
}

HANDLE WINAPI OpenW(const OpenInfo* info)
{
	std::wstring file;
	switch (info->OpenFrom)
	{
	case OPEN_COMMANDLINE: file = Unquote(reinterpret_cast<const OpenCommandLineInfo*>(info->Data)->CommandLine); break;
	case OPEN_PLUGINSMENU: file = CurrentPanelFile(); break;
	case OPEN_EDITOR: file = CurrentEditorFile(); break;
	default: return nullptr;
	}
	if (!file.empty()) OpenPreview(file);
	return nullptr;
}

intptr_t WINAPI ProcessEditorEventW(const ProcessEditorEventInfo* info)
{
	if (info->Event == EE_CLOSE) { Views.erase(info->EditorID); return 0; }
	if (info->Event != EE_REDRAW || Pending.empty() || Views.contains(info->EditorID)) return 0;

	auto it = Pending.find(ToLower(CurrentEditorFile()));
	if (it == Pending.end()) return 0;
	ApplyColors(info->EditorID, it->second);
	Views[info->EditorID] = std::move(it->second);
	Pending.erase(it);
	return 0;
}

} // extern "C"

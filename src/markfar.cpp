// MarkFar: Markdown preview for Far Manager 3.
// The preview is a locked Far editor on a temporary *.mfview file; MarkFar
// colours it, Colorer colours its code blocks through markfar.hrc. F6 switches
// between the preview and the real file in Far's editor (docs/004).

#include <windows.h>
#include <plugin.hpp>
#include <farcolor.hpp>

#include "render.hpp"
#include "version.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace markfar;

namespace {

// {68FB4233-2F10-4C2F-8B08-14DDD4EE68F5}
constexpr GUID MainGuid = {0x68fb4233, 0x2f10, 0x4c2f, {0x8b, 0x08, 0x14, 0xdd, 0xd4, 0xee, 0x68, 0xf5}};
// {23D3C8BC-7DEB-477B-B623-A6B4BAB2FF2D}
constexpr GUID MenuGuid = {0x23d3c8bc, 0x7deb, 0x477b, {0xb6, 0x23, 0xa6, 0xb4, 0xba, 0xb2, 0xff, 0x2d}};
// {3D573BD2-1176-4237-9C5D-6A22E2AAB3D4}
constexpr GUID ConfigGuid = {0x3d573bd2, 0x1176, 0x4237, {0x9c, 0x5d, 0x6a, 0x22, 0xe2, 0xaa, 0xb3, 0xd4}};
// {3A290698-DD8B-4800-B6D9-EC84560F5F91}
constexpr GUID DialogGuid = {0x3a290698, 0xdd8b, 0x4800, {0xb6, 0xd9, 0xec, 0x84, 0x56, 0x0f, 0x5f, 0x91}};

// FarColorer, whose settings MarkFar adjusts so that Colorer finds markfar.hrc.
constexpr GUID ColorerGuid = {0xd2f36b62, 0xa470, 0x418d, {0x83, 0xa3, 0xed, 0x7a, 0x37, 0x10, 0xe5, 0xb5}};
constexpr wchar_t ColorerKey[] = L"D2F36B62-A470-418D-83A3-ED7A3710E5B5";

// Above Colorer, so MarkFar's colours win where both colour the same text.
constexpr uintptr_t ColorPriority = EDITOR_COLOR_NORMAL_PRIORITY + 1;

// Order matches the .lng files.
enum MsgId
{
	MTitle,
	MConfigTitle,
	MWrap,
	MShowUrls,
	MF6InEditor,
	MOk,
	MCancel,
	MCannotRead,
	MCannotWrite,
	MKeyWrap,
	MKeySource,
	MKeyPreview,
	MF3,
	MColorer,
	MColorerFailed,
};

PluginStartupInfo Far;
FarStandardFunctions Fsf;

struct Options
{
	bool wrap = true;
	bool showUrls = true;
	bool f6InEditor = true;
	bool f3 = true;              // F3 on a Markdown file in a panel opens the preview
	bool colorer = true;         // register markfar.hrc with Colorer
} Opt;

struct View
{
	intptr_t id = -1;            // editor id, -1 until the editor shows up
	std::wstring source;         // the .md file
	std::wstring temp;           // the .mfview file
	std::wstring text;           // Markdown the view was rendered from
	bool wrap = true;
	int width = 0;               // text width the view was rendered for
	int topSource = 0;           // source line to show at the top once opened
	bool stale = false;          // text changed: re-render when the view is current
	Rendered r;
};

std::vector<View> Views;
bool Busy = false;               // re-rendering: ignore our own editor events

const wchar_t* Msg(MsgId id) { return Far.GetMsg(&MainGuid, id); }

std::wstring Lower(std::wstring s)
{
	CharLowerBuffW(s.data(), static_cast<DWORD>(s.size()));
	return s;
}

bool SamePath(const std::wstring& a, const std::wstring& b) { return Lower(a) == Lower(b); }

View* FindById(intptr_t id)
{
	for (auto& v : Views) if (v.id == id) return &v;
	return nullptr;
}

View* FindBySource(const std::wstring& source)
{
	for (auto& v : Views) if (SamePath(v.source, source)) return &v;
	return nullptr;
}

View* FindByTemp(const std::wstring& temp)
{
	for (auto& v : Views) if (v.id < 0 && SamePath(v.temp, temp)) return &v;
	return nullptr;
}

void ShowError(MsgId id, const std::wstring& path)
{
	const wchar_t* lines[] = {Msg(MTitle), Msg(id), path.c_str()};
	Far.Message(&MainGuid, nullptr, FMSG_WARNING | FMSG_MB_OK, nullptr, lines, 3, 0);
}

// --- settings -------------------------------------------------------------

class Settings
{
public:
	Settings()
	{
		FarSettingsCreate fsc{sizeof(FarSettingsCreate), MainGuid, INVALID_HANDLE_VALUE};
		if (Far.SettingsControl(INVALID_HANDLE_VALUE, SCTL_CREATE, PSL_ROAMING, &fsc)) handle_ = fsc.Handle;
	}
	~Settings() { if (handle_ != INVALID_HANDLE_VALUE) Far.SettingsControl(handle_, SCTL_FREE, 0, nullptr); }

	bool Get(const wchar_t* name, bool def)
	{
		FarSettingsItem item{sizeof(FarSettingsItem), 0, name, FST_QWORD, {}};
		if (handle_ == INVALID_HANDLE_VALUE || !Far.SettingsControl(handle_, SCTL_GET, 0, &item)) return def;
		return item.Number != 0;
	}

	void Set(const wchar_t* name, bool value)
	{
		FarSettingsItem item{sizeof(FarSettingsItem), 0, name, FST_QWORD, {}};
		item.Number = value ? 1 : 0;
		if (handle_ != INVALID_HANDLE_VALUE) Far.SettingsControl(handle_, SCTL_SET, 0, &item);
	}

private:
	HANDLE handle_ = INVALID_HANDLE_VALUE;
};

void LoadOptions()
{
	Settings s;
	Opt.wrap = s.Get(L"WordWrap", true);
	Opt.showUrls = s.Get(L"ShowUrls", true);
	Opt.f6InEditor = s.Get(L"F6InEditor", true);
	Opt.f3 = s.Get(L"F3", true);
	Opt.colorer = s.Get(L"Colorer", true);
}

void SaveOptions()
{
	Settings s;
	s.Set(L"WordWrap", Opt.wrap);
	s.Set(L"ShowUrls", Opt.showUrls);
	s.Set(L"F6InEditor", Opt.f6InEditor);
	s.Set(L"F3", Opt.f3);
	s.Set(L"Colorer", Opt.colorer);
}

// --- setup: F3 macro and Colorer --------------------------------------

std::wstring PluginDir()
{
	std::wstring dir = Far.ModuleName;
	dir.resize(dir.find_last_of(L'\\'));
	return dir;
}

std::wstring CurrentPanelFile();
bool MacroAdded = false;
constexpr intptr_t SetupEvent = -1;

bool IsMarkdown(const std::wstring& file);

// Condition of the F3 macro: a Markdown file under the cursor in a file panel.
intptr_t WINAPI F3Condition(void*, FARADDKEYMACROFLAGS)
{
	PanelInfo pi{sizeof(PanelInfo)};
	if (!Far.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &pi)) return 0;
	if (pi.PanelType != PTYPE_FILEPANEL || (pi.Flags & PFLAGS_PLUGIN)) return 0;
	return IsMarkdown(CurrentPanelFile()) ? 1 : 0;
}

// The macro lives only in memory: MarkFar adds it on every start of Far
// (PF_PRELOAD) and removes it when the setting is switched off.
void SetupF3(bool enable)
{
	static int id;   // the macro's identity: its address
	if (enable == MacroAdded) return;
	if (enable)
	{
		MacroAddMacro m{sizeof(MacroAddMacro), &id, L"Plugin.Call(\"68FB4233-2F10-4C2F-8B08-14DDD4EE68F5\", \"F3\")",
			L"MarkFar: F3 opens the preview of a Markdown file", KMFLAGS_LUA, {}, MACROAREA_SHELL, F3Condition, 0};
		m.AKey.EventType = KEY_EVENT;
		m.AKey.Event.KeyEvent.bKeyDown = TRUE;
		m.AKey.Event.KeyEvent.wRepeatCount = 1;
		m.AKey.Event.KeyEvent.wVirtualKeyCode = VK_F3;
		MacroAdded = Far.MacroControl(&MainGuid, MCTL_ADDMACRO, 0, &m) != 0;
	}
	else
	{
		Far.MacroControl(&MainGuid, MCTL_DELMACRO, 0, &id);
		MacroAdded = false;
	}
}

// Points Colorer's "Custom schemas folder (hrc)" at the plugin's hrc folder.
// If the user already has a custom folder, copies markfar.hrc into it instead.
// Returns true when Colorer's settings changed (Colorer must reload them).
bool SetupColorer(bool enable)
{
	FarSettingsCreate fsc{sizeof(FarSettingsCreate), ColorerGuid, INVALID_HANDLE_VALUE};
	if (!Far.SettingsControl(INVALID_HANDLE_VALUE, SCTL_CREATE, PSL_ROAMING, &fsc)) return false;
	const HANDLE h = fsc.Handle;
	FarSettingsValue sub{sizeof(FarSettingsValue), 0, ColorerKey};
	intptr_t key = Far.SettingsControl(h, SCTL_OPENSUBKEY, 0, &sub);
	if (!key) key = Far.SettingsControl(h, SCTL_CREATESUBKEY, 0, &sub);

	FarSettingsItem item{sizeof(FarSettingsItem), static_cast<size_t>(key), L"UserHrcPath", FST_STRING, {}};
	std::wstring current;
	if (key && Far.SettingsControl(h, SCTL_GET, 0, &item) && item.String) current = item.String;
	const std::wstring ours = PluginDir() + L"\\hrc";

	bool changed = false;
	auto set = [&](const wchar_t* value)
	{
		FarSettingsItem w{sizeof(FarSettingsItem), static_cast<size_t>(key), L"UserHrcPath", FST_STRING, {}};
		w.String = value;
		changed = key && Far.SettingsControl(h, SCTL_SET, 0, &w);
	};
	if (enable)
	{
		if (current.empty()) set(ours.c_str());
		else if (Lower(current) != Lower(ours)
			&& !CopyFileW((ours + L"\\markfar.hrc").c_str(), (current + L"\\markfar.hrc").c_str(), FALSE))
		{
			const wchar_t* lines[] = {Msg(MTitle), Msg(MColorerFailed), current.c_str()};
			Far.Message(&MainGuid, nullptr, FMSG_WARNING | FMSG_MB_OK, nullptr, lines, 3, 0);
		}
	}
	else
	{
		if (Lower(current) == Lower(ours)) set(L"");
		else if (!current.empty()) DeleteFileW((current + L"\\markfar.hrc").c_str());
	}
	Far.SettingsControl(h, SCTL_FREE, 0, nullptr);
	return changed;
}

void ReloadColorer()
{
	MacroSendMacroText m{sizeof(MacroSendMacroText), KMFLAGS_LUA, {},
		L"if Plugin.Exist(\"D2F36B62-A470-418D-83A3-ED7A3710E5B5\") then Plugin.Call(\"D2F36B62-A470-418D-83A3-ED7A3710E5B5\", \"Settings\", \"Reload\") end"};
	Far.MacroControl(&MainGuid, MCTL_SENDSTRING, MSSC_POST, &m);
}

// Brings the F3 macro and the Colorer registration in line with the settings.
void ApplySetup()
{
	SetupF3(Opt.f3);
	if (SetupColorer(Opt.colorer)) ReloadColorer();
}

// --- colours --------------------------------------------------------------

FarColor Palette(PaletteColors index)
{
	FarColor c{};
	Far.AdvControl(&MainGuid, ACTL_GETCOLOR, index, &c);
	return c;
}

FarColor WithForeground(FarColor base, const FarColor& from)
{
	base.Flags = (base.Flags & ~FCF_FG_INDEX) | (from.Flags & FCF_FG_INDEX);
	base.ForegroundColor = from.ForegroundColor;
	return base;
}

FarColor WithIndex(FarColor base, int index)
{
	base.Flags |= FCF_FG_INDEX;
	base.ForegroundColor = 0xFF000000 | index;
	return base;
}

// Headings and emphasis take their colours from Far's panel palette, so they
// follow the active colour scheme; the background is the editor's.
FarColor ColorFor(unsigned style)
{
	const FarColor text = Palette(COL_EDITORTEXT);
	FarColor c = text;
	if (style & S_MARK)
	{
		c = Palette(COL_EDITORSELECTEDTEXT);
	}
	else if (style & S_H1) c = WithForeground(text, Palette(COL_PANELSELECTEDTEXT));
	else if (style & (S_H | S_TITLE)) c = WithForeground(text, Palette(COL_PANELHIGHLIGHTTEXT));
	else if (style & S_CODE) c = WithIndex(text, 10);     // light green
	else if (style & S_LINK) c = WithIndex(text, 13);     // light magenta
	else if (style & (S_DIM | S_DEL)) c = WithIndex(text, 8);   // dark grey
	else if (style & (S_STRONG | S_EM)) c = WithForeground(text, Palette(COL_PANELHIGHLIGHTTEXT));

	if (style & (S_STRONG | S_H1 | S_H)) c.Flags |= FCF_FG_BOLD;
	if (style & S_EM) c.Flags |= FCF_FG_ITALIC;
	if (style & S_DEL) c.Flags |= FCF_FG_STRIKEOUT;
	return c;
}

void ApplyColors(const View& v)
{
	for (size_t line = 0; line < v.r.runs.size(); ++line)
		for (const auto& run : v.r.runs[line])
		{
			EditorColor ec{sizeof(EditorColor), static_cast<intptr_t>(line), 0, run.start, run.end - 1,
				ColorPriority, ECF_NONE, ColorFor(run.style), MainGuid};
			Far.EditorControl(v.id, ECTL_ADDCOLOR, 0, &ec);
		}
}

// --- files ----------------------------------------------------------------

bool ReadSource(const std::wstring& path, std::wstring& text)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	LARGE_INTEGER size{};
	GetFileSizeEx(h, &size);
	std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
	DWORD read = 0;
	const bool ok = ReadFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
	CloseHandle(h);
	if (!ok) return false;

	if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE)
	{
		text.assign(reinterpret_cast<const wchar_t*>(bytes.data() + 2), (bytes.size() - 2) / 2);
		return true;
	}
	size_t skip = 0;
	if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) skip = 3;
	const char* data = bytes.data() + skip;
	const int len = static_cast<int>(bytes.size() - skip);
	UINT cp = CP_UTF8;
	int n = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS, data, len, nullptr, 0);
	if (n == 0 && len > 0) { cp = CP_ACP; n = MultiByteToWideChar(cp, 0, data, len, nullptr, 0); }
	text.assign(n, L'\0');
	MultiByteToWideChar(cp, 0, data, len, text.data(), n);
	return true;
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

std::wstring FileName(const std::wstring& path)
{
	const size_t slash = path.find_last_of(L"\\/");
	return path.substr(slash == std::wstring::npos ? 0 : slash + 1);
}

std::wstring TempPath(const std::wstring& source)
{
	wchar_t dir[MAX_PATH];
	GetTempPathW(MAX_PATH, dir);
	std::wstring folder = std::wstring(dir) + L"MarkFar";
	CreateDirectoryW(folder.c_str(), nullptr);
	unsigned hash = 2166136261u;   // FNV-1a: one temp file per source path
	for (wchar_t ch : Lower(source)) hash = (hash ^ ch) * 16777619u;
	wchar_t tag[16];
	Fsf.snprintf(tag, 16, L".%08x", hash);
	return folder + L"\\" + FileName(source) + tag + L".mfview";
}

std::wstring Join(const std::vector<std::wstring>& lines)
{
	std::wstring text;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (i) text += L"\r\n";
		text += lines[i];
	}
	return text;
}

// --- Far windows and editors ----------------------------------------------

int ScreenWidth()
{
	SMALL_RECT r{};
	Far.AdvControl(&MainGuid, ACTL_GETFARRECT, 0, &r);
	return r.Right - r.Left + 1;
}

// Far's editor keeps one column for the cursor past the end of a line.
int TextWidth(int windowWidth) { return std::max(windowWidth - 1, 20); }

EditorInfo GetEditorInfo(intptr_t id)
{
	EditorInfo ei{sizeof(EditorInfo)};
	Far.EditorControl(id, ECTL_GETINFO, 0, &ei);
	return ei;
}

std::wstring EditorFile(intptr_t id)
{
	const intptr_t size = Far.EditorControl(id, ECTL_GETFILENAME, 0, nullptr);
	if (size <= 0) return {};
	std::wstring name(size, L'\0');
	Far.EditorControl(id, ECTL_GETFILENAME, size, name.data());
	name.resize(wcslen(name.c_str()));
	return name;
}

std::wstring EditorText(intptr_t id)
{
	const EditorInfo ei = GetEditorInfo(id);
	std::wstring text;
	for (intptr_t i = 0; i < ei.TotalLines; ++i)
	{
		EditorGetString egs{sizeof(EditorGetString), i};
		Far.EditorControl(id, ECTL_GETSTRING, 0, &egs);
		if (i) text += L'\n';
		text.append(egs.StringText, egs.StringLength);
	}
	return text;
}

void SetTop(intptr_t id, intptr_t line)
{
	EditorSetPosition esp{sizeof(EditorSetPosition), line, 0, -1, line, 0, -1};
	Far.EditorControl(id, ECTL_SETPOSITION, 0, &esp);
}

// Brings an open editor of `file` to the front, or opens one.
void OpenEditor(const std::wstring& file, const wchar_t* title, EDITOR_FLAGS extra, intptr_t line, uintptr_t codePage)
{
	Far.Editor(file.c_str(), title, 0, 0, -1, -1, EF_NONMODAL | EF_IMMEDIATERETURN | extra, line, line < 0 ? -1 : 1, codePage);
}

// Source line to show once the source editor of `path` gets the focus.
std::wstring PendingSource;
int PendingSourceLine = 0;

void SetKeyBar(intptr_t id, bool preview)
{
	KeyBarLabel preview_labels[] = {
		{{VK_F2, 0}, Msg(MKeyWrap), Msg(MKeyWrap)},
		{{VK_F6, 0}, Msg(MKeySource), Msg(MKeySource)},
	};
	KeyBarLabel source_labels[] = {
		{{VK_F6, 0}, Msg(MKeyPreview), Msg(MKeyPreview)},
	};
	KeyBarTitles titles{};
	if (preview) { titles.CountLabels = std::size(preview_labels); titles.Labels = preview_labels; }
	else { titles.CountLabels = std::size(source_labels); titles.Labels = source_labels; }
	FarSetKeyBarTitles set{sizeof(FarSetKeyBarTitles), &titles};
	Far.EditorControl(id, ECTL_SETKEYBAR, 0, &set);
}

bool IsMarkdown(const std::wstring& file)
{
	const std::wstring f = Lower(file);
	for (const wchar_t* ext : {L".md", L".markdown", L".mdown", L".mkd"})
	{
		const size_t n = wcslen(ext);
		if (f.size() > n && f.compare(f.size() - n, n, ext) == 0) return true;
	}
	return false;
}

// --- the preview ----------------------------------------------------------

void RenderView(View& v)
{
	RenderOptions o;
	o.width = v.width;
	o.wrap = v.wrap;
	o.showUrls = Opt.showUrls;
	v.r = Render(v.text, o);
}

intptr_t RenderedLineOf(const View& v, int sourceLine)
{
	for (size_t i = 0; i < v.r.sourceLine.size(); ++i)
		if (v.r.sourceLine[i] >= sourceLine) return static_cast<intptr_t>(i);
	return v.r.sourceLine.empty() ? 0 : static_cast<intptr_t>(v.r.sourceLine.size() - 1);
}

int SourceLineOf(const View& v, intptr_t line)
{
	if (v.r.sourceLine.empty()) return 0;
	return v.r.sourceLine[std::clamp<intptr_t>(line, 0, v.r.sourceLine.size() - 1)];
}

// Renders the view again (new text, width or wrap) inside the same editor
// window and keeps the source line at the top of the screen.
void Rerender(View& v, int topSource)
{
	Busy = true;
	const EditorInfo ei = GetEditorInfo(v.id);
	v.width = TextWidth(static_cast<int>(ei.WindowSizeX));
	RenderView(v);

	EditorSetParameter p{sizeof(EditorSetParameter), ESPT_LOCKMODE};
	p.iParam = 0;
	Far.EditorControl(v.id, ECTL_SETPARAM, 0, &p);
	EditorSetParameter ai{sizeof(EditorSetParameter), ESPT_AUTOINDENT};
	ai.iParam = 0;
	Far.EditorControl(v.id, ECTL_SETPARAM, 0, &ai);

	SetTop(v.id, 0);
	for (intptr_t i = 1; i < ei.TotalLines; ++i) Far.EditorControl(v.id, ECTL_DELETESTRING, 0, nullptr);
	EditorSetString empty{sizeof(EditorSetString), 0, 0, L"", nullptr};
	Far.EditorControl(v.id, ECTL_SETSTRING, 0, &empty);
	std::wstring text;
	for (size_t i = 0; i < v.r.lines.size(); ++i)
	{
		if (i) text += L'\n';
		text += v.r.lines[i];
	}
	Far.EditorControl(v.id, ECTL_INSERTTEXT, 0, const_cast<wchar_t*>(text.c_str()));

	EditorSaveFile save{sizeof(EditorSaveFile), nullptr, nullptr, CP_UTF8};
	Far.EditorControl(v.id, ECTL_SAVEFILE, 0, &save);   // clears "modified": no save prompt on close
	ai.iParam = (ei.Options & EOPT_AUTOINDENT) ? 1 : 0;
	Far.EditorControl(v.id, ECTL_SETPARAM, 0, &ai);
	p.iParam = 1;
	Far.EditorControl(v.id, ECTL_SETPARAM, 0, &p);

	ApplyColors(v);
	SetTop(v.id, RenderedLineOf(v, topSource));
	Far.EditorControl(v.id, ECTL_REDRAW, 0, nullptr);
	Busy = false;
}

// Opens the preview of `source`; `text` is its Markdown (from disk or from
// an open editor), `topSource` the source line to show at the top.
void OpenPreview(const std::wstring& source, const std::wstring& text, int topSource)
{
	if (View* v = FindBySource(source); v && v->id >= 0)
	{
		// Far changes only the current editor: bring the preview to the front,
		// re-render once it is current (EE_GOTFOCUS / ProcessSynchroEventW).
		v->text = text;
		v->topSource = topSource;
		v->stale = true;
		OpenEditor(v->temp, nullptr, EF_OPENMODE_USEEXISTING, -1, CP_UTF8);
		Far.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, reinterpret_cast<void*>(v->id));
		return;
	}

	View v;
	v.source = source;
	v.temp = TempPath(source);
	v.text = text;
	v.wrap = Opt.wrap;
	v.width = TextWidth(ScreenWidth());
	v.topSource = topSource;
	RenderView(v);
	if (!WriteUtf8(v.temp, Join(v.r.lines)))
	{
		ShowError(MCannotWrite, v.temp);
		return;
	}
	std::erase_if(Views, [&](const View& old) { return old.id < 0 && SamePath(old.temp, v.temp); });
	const std::wstring title = std::wstring(Msg(MTitle)) + L": " + FileName(source);
	const std::wstring temp = v.temp;
	const intptr_t startLine = RenderedLineOf(v, topSource) + 1;
	Views.push_back(std::move(v));
	Far.Editor(temp.c_str(), title.c_str(), 0, 0, -1, -1,
		EF_NONMODAL | EF_IMMEDIATERETURN | EF_LOCKED | EF_DELETEONLYFILEONCLOSE | EF_DISABLEHISTORY | EF_DISABLESAVEPOS,
		startLine, 1, CP_UTF8);
}

void OpenFile(const std::wstring& path)
{
	std::wstring text;
	if (!ReadSource(path, text)) { ShowError(MCannotRead, path); return; }
	OpenPreview(path, text, 0);
}

void SwitchToSource(View& v)
{
	const EditorInfo ei = GetEditorInfo(v.id);
	const int line = SourceLineOf(v, ei.TopScreenLine);
	PendingSource = v.source;
	PendingSourceLine = line;
	OpenEditor(v.source, nullptr, EF_OPENMODE_USEEXISTING, line + 1, CP_DEFAULT);
}

void JumpHeading(View& v, bool forward)
{
	const EditorInfo ei = GetEditorInfo(v.id);
	const auto& h = v.r.headings;
	intptr_t target = -1;
	if (forward)
	{
		for (int line : h) if (line > ei.CurLine) { target = line; break; }
	}
	else
	{
		for (auto it = h.rbegin(); it != h.rend(); ++it) if (*it < ei.CurLine) { target = *it; break; }
	}
	if (target < 0) return;
	EditorSetPosition esp{sizeof(EditorSetPosition), target, 0, -1, target, 0, -1};
	Far.EditorControl(v.id, ECTL_SETPOSITION, 0, &esp);
	Far.EditorControl(v.id, ECTL_REDRAW, 0, nullptr);
}

std::wstring Unquote(std::wstring s)
{
	while (!s.empty() && s.front() == L' ') s.erase(0, 1);
	while (!s.empty() && s.back() == L' ') s.pop_back();
	if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') s = s.substr(1, s.size() - 2);
	return s;
}

std::wstring FullPath(const std::wstring& path)
{
	const DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
	if (n == 0) return path;
	std::wstring full(n, L'\0');
	full.resize(GetFullPathNameW(path.c_str(), n, full.data(), nullptr));
	return full;
}

std::wstring CurrentPanelFile()
{
	const intptr_t dirSize = Far.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, nullptr);
	if (dirSize <= 0) return {};
	std::vector<char> dirBuf(dirSize);
	auto* dir = reinterpret_cast<FarPanelDirectory*>(dirBuf.data());
	dir->StructSize = sizeof(FarPanelDirectory);
	Far.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, dirSize, dir);

	const intptr_t itemSize = Far.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, nullptr);
	if (itemSize <= 0) return {};
	std::vector<char> itemBuf(itemSize);
	FarGetPluginPanelItem get{sizeof(FarGetPluginPanelItem), static_cast<size_t>(itemSize), reinterpret_cast<PluginPanelItem*>(itemBuf.data())};
	Far.PanelControl(PANEL_ACTIVE, FCTL_GETCURRENTPANELITEM, 0, &get);
	if (get.Item->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) return {};

	std::wstring path = dir->Name;
	if (!path.empty() && path.back() != L'\\') path += L'\\';
	return path + get.Item->FileName;
}

bool Configure()
{
	const int w = 64, h = 11;
	FarDialogItem items[] = {
		{DI_DOUBLEBOX, 3, 1, w - 4, h - 2, {}, nullptr, nullptr, DIF_NONE, Msg(MConfigTitle)},
		{DI_CHECKBOX, 5, 2, 0, 2, {Opt.wrap}, nullptr, nullptr, DIF_NONE, Msg(MWrap)},
		{DI_CHECKBOX, 5, 3, 0, 3, {Opt.showUrls}, nullptr, nullptr, DIF_NONE, Msg(MShowUrls)},
		{DI_CHECKBOX, 5, 4, 0, 4, {Opt.f6InEditor}, nullptr, nullptr, DIF_NONE, Msg(MF6InEditor)},
		{DI_CHECKBOX, 5, 5, 0, 5, {Opt.f3}, nullptr, nullptr, DIF_NONE, Msg(MF3)},
		{DI_CHECKBOX, 5, 6, 0, 6, {Opt.colorer}, nullptr, nullptr, DIF_NONE, Msg(MColorer)},
		{DI_TEXT, -1, 7, 0, 7, {}, nullptr, nullptr, DIF_SEPARATOR, L""},
		{DI_BUTTON, 0, 8, 0, 8, {}, nullptr, nullptr, DIF_CENTERGROUP | DIF_DEFAULTBUTTON, Msg(MOk)},
		{DI_BUTTON, 0, 8, 0, 8, {}, nullptr, nullptr, DIF_CENTERGROUP, Msg(MCancel)},
	};
	HANDLE dlg = Far.DialogInit(&MainGuid, &DialogGuid, -1, -1, w, h, L"Settings", items, std::size(items), 0, FDLG_NONE, nullptr, nullptr);
	if (dlg == INVALID_HANDLE_VALUE) return false;
	const bool ok = Far.DialogRun(dlg) == 7;
	if (ok)
	{
		auto checked = [&](int i) { return Far.SendDlgMessage(dlg, DM_GETCHECK, i, nullptr) == BSTATE_CHECKED; };
		Opt.wrap = checked(1);
		Opt.showUrls = checked(2);
		Opt.f6InEditor = checked(3);
		Opt.f3 = checked(4);
		Opt.colorer = checked(5);
		SaveOptions();
		ApplySetup();
	}
	Far.DialogFree(dlg);
	return ok;
}

bool IsKey(const INPUT_RECORD& rec, WORD vk, DWORD mods)
{
	if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown || rec.Event.KeyEvent.wVirtualKeyCode != vk) return false;
	const DWORD state = rec.Event.KeyEvent.dwControlKeyState;
	const DWORD ctrl = state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
	const DWORD alt = state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED);
	const DWORD shift = state & SHIFT_PRESSED;
	return (ctrl != 0) == ((mods & LEFT_CTRL_PRESSED) != 0) && (alt != 0) == ((mods & LEFT_ALT_PRESSED) != 0)
		&& (shift != 0) == ((mods & SHIFT_PRESSED) != 0);
}

} // namespace

extern "C" {

void WINAPI GetGlobalInfoW(GlobalInfo* info)
{
	info->StructSize = sizeof(GlobalInfo);
	info->MinFarVersion = FARMANAGERVERSION;
	info->Version = MAKEFARVERSION(MARKFAR_VERSION_MAJOR, MARKFAR_VERSION_MINOR, MARKFAR_VERSION_PATCH, 0, VS_RELEASE);
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
	LoadOptions();
	// Macros and Colorer are ready only once Far is up: finish setup from
	// ProcessSynchroEventW (Param SetupEvent; editor ids are 0 and up).
	Far.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, reinterpret_cast<void*>(SetupEvent));
}

void WINAPI GetPluginInfoW(PluginInfo* info)
{
	static const wchar_t* menu[1];
	static const wchar_t* config[1];
	menu[0] = Msg(MTitle);
	config[0] = Msg(MTitle);
	info->StructSize = sizeof(PluginInfo);
	info->Flags = PF_EDITOR | PF_PRELOAD;
	info->PluginMenu.Guids = &MenuGuid;
	info->PluginMenu.Strings = menu;
	info->PluginMenu.Count = 1;
	info->PluginConfig.Guids = &ConfigGuid;
	info->PluginConfig.Strings = config;
	info->PluginConfig.Count = 1;
	info->CommandPrefix = L"markfar";
}

HANDLE WINAPI OpenW(const OpenInfo* info)
{
	switch (info->OpenFrom)
	{
	case OPEN_COMMANDLINE:
	{
		const std::wstring file = Unquote(reinterpret_cast<const OpenCommandLineInfo*>(info->Data)->CommandLine);
		if (!file.empty()) OpenFile(FullPath(file));
		break;
	}
	case OPEN_PLUGINSMENU:
		if (const std::wstring file = CurrentPanelFile(); !file.empty()) OpenFile(file);
		break;
	case OPEN_FROMMACRO:
	{
		const auto* mi = reinterpret_cast<const OpenMacroInfo*>(info->Data);
		if (mi && mi->Count > 0 && mi->Values[0].Type == FMVT_STRING && std::wstring(mi->Values[0].String) == L"F3")
			if (const std::wstring file = CurrentPanelFile(); !file.empty()) OpenFile(file);
		break;
	}
	case OPEN_EDITOR:
	{
		const EditorInfo ei = GetEditorInfo(-1);
		if (View* v = FindById(ei.EditorID)) SwitchToSource(*v);
		else OpenPreview(EditorFile(-1), EditorText(-1), static_cast<int>(ei.TopScreenLine));
		break;
	}
	default: break;
	}
	return nullptr;
}

intptr_t WINAPI ConfigureW(const ConfigureInfo*)
{
	return Configure();
}

intptr_t WINAPI ProcessEditorEventW(const ProcessEditorEventInfo* info)
{
	if (Busy) return 0;
	switch (info->Event)
	{
	case EE_SAVE:
	case EE_CLOSE:
		// The source of an open preview was saved or closed: the preview shows
		// the new text when it is next in front (Esc from the source leads there).
		if (!FindById(info->EditorID))
		{
			const std::wstring file = EditorFile(info->EditorID);
			if (View* v = FindBySource(file); v && v->id >= 0)
			{
				std::wstring text;
				if (info->Event == EE_SAVE) text = EditorText(info->EditorID);
				else if (!ReadSource(file, text)) break;
				v->text = std::move(text);
				v->topSource = static_cast<int>(GetEditorInfo(info->EditorID).TopScreenLine);
				v->stale = true;
			}
		}
		if (info->Event == EE_CLOSE)
			std::erase_if(Views, [&](const View& x) { return x.id == info->EditorID; });
		break;
	case EE_GOTFOCUS:
		if (View* v = FindById(info->EditorID))
		{
			SetKeyBar(v->id, true);
			if (v->stale) Far.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, reinterpret_cast<void*>(v->id));
		}
		else
		{
			const std::wstring file = EditorFile(info->EditorID);
			if (Opt.f6InEditor && IsMarkdown(file)) SetKeyBar(info->EditorID, false);
			if (!PendingSource.empty() && SamePath(file, PendingSource))
			{
				SetTop(info->EditorID, PendingSourceLine);
				PendingSource.clear();
			}
		}
		break;
	case EE_REDRAW:
	{
		if (View* v = FindById(info->EditorID))
		{
			const EditorInfo ei = GetEditorInfo(v->id);
			if (TextWidth(static_cast<int>(ei.WindowSizeX)) != v->width)
				Far.AdvControl(&MainGuid, ACTL_SYNCHRO, 0, reinterpret_cast<void*>(v->id));
			return 0;
		}
		if (Views.empty()) return 0;
		if (View* v = FindByTemp(EditorFile(info->EditorID)))
		{
			v->id = info->EditorID;
			SetKeyBar(v->id, true);
			ApplyColors(*v);
			SetTop(v->id, RenderedLineOf(*v, v->topSource));
		}
		break;
	}
	default: break;
	}
	return 0;
}

intptr_t WINAPI ProcessSynchroEventW(const ProcessSynchroEventInfo* info)
{
	if (info->Event != SE_COMMONSYNCHRO) return 0;
	const intptr_t id = reinterpret_cast<intptr_t>(info->Param);
	if (id == SetupEvent) { ApplySetup(); return 0; }
	View* v = FindById(id);
	if (!v || GetEditorInfo(-1).EditorID != id) return 0;
	const EditorInfo ei = GetEditorInfo(id);
	if (v->stale)
	{
		v->stale = false;
		Rerender(*v, v->topSource);
	}
	else if (TextWidth(static_cast<int>(ei.WindowSizeX)) != v->width) Rerender(*v, SourceLineOf(*v, ei.TopScreenLine));
	return 0;
}

intptr_t WINAPI ProcessEditorInputW(const ProcessEditorInputInfo* info)
{
	const INPUT_RECORD& rec = info->Rec;
	if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) return 0;
	const EditorInfo ei = GetEditorInfo(-1);

	if (View* v = FindById(ei.EditorID))
	{
		if (IsKey(rec, VK_F2, 0))
		{
			v->wrap = !v->wrap;
			Rerender(*v, SourceLineOf(*v, ei.TopScreenLine));
			return 1;
		}
		if (IsKey(rec, VK_F6, 0)) { SwitchToSource(*v); return 1; }
		if (IsKey(rec, VK_DOWN, LEFT_CTRL_PRESSED)) { JumpHeading(*v, true); return 1; }
		if (IsKey(rec, VK_UP, LEFT_CTRL_PRESSED)) { JumpHeading(*v, false); return 1; }
		if (IsKey(rec, VK_F1, 0)) { Far.ShowHelp(Far.ModuleName, L"Preview", FHELP_SELFHELP); return 1; }
		return 0;
	}

	if (Opt.f6InEditor && IsKey(rec, VK_F6, 0))
	{
		const std::wstring file = EditorFile(-1);
		if (IsMarkdown(file))
		{
			OpenPreview(file, EditorText(-1), static_cast<int>(ei.TopScreenLine));
			return 1;
		}
	}
	return 0;
}

} // extern "C"

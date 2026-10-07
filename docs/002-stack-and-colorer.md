# 002 — Stack choice and Colorer integration

Status: accepted 2026-10-07.

Follows [001-kickoff](001-kickoff.md). Inputs from the owner (2026-10-07):
minimum dependencies and resource use, nothing outside what is native to Far;
C or C++ is Claude's call; the project closes one personal need and is not
expected to grow, so low maintenance matters more than features; reusing
Colorer for code highlighting is welcome but not a requirement.

## Environment checked

| Item | Value | How checked |
|---|---|---|
| Far Manager | 3.0.6699.0 x64 | `Far.exe` version resource |
| Plugin SDK | 3.0.6699, `PluginSDK\Headers.c\plugin.hpp` | ships with Far |
| FarColorer | installed, `Plugins\FarColorer`, base 1.2.0.99 | plugin folder |
| Compiler | Visual Studio Professional 2026 (18.10), MSVC 14.51 | `vswhere`, `VC\Tools\MSVC` |
| CMake | not found in the Visual Studio folder | file check |

## Decision: stack

- **Language: C++20.** Far's API is wide-character and struct-based;
  `std::wstring`, `std::vector` and RAII remove most of the bookkeeping that C
  would need, with no runtime cost. No exceptions across the plugin boundary,
  no RTTI-dependent code.
- **Compiler: MSVC from Visual Studio 2026**, x64 only (the installed Far is
  x64; an x86 build is one switch away if ever needed).
- **Static runtime (`/MT`).** The result is one DLL with no dependency on the
  Visual C++ Redistributable.
- **Build: a single `build.cmd`** that calls `vcvars64.bat` and `cl.exe`. No
  CMake (not installed), no solution files to maintain. A `.vcxproj` for
  debugging in Visual Studio can be added later if needed.
- **Markdown parser: md4c** (MIT), vendored as source (`md4c.c`, `md4c.h`)
  and compiled into the DLL. CommonMark-compliant, with tables, task lists,
  strikethrough and autolinks; streaming callbacks, no syntax tree to allocate.
- **No other third-party code.**

## Requirement: word wrap

Word wrap is a must-have, not a refinement: the console renderers tried
before failed on exactly this point (001, "Goal"). It can be switched off.

- **Toggle:** `F2` switches wrap on and off in the open view, as `F2` does in
  Far's own viewer. (`F2` normally saves in the editor; the view is locked, so
  MarkFar takes the key over.) With wrap off, long lines keep their length and
  the editor scrolls horizontally as usual.

OK.

- **Default:** a plugin setting "Word wrap" (on by default) in
  `F9 → Options → Plugins configuration → MarkFar` decides how a view opens.

OK.

With wrap on:

- Every line of the rendered view fits the window width; there is no
  horizontal scrolling.

OK.

- Paragraphs wrap at word boundaries; a word longer than the line breaks
  inside the word.

OK.

- List items and block quotes wrap with a hanging indent, so continuation
  lines stay aligned under the text, not under the bullet or `>`.

OK.

- Code blocks wrap too, with a continuation mark at the start of each
  continued line (for example `↪`), so a wrapped code line is not mistaken
  for two lines.

OK.

- Tables: wrap text inside cells when the table is wider than the window
  (details in 001, Q7).

OK.

- When the window size changes, the view is rendered again at the new width
  and keeps the reading position (the same source line stays at the top).

OK.

## Decision: display surface

The rendered document opens in **Far's own editor, locked (read-only)**, and
the plugin colours it through `ECTL_ADDCOLOR`.

- The viewer (F3) cannot be coloured by plugins: its control API
  (`VCTL_GETINFO` … `VCTL_GETFILENAME`) has no colour command. Verified in
  `plugin.hpp`.
- The editor accepts colours from several plugins at once: each
  `EditorColor` carries an `Owner` GUID and a `Priority`
  (`EDITOR_COLOR_NORMAL_PRIORITY` = `0x80000000`). Verified in `plugin.hpp`.
- The editor gives search, selection, copy and bookmarks for free.
- The editor has no soft wrap, so the plugin wraps the rendered text to the
  window width itself and renders again when the window size changes.
- The rendered text goes to a temporary file opened with `EF_LOCKED`,
  `EF_DELETEONCLOSE`, `EF_DISABLEHISTORY` and `EF_DISABLESAVEPOS`, so nothing
  is saved and the file history stays clean.

Rejected: an own full-screen view (`DI_USERCONTROL`). It gives full control
but needs scrolling, search, selection and resize written from scratch, and
Colorer cannot colour it.

## Colorer: what is possible

Findings:

1. **Colorer colours editors only.** It has no API through which another
   plugin hands it a piece of text and gets colours back.
2. **`colorer.dll` is a Far plugin, not a library.** It exports only the Far
   plugin entry points, so MarkFar cannot link against it.
3. **It has a macro API** (`Plugin.Call("D2F36B62-A470-418d-83A3-ED7A3710E5B5", …)`,
   documented in `colorere.hlf`). Among other things, `"Types", "Set", <type>`
   sets the file type of the current editor, and `"Editor", "Refresh"`
   re-colours it. A native plugin can run such a call through `MacroControl`.
4. **Its Markdown scheme does not highlight code by language.**
   `hrc/misc/markdown.hrc` paints a fenced block as one `Code` region whatever
   its language tag.
5. **User schemes are supported.** Colorer loads additional HRC files from a
   user path set in its settings; an HRC scheme can embed other schemes
   (`<inherit scheme="sql:sql"/>`) inside a region.

What this allows — **Colorer colours the code blocks of the rendered view**:

- MarkFar renders code blocks with a recognisable frame, for example a first
  line `── sql ──`.
- MarkFar ships a small `markfar.hrc` that matches such a frame and embeds the
  Colorer scheme of that language for the block body.
- After opening the rendered view, MarkFar sets the editor type to `markfar`
  through the macro API. Colorer then colours every code block in any of its
  languages (about 250), and MarkFar colours headings, emphasis, links and
  quotes itself with a higher priority.

Cost and risks:

- The HRC file has to be registered once in Colorer's user-scheme settings
  (a one-time manual step, or done by MarkFar on first run — *to verify*).
- Two owners colour the same editor. Colorer may paint the default text colour
  over the whole line; MarkFar's colours must then win by priority. Needs a
  prototype (*to verify*).
- `markfar.hrc` lists the languages it maps (`sql`, `csharp`, `json`, …),
  one line per language. Adding a language is one line, not a tokenizer.

Fallback when Colorer is absent or the type cannot be set: code blocks are
shown in one colour, without syntax highlighting. Colorer ships with Far, so
this is the rare case.

**Recommendation:** go with Colorer for code blocks, prove it with a spike
first. Writing and maintaining tokenizers for several languages is the
maintenance burden the owner wants to avoid, and Colorer already covers far
more languages than MarkFar ever would.

## Requirement: help and messages

- Help files ship with the plugin in three languages: English
  (`MarkFarEng.hlf`), Russian (`MarkFarRus.hlf`) and Lithuanian
  (`MarkFarLit.hlf`). Far picks the file that matches its interface language
  and falls back to English. Content: what the plugin does, how to open a
  view, keys (including the wrap toggle), settings, and the Colorer setup.
  `F1` in the view and in the settings dialog opens it.
- All user-visible text (menu item, settings dialog, messages) comes from
  language files in the same three languages: `MarkFarEng.lng`,
  `MarkFarRus.lng`, `MarkFarLit.lng`. Another language is a new pair of
  files, not a code change.
- English is the source; the Russian and Lithuanian files are translations of
  it and change together with it. The Lithuanian text, written by Claude,
  should be read once by a native speaker before a public release.

## Plan

1. **Spike (about one day).** A plugin that opens a hard-coded rendered text
   in a locked editor, colours a heading, and lets Colorer colour one framed
   SQL block through `markfar.hrc`, with one wrapped long line in that block
   (Colorer must keep colouring the continuation line). Answers the two
   *to verify* points.
2. **Version 1.** md4c rendering to word-wrapped text plus colours, re-render on
   resize, wrap toggle and setting, code blocks through Colorer, opening by key
   (see Q3 in 001), help and language files in English, Russian and
   Lithuanian.
3. **Afterwards, if needed.** Tables, links, settings.

## Questions

1. **Build files:** is a plain `build.cmd` enough, or do you want a Visual
   Studio project (`.vcxproj`) to debug the plugin under VS?

`build.cmd` is enough.

2. **Spike first:** agree to spend the first step on the Colorer spike, before
   any Markdown rendering?

Yes.

3. **Still open in 001:** Q1 (rendered vs source), Q3 (how the view opens),
   Q4 (dialect: front matter, `[[wiki-links]]`), Q7 (keys, links, tables),
   Q8 (colours). They do not block the spike.
   Answered in 001, carried into [003-v1-scope](003-v1-scope.md).

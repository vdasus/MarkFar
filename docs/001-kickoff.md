# 001 — Kickoff: scope and open questions

Status: answered 2026-10-07; carried into [002-stack-and-colorer](002-stack-and-colorer.md) and [003-v1-scope](003-v1-scope.md).

This document starts the design discussion. It states the goal, lists the
technical options known so far, and asks the questions whose answers decide
the design. Answer inline under each question (or in a reply file); the next
document turns the answers into a decision.

## Goal

Read Markdown files inside Far Manager 3 as fast as the built-in viewer opens
plain text, but rendered: headings, emphasis, lists, tables, links and code
blocks shown with formatting and colour, with word wrap and normal scrolling
in both directions.

Background: console renderers tried so far (glow 3.0.0 with `-t`) are fast but
awkward inside Far — `-w` is ignored in TUI mode, the left arrow leaves the
document instead of scrolling back, long lines are not wrapped.

## Technical landscape

Facts marked *to verify* come from memory and must be checked against the
current Far 3 SDK and FarNet releases before a decision rests on them.

### Ways to write a Far 3 plugin

| Option | Language | Notes |
|---|---|---|
| Native plugin | C / C++ | Full Far 3 API (`plugin.hpp`). Fastest start-up, no runtime dependency. Separate x86 / x64 builds. Most code to write. |
| FarNet module | C# | FarNet hosts .NET modules inside Far. Rich wrapper API, easy access to NuGet libraries (Markdig, highlighters). Requires FarNet and the .NET 10 runtime (x64) installed (source: github.com/nightroman/FarNet README, 2026-10-07). |
| Lua script | Lua | LuaMacro ships with Far 3. Good for gluing an external renderer to a key; weak for a custom renderer. |

### Ways to show coloured output

The built-in viewer (F3) wraps and scrolls well, but plugins cannot colour
its text (*to verify*). Options:

1. **Built-in viewer, no colour.** Render Markdown to formatted plain text
   (underlined headings, indented lists, framed code) in a temporary file and
   open it in the viewer. Smallest effort; wrap and scroll come for free.
2. **Built-in editor, read-only, with colour.** The editor API lets a plugin
   colour ranges (`ECTL_ADDCOLOR`, the mechanism the Colorer plugin uses). The
   editor has no soft wrap, so the plugin wraps the rendered text to the window
   width itself and re-renders on resize.
3. **Own full-screen view.** A dialog with a user control (`DI_USERCONTROL`)
   that the plugin paints cell by cell. Full control over colour, wrap and keys;
   the plugin implements scrolling, search and resize itself. Most effort.

### Building blocks

- Markdown parsers: Markdig (C#, BSD-2-Clause), md4c (C, MIT), cmark-gfm
  (C, BSD-2-Clause).
- Code-block highlighting: TextMateSharp (C#, TextMate grammars), ColorCode
  (C#), or the HRC schemes of Colorer, which Far users already have.

### Related existing tools

- Colorer (bundled with Far 3) already highlights Markdown **source** in the
  editor (F4). If "syntax highlighting" means coloured source rather than a
  rendered view, part of the need may already be met.

## Questions

### Q1. What should the plugin show?

- (a) Rendered Markdown: no `#`, `**`, backticks; headings, bold, lists and
  tables drawn with formatting and colour.
- (b) The source text with syntax highlighting (like Colorer, but in the
  viewer with wrap).
- (c) Both, switchable with a key.

(c)

### Q2. What does "syntax highlighting" cover?

- Markdown elements only (headings, emphasis, links, quotes)?
- Also the code inside fenced blocks (` ```csharp `, ` ```sql `, ...)? If yes,
  which languages matter most to you?

The main common languages; for me: C#, SQL, Python, ...

### Q3. How is the plugin opened?

- Replace F3 for `*.md` (via file association or by intercepting the viewer)?
- A separate key (for example `Ctrl+Shift+F3`) or a plugin menu item (F11)?
- In quick view (Ctrl+Q panel) as well?

Option 1 is enough; option 2 can be added as well.

### Q4. Which Markdown dialect?

CommonMark plus GitHub extensions (tables, task lists, strikethrough,
autolinks) is the usual baseline. Anything beyond it: front matter (YAML at
the top, as in your wiki pages), `[[wiki-links]]` (Obsidian), mermaid blocks
(shown as source only — a console cannot draw them), footnotes?

The usual baseline, plus Obsidian syntax only if it is cheap.

### Q5. Language and runtime

- C# on FarNet (fastest to write, needs FarNet installed) or native C++ (no
  dependency, more work)? Is FarNet already installed in your Far?
- Which Far 3 build do you run (x64 or x86, version from `Far -?` or the About
  dialog)?

**Answer (2026-10-07):** no FarNet and no .NET. Use what is native to Far and
keep dependencies and resource use to a minimum. FarNet is not installed.

Consequences:

- The plugin is a native C/C++ DLL built against the Far 3 SDK (`plugin.hpp`).
- Third-party code is limited to small libraries compiled into the DLL, with no
  runtime dependencies. Candidate parser: md4c (MIT, one `.c` and one `.h`,
  streaming callbacks, no allocations per node).
- Code-block highlighting: reuse Colorer if it fits (welcome, not required).
- C or C++ and the toolchain are Claude's choice; the machine has Visual
  Studio Professional 2026 and Far 3.0.6699 x64. Decided in
  [002-stack-and-colorer](002-stack-and-colorer.md).

### Q6. Which display approach (see "Ways to show coloured output")?

A reasonable path is to start with option 1 as a working first version within
a day, then move to option 2 or 3 for colour. Do you agree, or should the
first version already have colour?

The first version should have colour.

### Q7. Viewer behaviour

- Keys you expect: arrows, PgUp/PgDn, Home/End, search (F7), toggle wrap,
  switch to source, open in editor (F4), jump between headings?
- Links: ignore, show the URL, or open in the browser on Enter?
- Images: show alt text and path only?
- Very wide tables: wrap cells, or scroll horizontally?
1. OK.
2. Show the URL, with an option to turn it off; when it is off, ignore links.

### Q8. Colours

Fixed palette, follow Far's colour settings, or a configurable theme? Do you
use a dark or a light console?

Follow the Far colour scheme.

### Q9. Distribution and quality bar

- Only for your own use, or published (GitHub releases, Far plugin
  directory / PlugRing)?
- Tests: unit tests for the renderer (xUnit v3 + FluentAssertions 7 +
  NSubstitute + AutoFixture if C#)? A CI build on GitHub Actions?
- Interface language: English only, or also Russian / Lithuanian
  (Far `.lng` files)?
1. Published, free of charge.
2. No tests; a CI build only to create a release on GitHub.
3. English, Russian, Lithuanian.

### Q10. Performance target

What file sizes should open instantly? A guideline: under 100 ms for a 1 MB
file, render lazily beyond that.

OK.

## Next step

Once the questions are answered: a decision document (`002-...`) with the
chosen runtime and display approach, then a minimal working plugin.

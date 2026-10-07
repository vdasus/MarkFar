# 006 — Version 1 build notes

Status: built 2026-10-07, waiting for the owner's check in Far.

What version 1 implements, where it differs from
[003-v1-scope](003-v1-scope.md) and [004-preview-and-edit](004-preview-and-edit.md),
and what to check.

## Implemented

- Rendering with md4c 0.6.0 (UTF-16 build): CommonMark, GitHub extensions
  (tables, task lists, strikethrough, autolinks, footnotes, admonitions
  `> [!NOTE]`), `[[wiki-links]]`, `==highlight==`, YAML front matter shown
  dimmed.
- Word wrap with hanging indents for lists and quotes, wrapped code lines
  with `» `, table cells wrapped inside their columns; `F2` toggles wrap,
  the setting sets the default.
- Re-rendering on window resize, keeping the source line at the top.
- `F6` both ways between preview and source; the preview renders from the
  editor text, saved or not. `F6` in any `.md` editor opens the preview
  (setting). `F11 → MarkFar` in an editor does the same.
- `Ctrl+Down` / `Ctrl+Up` jump between headings.
- Settings dialog (`F9 → Options → Plugins configuration → MarkFar`): word
  wrap, link URLs, `F6` in Markdown editors.
- Help and language files in English, Russian and Lithuanian.
- Release workflow: a `v*` tag builds the plugin on GitHub and attaches
  `MarkFar-<tag>-x64.zip` to a release.

## Colours

Headings and emphasis follow Far's palette on the editor background:

| Element | Colour |
|---|---|
| Headings 1–2 and their underline | foreground of "Panel → Selected text" |
| Headings 3–6, bold, italic, table headers, admonition titles | foreground of "Panel → Highlighted text" |
| `==highlight==` | "Editor → Selected text" |
| Inline code, code blocks without a known language | light green (console colour 10) |
| Links | light magenta (console colour 13) |
| URLs, frames, quote bars, front matter, struck-out text | dark grey (console colour 8) |

The last three have no fitting entry in Far's palette and use fixed console
colours, which still follow the console's colour table. Bold, italic and
strike-out are added as font styles for terminals that render them.

## Differences from the scope

- **Opening a file that is open in an editor.** The preview takes the text
  from that editor, not from disk, so unsaved changes show.
- **Performance.** A 1 MB file renders in roughly 0.2 s (measured with a
  console harness, process start included in 0.5 s) — above the 100 ms
  target in 003. Lazy rendering is not implemented; it is worth doing only
  if large files turn out to be common.

## Checks for the owner

1. Open a few real `.md` files (`F11 → MarkFar`); compare with the source.
2. `F2`, then resize the window: the text re-wraps, the place is kept.
3. `F6` to the source, type something, `F6` back: the change shows without
   saving. `Esc` in the preview leaves the editor open.
4. `Ctrl+Down` / `Ctrl+Up` on a long file.
5. Settings dialog: switch "Show link URLs" off, reopen a preview.
6. `F1` in the preview; Russian help when Far's interface is Russian.
7. `F3` on `*.md` after adding the file association.

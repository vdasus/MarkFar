# 003 — Version 1 scope

Status: accepted 2026-10-07.

Turns the owner's answers in [001-kickoff](001-kickoff.md) (2026-10-07) into
the scope of version 1. Stack, display surface, word wrap, help files and the
Colorer approach are in [002-stack-and-colorer](002-stack-and-colorer.md) and
are not repeated here.

## Scope

| Topic | Version 1 | From |
|---|---|---|
| What is shown | Rendered view and source view, switchable with one key. | Q1 (c) |
| Source view | The real `.md` file in Far's editor, editable, coloured by Colorer's own `markdown` scheme; no word wrap. Changed by [004-preview-and-edit](004-preview-and-edit.md). | Q1, 004 |
| Code highlighting | Fenced blocks through Colorer (002). Languages mapped in `markfar.hrc` at least: C#, SQL, Python, JavaScript, TypeScript, JSON, XML/HTML, YAML, PowerShell, shell, C/C++, Java, CSS, Markdown. | Q2 |
| Opening | `F3` on `*.md` opens the rendered view. A plugin menu item (`F11 → MarkFar`) opens it for any file. | Q3 (1 + 2) |
| Dialect | CommonMark + GitHub extensions (tables, task lists, strikethrough, autolinks). | Q4 |
| Obsidian extras | Front matter shown as a dimmed block at the top. `[[wiki-links]]` shown as link text (md4c parses them with one flag). Callouts, embeds and other Obsidian syntax are out of scope. | Q4, "only if cheap" |
| Mermaid | Shown as a code block (source text). | Q4 |
| Colour in version 1 | Yes. | Q6 |
| Links | Link text followed by the URL in a dimmed colour. Setting "Show link URLs" (on by default); off shows the text only. | Q7 |
| Images | Alt text and path, as a dimmed line. | Q7, default |
| Wide tables | Cell text wraps inside its column (002). | Q7, default |
| Colours | Taken from Far's palette (editor text, background and highlight colours), so the view follows the active Far colour scheme. Code-block colours come from the user's Colorer scheme. | Q8 |
| Languages | English, Russian, Lithuanian: help and language files (002). | Q9 |
| Distribution | Free, under 0BSD. GitHub Releases with a zip of the plugin folder. | Q9 |
| Tests | None. | Q9 |
| CI | GitHub Actions on a Windows runner: on a version tag, build the x64 DLL and attach the zip to a GitHub Release. Nothing else. | Q9 |
| Performance | A 1 MB file opens in under 100 ms; larger files render lazily. | Q10 |

## Keys in the view

The view is a locked Far editor (002), so all of Far's editor keys keep
working: arrows, `PgUp`/`PgDn`, `Home`/`End`, `Ctrl+Home`/`Ctrl+End`, search
(`F7`, `Shift+F7`), selection and copy. MarkFar adds:

| Key | Action |
|---|---|
| `F2` | Word wrap on/off (002). |
| `F6` | Switch to the source (the real file in Far's editor), keeping the position — see [004-preview-and-edit](004-preview-and-edit.md). |
| `Ctrl+Down` / `Ctrl+Up` | Next / previous heading. |
| `Esc`, `F10` | Close the view. |
| `F1` | Help. |

`Ctrl+Up`/`Ctrl+Down` normally scroll the editor by one line without moving
the cursor; in the view they jump between headings instead.

## How `F3` reaches MarkFar

Recommended: **a Far file association**, a native Far mechanism. MarkFar
registers a command prefix (`markfar:`); the association for `*.md` sets the
view command (`F3`) to `markfar:!\!.!`. The owner can remove or change it in
`F9 → Commands → File associations` at any time. The help file explains the
one-time setup.

Rejected: catching Far's viewer as it opens a `.md` file and replacing it.
That works without setup, but a plain-text look at a `.md` file then needs a
way around the plugin, and it fights other plugins that watch the viewer.

## Questions

1. **Keys:** `Tab` for rendered/source and `Ctrl+Up`/`Ctrl+Down` for headings
   — fine, or other keys?

OK.

2. **`F3` setup:** the file association (one manual step, described in the
   help), as recommended above?

OK.

3. **Languages for code blocks:** the list in the table — anything to add or
   drop?

OK.

4. **Release name and version:** start at `0.1.0` and tag `v0.1.0`?

Yes.

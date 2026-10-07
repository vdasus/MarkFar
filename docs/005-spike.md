# 005 — Spike: Colorer and MarkFar colours in one editor

Status: done 2026-10-07. Version 1 replaced the spike build.

The spike answers the open points of [002-stack-and-colorer](002-stack-and-colorer.md)
and [004-preview-and-edit](004-preview-and-edit.md) before the real renderer is
written. It does not read the Markdown file yet: whatever file it is opened
on, it shows a fixed sample.

## What the spike build does

* Registers the plugin menu item `F11 → MarkFar` (panels and editor) and the
command prefix `markfar:`.
* Writes a fixed sample to `%TEMP%\MarkFar\<file>.mfview` and opens it in a
locked editor that deletes the file on close.
* Colours the sample heading itself (bold, bright yellow on the editor
background) and one line with bold, italic and struck-out words.
* Frames an SQL block as `── sql ───` … `───`; `markfar.hrc` hands the block
body to Colorer's SQL scheme. The long `SELECT` line is wrapped to the window
width with a `» ` continuation mark (`↪` in the first build).

## How to run it

1. Install the build and register `markfar.hrc` in Colorer:
   [dev-install](dev-install.md), sections "Build and install" and
   "Colorer scheme".
2. Restart Far.
3. On any file in a panel: `F11 → MarkFar`. Or type
   `markfar:C:\any\file.md` in the command line.

## Checks

Answer each with yes / no and a note; a screenshot helps for 1–4.

1. **Heading colour.** "MarkFar spike" and the `═══` line under it are
   bright yellow; the heading is bold.

   Yellow, but the heading does not look bold.

2. **Font styles.** On the line "Bold, italic and struck-out text …" the
   words are bold, italic and struck out. (Depends on the console: Windows
   Terminal shows all three; the classic console may show colour only.)

   I use conhost: the struck-out word is grey, bold and italic are white.

3. **Colorer in the SQL block.** Keywords, strings and the comment inside the
   block have SQL colours; the frame lines look like comments. If the whole
   preview stays plain, `F11 → FarColorer → List types` (or `Alt+L`) should
   show "MarkFar preview" as the current type.

   Seems OK.

4. **Wrapped line.** The continuation line starting with `↪` is coloured as
   SQL too, and the `↪ ` mark looks like a comment.

   `↪` is shown as an empty box; see the screenshot.

5. **Both at once.** The heading keeps MarkFar's colour while Colorer is
   active — Colorer does not paint over it.

   Not sure what to check; see the attached screenshot.

6. **Locked.** Typing in the preview does nothing; `Esc` closes it without a
   save prompt, and `%TEMP%\MarkFar` holds no `.mfview` file afterwards.

   Yes.

7. **`F3` with the file already open (for 004).** Open a `.md` file with
   `F4`, switch to the panels (`Ctrl+O` or `F12`), and press `F3` on the same
   file. What does Far do — open the viewer, or offer to switch to the open
   editor?

   After `Ctrl+O`: attached screenshot 2.

## Results

First run, 2026-10-07, Far 3.0.6699 x64 in the classic console (conhost).
Screenshots stay out of the repository.

| Check | Result |
|---|---|
| 1. Heading colour | Heading text and its `═══` underline yellow. Not bold (conhost). |
| 2. Font styles | conhost shows colours only: bold and italic words white, struck-out word grey, no styles. Windows Terminal is expected to show the styles. |
| 3. Colorer in the SQL block | Works: keywords, names, strings and comments in SQL colours, frame lines as comments. |
| 4. Wrapped line | Colorer keeps colouring the continued SQL line. The `↪` mark shows as an empty box: the console font has no glyph for it. |
| 5. Both at once | Works: the heading keeps MarkFar's yellow with Colorer active. |
| 6. Locked | Works: no typing, no save prompt, temporary file deleted. |
| 7. `F3` on an open file | Not needed: with the `markfar:` association Far runs MarkFar instead of its viewer, so MarkFar itself decides what to do when the file is already open in an editor (it looks for that window, see 004). Dropped. |

Conclusions:

- **The Colorer approach works.** Code blocks are coloured by Colorer in any
  of its languages, wrapped code lines included, and MarkFar's own colours
  survive next to Colorer's. The open points of 002 are closed.
- **Font styles are a bonus, not a carrier.** In conhost only colour shows,
  so every Markdown element must be told apart by colour alone; bold, italic
  and strike-out flags are added for terminals that render them.
- **Only glyphs the console font has.** Continuation mark changed from `↪`
  to `»` (Latin-1, present in every console font).

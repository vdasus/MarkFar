# 005 — Spike: Colorer and MarkFar colours in one editor

Status: built, waiting for the owner's check in Far.

The spike answers the open points of [002-stack-and-colorer](002-stack-and-colorer.md)
and [004-preview-and-edit](004-preview-and-edit.md) before the real renderer is
written. It does not read the Markdown file yet: whatever file it is opened
on, it shows a fixed sample.

## What the spike build does

- Registers the plugin menu item `F11 → MarkFar` (panels and editor) and the
  command prefix `markfar:`.
- Writes a fixed sample to `%TEMP%\MarkFar\<file>.mfview` and opens it in a
  locked editor that deletes the file on close.
- Colours the sample heading itself (bold, bright yellow on the editor
  background) and one line with bold, italic and struck-out words.
- Frames an SQL block as `── sql ───` … `───`; `markfar.hrc` hands the block
  body to Colorer's SQL scheme. The long `SELECT` line is wrapped to the window
  width with a `↪ ` continuation mark.

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
2. **Font styles.** On the line "Bold, italic and struck-out text …" the
   words are bold, italic and struck out. (Depends on the console: Windows
   Terminal shows all three; the classic console may show colour only.)
3. **Colorer in the SQL block.** Keywords, strings and the comment inside the
   block have SQL colours; the frame lines look like comments. If the whole
   preview stays plain, `F11 → FarColorer → List types` (or `Alt+L`) should
   show "MarkFar preview" as the current type.
4. **Wrapped line.** The continuation line starting with `↪` is coloured as
   SQL too, and the `↪ ` mark looks like a comment.
5. **Both at once.** The heading keeps MarkFar's colour while Colorer is
   active — Colorer does not paint over it.
6. **Locked.** Typing in the preview does nothing; `Esc` closes it without a
   save prompt, and `%TEMP%\MarkFar` holds no `.mfview` file afterwards.
7. **`F3` with the file already open (for 004).** Open a `.md` file with
   `F4`, switch to the panels (`Ctrl+O` or `F12`), and press `F3` on the same
   file. What does Far do — open the viewer, or offer to switch to the open
   editor?

## Results

To be filled in after the check.

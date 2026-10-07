# 004 — Preview and edit in one flow

Status: accepted 2026-10-07.

Changes the "source view" of [003-v1-scope](003-v1-scope.md): the source is
no longer a read-only copy but the real file in Far's editor.

## Use case

The owner presses `F3` on a `.md` file and reads it in preview. The file has
questions to answer, so the owner switches to the source, types the answers,
saves, switches back to preview to check the result, and goes on reading.
Switching back and forth must be one key, must keep the reading position, and
must never lose an edit.

## Design

Two windows form one document:

| | Preview | Source |
|---|---|---|
| What it is | Locked Far editor on a temporary file with the rendered text (002). | Ordinary Far editor on the real `.md` file. |
| Editing | No. | Yes, everything Far's editor offers. |
| Colours | MarkFar + Colorer for code blocks (002). | Colorer's own `markdown` scheme. |
| Word wrap | Yes, `F2` toggles it (002). | No — Far's editor has no soft wrap (see "Limits"). |
| `F2` | Wrap on/off. | Save, as always in Far. |
| `F6` | Switch to source. | Switch to preview. |
| `Esc`, `F10` | Close the preview. | Close the editor; Far asks to save unsaved changes, as always. |

### Why `F6`

In Far, `F6` already switches between the viewer and the editor of the same
file (`FarEng.hlf`: viewer "`F6` Switch to editor", editor "`F6` Switch to
viewer"). Preview takes the viewer's place in that pair, so the key does what
a Far user expects. `Tab` from 003 is dropped: in the source editor it inserts
a tab character.

### Behaviour

- **Preview to source:** if a source editor for the file is already open,
  MarkFar brings it to the front (`ACTL_SETCURRENTWINDOW`); otherwise it opens
  one. The cursor goes to the source line of the text at the top of the
  preview.
- **Source to preview:** MarkFar renders from the **editor's current text**
  (`ECTL_GETSTRING`), not from the file on disk, so unsaved answers already
  show in the preview. Saving is needed only to keep the changes. The preview
  opens at the rendered line of the cursor's source line.
- **Undo and history survive switching:** the source editor stays open in the
  background while the preview is in front, so its undo history, cursor and
  selection are kept.
- **Closing:**
  - `Esc` in the preview closes only the preview. A source editor with
    unsaved changes stays open, so nothing is lost.
  - Closing the source editor does not close the preview. Changed in 0.1.2:
    the owner returns to the preview with `Esc` from the source (`F3`, `F6`,
    edit, `F2`, `Esc`), so saving (`EE_SAVE`) or closing (`EE_CLOSE`) the
    source marks the preview stale, and it re-renders with the new text when
    it is in front again.
- **`F6` in other `.md` editors:** a `.md` file opened with `F4` from the
  panel is the same ordinary editor, so `F6` there opens the preview too
  instead of Far's viewer. A setting "F6 in Markdown editor opens preview"
  (on by default) turns it off for owners who want the plain viewer.

### Position mapping

md4c reports the source offset of every block. While rendering, MarkFar
records for each rendered line the source line it came from. One table serves
three needs: preview to source (`F6`), source to preview, and keeping the
position on resize or wrap toggle (002). It is needed anyway, so this flow
adds no new mechanism.

## Limits

- **No word wrap in the source.** Far's editor cannot wrap lines on screen
  without changing the text. Wrapping a copy and writing the edits back to the
  real file would be fragile, so the source shows lines as they are, with
  horizontal scrolling. Files written with short lines (as these documents
  are) are not affected. The AutoWrap plugin (installed) wraps while typing,
  if wanted.
- This replaces the 001 Q1 answer "(c) both, switchable" in one detail: the
  source is editable but not wrapped.

## Effort

About one to two days on top of version 1: window pairing and focus
(`ACTL_GETWINDOWINFO`, `ACTL_SETCURRENTWINDOW`, `EE_CLOSE`), rendering from an
editor buffer instead of a file, and the `F6` interception. The position
mapping is shared with resize handling.

Risk to check in the spike: Far's behaviour when `F3` is pressed on a file
that is already open in an editor (it may offer to switch to that window).

## Questions

1. **Source without wrap** — acceptable, given that editing the real file is
   the point?

   Yes.
2. **`F6` and `F4`** — `F6` both ways, `F4` from preview to source as well?

   `F6` both ways; no extra `F4`.
3. **`F6` in any `.md` editor** opens the preview (setting, on by default) —
   or only in editors MarkFar opened itself?

   In any `.md` editor.
4. **Version 1 or later:** include this flow in version 1, or ship the
   read-only preview first?

   Version 1. A preview-only build only if something needs checking first
   (the spike in 002 covers that).

# 007 — Setup without manual steps

Status: implemented in 0.2.0; fixes in 0.2.1 wait for the owner's check.

## Problem

Up to 0.1.2, installing MarkFar meant two manual steps after copying the
folder: a file association for `F3` on `*.md`, and the path of the plugin's
`hrc` folder in Colorer's settings. The owner asked for an installation that
needs neither. Far Manager 3 has no package manager and no API for file
associations.

## Decision

MarkFar does both itself, with Far's own mechanisms, every time Far starts:

- **`F3`:** the plugin is loaded at start (`PF_PRELOAD`) and adds an
  in-memory macro with `MCTL_ADDMACRO`: key `F3`, area panels, condition
  (a callback) "the current item of a file panel is a `.md`/`.markdown`
  file", action `Plugin.Call(<MarkFar>, "F3")`, which `OpenW` handles as
  `OPEN_FROMMACRO`. No file is written to the user's profile; switching the
  setting off removes the macro (`MCTL_DELMACRO`).

  OK.

- **Colorer:** MarkFar opens Colorer's settings through Far's settings API
  (Colorer's GUID, subkey of the same name, value `UserHrcPath`). Empty →
  set to `<plugin>\hrc`. Another folder → copy `markfar.hrc` into it (and
  delete it there when the setting is switched off). Then it asks Colorer to
  reload (`Plugin.Call(<Colorer>, "Settings", "Reload")` posted as a macro).
- Two new settings, both on by default: "F3 on a Markdown file in the panels
  opens the preview", "Colour code blocks with Colorer".

  Colorer's setting is empty; the SQL block in a test file is not coloured.

Rejected: a Lua macro file in `%FARPROFILE%\Macros\scripts` (leaves a file
behind after removal of the plugin); an install script (`cmd` is blocked on
the owner's machine, PowerShell runs in Constrained Language mode).

## Risks to check

- Whether "Settings → Reload" makes Colorer re-read `UserHrcPath`, or only a
  restart of Far does.
- Colorer may write its own settings back when its configuration dialog is
  closed with OK; that keeps MarkFar's value, since it reads it first.
- `PF_PRELOAD` loads the plugin at every start of Far (one DLL of about
  0.5 MB; no measurable delay expected).

## Checks for the owner

1. Remove the `*.md` file association added for 0.1.x
   (`F9 → Commands → File associations`), so the macro is what runs.

   Done.
2. In Colorer's settings, clear "Custom schemas folder (hrc)".

   Done.
3. Close Far, install 0.2.0, start Far.

   Done. But the help does not show the version.
4. `F3` on a `.md` file opens the preview; `Alt+F3` opens Far's viewer.

   OK.
5. Code blocks in the preview are coloured; Colorer's field now shows the
   plugin's `hrc` folder (after a restart of Far if not at once).

   No.
6. MarkFar settings: switch both new options off → `F3` opens Far's viewer,
   Colorer's field is empty again. Switch them back on.

   Not good either: the settings dialog is too narrow, the last option runs
   past its frame (screenshot `bad1.png`).

## Results

First check of 0.2.0, 2026-10-07: the `F3` macro works (checks 1–4).
Fixed in 0.2.1:

- **Colorer path not set.** Far opens a plugin's settings at that plugin's
  own key; Colorer's values sit there, at the root. 0.2.0 opened a subkey
  named after Colorer's GUID below it, created it when missing, and wrote
  `UserHrcPath` there, where Colorer never looks. 0.2.1 writes at the root
  and deletes the stray subkey.
- **No version in the help.** The version stood only in the contents topic;
  `F1` in the preview opens the keys topic. 0.2.1 shows it there as well.
- **Settings dialog too narrow.** Wider dialog, shorter Colorer label.

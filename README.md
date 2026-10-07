# MarkFar

A Markdown preview plugin for [Far Manager 3](https://www.farmanager.com/):
open a `.md` file and read it rendered, with syntax highlighting, inside Far.

Status: version 0.1.0 in testing.

## Install

1. Download `MarkFar-<version>-x64.zip` from
   [Releases](https://github.com/vdasus/MarkFar/releases) and unpack the
   `MarkFar` folder into `%APPDATA%\Far Manager\Profile\Plugins`.
2. Restart Far.
3. Optional, once: point Colorer at the plugin's `hrc` folder and add a file
   association so that `F3` on `*.md` opens the preview. Both steps are in
   the plugin help (`F1` in the preview) and in
   [docs/dev-install.md](docs/dev-install.md).

## Use

| Key | In the preview |
|---|---|
| `F2` | Word wrap on/off |
| `F6` | Switch to the file in Far's editor; `F6` there returns to the preview |
| `Ctrl+Down` / `Ctrl+Up` | Next / previous heading |
| `Esc` | Close |
| `F1` | Help |

Building from source: [docs/dev-install.md](docs/dev-install.md). Design
notes: [docs/](docs/).

## Third-party code

- [md4c](https://github.com/mity/md4c) 0.6.0, MIT license
  (`third_party/md4c`).
- Far Manager plugin SDK headers, BSD 3-Clause license (`sdk/`).

## Authorship

The code and documentation in this repository are written by Claude (an AI
model by Anthropic) under the supervision of vdasus, who sets the
direction, reviews the work and decides what goes in.

## License and disclaimer

Released under the [BSD Zero Clause License](LICENSE) (0BSD): use, copy,
modify and distribute it for any purpose, with or without fee, without any
obligation, including attribution.

The software is provided "as is", without warranty of any kind. The author
accepts no responsibility or liability for any damage, data loss or other
consequences of using it. Use it at your own risk.

# MarkFar — instructions for Claude

Markdown preview plugin for Far Manager 3 (C++20, llvm-mingw, md4c). Design
notes and decisions live in `docs/` (numbered documents); read the latest
ones before changing behaviour.

## Rules

- **Version (semver).** The single source is `src/version.hpp`; `build.sh`
  copies it into the help files. Bump it with every change that reaches the
  owner or the repository: PATCH for fixes, MINOR for new features, MAJOR
  for incompatible changes (settings, key bindings, file layout). Note the
  new version in the commit message. Release tags are `v<version>`.
- **Owner's comments in documents.** The owner answers questions inline in
  `docs/*.md`, often in transliterated Russian or rough English. Before every
  push: translate those answers into English, fix spelling and grammar, keep
  the meaning, change nothing else. Their Markdown formatting may also be
  broken by Notepad (escaped `\*`, `\_`, `\[`; `*` bullets; tables without
  spaces): restore it.
- **Commits** are authored as `vdasus <4681649+vdasus@users.noreply.github.com>`
  (set in this repository's `.git/config`). No real name or work e-mail in
  files or history.
- **Generated text** (docs, help, comments, commit messages) is in English,
  except the Russian and Lithuanian help and language files.

## Build and install

- Build under WSL: `./build.sh` (installs into
  `%APPDATA%\Far Manager\Profile\Plugins\MarkFar`; fails while Far runs —
  ask the owner to close Far), `./build.sh noinstall` to build only.
- `cmd.exe`, `.cmd` and `.bat` are blocked by group policy on the owner's
  machine; PowerShell runs in Constrained Language mode. Do not add Windows
  batch scripts.
- Help (`hlf/`) and language files (`lng/`) are UTF-8 with BOM and CRLF;
  message order in `.lng` matches `enum MsgId` in `src/markfar.cpp`.

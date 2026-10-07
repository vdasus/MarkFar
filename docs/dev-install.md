# Installing a development build for testing

How to run a MarkFar build in Far Manager while it is being developed.
A release installs the same way: unpack the zip from GitHub Releases into
that folder.

## Where Far looks for plugins

Far 3 loads plugins from two folders, each plugin in its own subfolder
(`FarEng.hlf`, topic "Plugins"):

1. `Plugins` next to `Far.exe` — `C:\Program Files\Far Manager\Plugins`.
   Writing there needs administrator rights.
2. `Plugins` in the user profile — `%APPDATA%\Far Manager\Profile\Plugins`.
   No administrator rights needed.

Development builds go to the second folder:

```
%APPDATA%\Far Manager\Profile\Plugins\MarkFar\
    MarkFar.dll
    MarkFarEng.lng   MarkFarRus.lng   MarkFarLit.lng
    MarkFarEng.hlf   MarkFarRus.hlf   MarkFarLit.hlf
    hrc\markfar.hrc
```

Far 3 has no setting for an extra plugin folder; the `-p` command-line switch
replaces the plugin folders instead of adding one, so it is not used here.

## Toolchain (once)

The build runs under WSL with llvm-mingw (see
[002-stack-and-colorer](002-stack-and-colorer.md) for why not MSVC). Unpack
the Linux x86_64 archive from https://github.com/mstorsjo/llvm-mingw/releases
into `~/.local/opt`:

```
mkdir -p ~/.local/opt && cd ~/.local/opt
curl -LO https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64.tar.xz
tar -xJf llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64.tar.xz && rm llvm-mingw-*.tar.xz
```

`build.sh` finds it there; putting its `bin` folder on `PATH` works as well.

## Build and install

1. Close Far Manager. Far keeps `MarkFar.dll` loaded, so a running Far blocks
   the copy. (Not needed for the very first install.)
2. In the repository folder, under WSL, run:

   ```
   ./build.sh
   ```

   It compiles `out/MarkFar/MarkFar.dll` and copies the `out/MarkFar` folder
   to `%APPDATA%\Far Manager\Profile\Plugins\MarkFar`.
   `./build.sh noinstall` only builds.
3. Start Far. On the first start after a new or changed plugin, Far finds it
   and adds it to its plugin cache.

Check: `F11` shows "MarkFar" in the plugin menu;
`F9 → Options → Plugins configuration` lists it.

## Colorer scheme (once)

`markfar.hrc` colours code blocks in the preview through Colorer (see
[002-stack-and-colorer](002-stack-and-colorer.md)). Point Colorer at it:

1. `F9 → Options → Plugins configuration → FarColorer`.
2. "Custom schemas folder (hrc)":
   `%APPDATA%\Far Manager\Profile\Plugins\MarkFar\hrc` written out in full,
   for example `C:\Users\<name>\AppData\Roaming\Far Manager\Profile\Plugins\MarkFar\hrc`.
3. Save. Colorer reloads its schemes.

## `F3` on `.md` files

`F3` reaches MarkFar through a Far file association
([003-v1-scope](003-v1-scope.md)):

1. `F9 → Commands → File associations`, `Ins` to add.
2. Mask: `*.md`
3. Command for view (`F3`): `markfar:!\!.!`
4. Leave the other commands empty.

Delete the association to get the plain viewer back.

## Remove

Close Far and delete `%APPDATA%\Far Manager\Profile\Plugins\MarkFar`. Delete
the `*.md` file association if one was added.

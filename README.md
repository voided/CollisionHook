# SourceMod CollisionHook Extension

CollisionHook exposes the `CH_ShouldCollide` and `CH_PassFilter` forwards for
SourceMod plugins.

## Build targets

| Engine | Windows | Linux |
| --- | --- | --- |
| Team Fortress 2 | x86, x64 | x86, x64 |
| Counter-Strike: Source | x86, x64 | x86, x64 |
| Day of Defeat: Source | x86, x64 | x86, x64 |
| Half-Life 2: Deathmatch | x86, x64 | x86, x64 |
| Left 4 Dead | x86 | x86 |
| Left 4 Dead 2 | x86 | x86 |
| Source SDK 2013 | x86 | x86 |
| Counter-Strike: Global Offensive (legacy dedicated server) | x86 | x86 |

Source SDK 2013 mods with different server binaries may need their own gamedata
signatures.

The [GitHub Actions workflow](.github/workflows/ci.yml) builds and verifies
packages on GitHub-hosted runners using public AlliedModders dependencies.
Linux packages are built on Ubuntu 22.04 for glibc 2.35 compatibility.
Local builds use AMBuild, a C++17 compiler, the matching HL2SDK branches, and
the `1.12-dev` branches of SourceMod and Metamod:Source. Initialize SourceMod's
`public/amtl`, `public/safetyhook`, and `sourcepawn` submodules before running
`configure.py`.

Install the package's `addons/` tree into the game directory so it merges with the
existing SourceMod installation. Match the extension architecture to the game
server process. SourceMod loads x64 extensions from `addons/sourcemod/extensions/x64/`.

[AlliedModders forum thread](https://forums.alliedmods.net/showthread.php?t=197815)

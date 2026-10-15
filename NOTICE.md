# Notice

nw-port is an unofficial fan project. It is not affiliated with, endorsed or sponsored by Amazon Games. New World
is a trademark of Amazon.com, Inc. or its affiliates.

The repository contains no files from the game. The tools extract what the project uses from each person's own
New World install, on their machine; extracted files and the imported Unreal project stay out of the repository and
must not be redistributed.

## Third-party components

Fetched or built by the setup scripts, not stored in this repository. Each keeps its own license.

| Component | Source | License |
|---|---|---|
| nw-buddy / nwbt | github.com/paulorsouza/nw-buddy (fork of giniedp/nw-buddy) + `patches/nwbt.patch` | MIT |
| ooz | github.com/powzix/ooz + `patches/ooz.patch` | GPL-3.0 |
| DirectXTex texconv | github.com/microsoft/DirectXTex | MIT |
| vgmstream | github.com/vgmstream/vgmstream | ISC-style (see its COPYING) |
| vorbis-tools (oggenc) | xiph.org, system package | GPL-2.0 |
| wwiser | github.com/bnnm/wwiser | see its repository |
| Unreal Engine | Epic Games | Unreal Engine EULA |

`patches/ooz.patch` modifies GPL-3.0 code and is distributed under GPL-3.0. `patches/nwbt.patch` and
`patches/nwbt-nwport.patch` modify MIT code and are distributed under MIT.

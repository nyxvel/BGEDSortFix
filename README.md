# BGED Sort Fix

SKSE plugin for Skyrim AE (written and tested on 1.6.1170). It fixes SOS/TNG genitals whose
behavior graph sometimes doesn't attach: bends, erections and the flaccid pose then do nothing
and the shaft stays straight out.

## The problem

A worn mesh can bring its own animation graph through a `BSBehaviorGraphExtraData` block
("BGED"). TNG/SOS genital meshes use it: `Auxbones\SOS\SOSFemale.hkx`,
`auxbones\TNG\TNGMale.hkx`... The game finds that block with a binary search over the
node's extra data, so the extra data must stay sorted by the names' 64-bit string addresses.

Meshes with a single shape (ERF Futanari, TNG's `m_genitals`/`c_genitals`...) keep BGED on
their root node. When the game attaches one, it folds the root into the shape and moves BGED
onto it, in the right order. RaceMenu (skee64 0.4.20) then adds `SHAPEDATA` to every
body-morphed shape and re-sorts the whole list comparing only the **low 32 bits** of the
addresses. When the strings sit more than 4 GB apart (as they do with EngineFixes' memory
manager), that order is wrong for the game, and its search misses BGED:

- fresh attach (load, cell change, undress): no graph at all;
- disable/enable or re-equip: a graph bound to the mesh instead of the skeleton, with every
  bone missing.

Either way the bend/erection events are sent and nothing happens. String addresses change
with every game launch, so it fails on some launches and works on others. Within one launch
the outcome never changes, so re-equipping doesn't help. Only actors whose genital shape gets
body morphs are affected (e.g. futa women with an OBody/BodySlide preset). Multi-shape meshes
such as UBE's keep BGED on a root RaceMenu never sorts.

## The fix

Right before the game sets up a worn part's graph, the plugin re-sorts that part's extra data
the way the game expects (under the game's own extra-data lock). That's all it does: no
settings, no ESP.

It patches three call sites of the game's biped graph setup (address library IDs 15678,
15680, 15683 -> 15718). It checks the code before patching and leaves anything unexpected
alone. On SE/VR it does nothing.

## Log

`Documents\My Games\Skyrim Special Edition\SKSE\BGEDSortFix.log`: the patched call sites,
plus one line per fix, for example

    Re-sorted the extra data of slot 52 ( (13000D6F)[1]/ (FE004B0A) [20%]) on 02002B74 Serana (part attach)

## Requirements

SKSE64, Address Library for SKSE Plugins.

## Building

VS 2022 Build Tools (C++ workload, CMake, Ninja) and a vcpkg checkout (`VCPKG_ROOT`, or
`..\vcpkg`). CommonLibSSE-NG is a git submodule.

    git clone https://github.com/nyxvel/BGEDSortFix
    cd BGEDSortFix
    git submodule update --init
    powershell -ExecutionPolicy Bypass -File build.ps1

The archive for the mod manager lands in `dist\`.

## License

MIT (`LICENSE`). CommonLibSSE-NG is MIT too.

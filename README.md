<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->

# felitronics-eqview

The EQ plot, as pieces you can reuse.

Almost every audio product ends up drawing the same picture: a logarithmic frequency axis, a
spectrum behind it, a response curve on top, and handles you drag. In the Darwin's Cat family that
picture had been written four times, and the same bugs had been fixed four times. This library is
that picture, taken apart into pieces a product can assemble.

It is **header-only**, it depends on
[felitronics-core](https://github.com/darwinscat/felitronics-core) and nothing else, and it takes
its colours as parameters — so a product whose look differs from the family's is not fighting it.

## Use it

Resolve core **first**: this library never fetches its own dependencies, so that one version of
core is chosen by the project that owns the graph, not by a library deep inside it.

```cmake
include(FetchContent)

FetchContent_MakeAvailable(felitronics_core)     # core first — always

FetchContent_Declare(felitronics_eqview
    GIT_REPOSITORY https://github.com/darwinscat/felitronics-eqview.git
    GIT_TAG        v0.1.0)                       # a tag, always: 0.x is a moving API
FetchContent_MakeAvailable(felitronics_eqview)

target_link_libraries(my_plugin PRIVATE felitronics::eqview)
```

Adding this library without core in place is a configure-time error with an explanation, not a
mystery link failure later.

## What's here

Nothing in this list needs JUCE. That is on purpose: this is the arithmetic of an EQ view, and it
is the half that can be tested. The drawing layers are being lifted out of TabbyEQ next, and they
will take their colours as a theme.

| | |
|---|---|
| `felitronics/eqview/PlotGrid.h` | Where a logarithmic axis puts its ticks, which of them carry a number, and how a scale thins its numbers when the window shrinks. |
| `felitronics/eqview/HandleMath.h` | Where a filter's node and its Q/slope handles sit on the plot, and how a filter type behaves under a drag. |
| `felitronics/eqview/TraceSet.h` | The response curves: per-band, per-lane and composite, evaluated at a display rate so the shape reads as the analog intent at any sample rate. |

The coordinate map itself — frequency ↔ pixel, dB ↔ pixel — lives in core as
`felitronics/analysis/PlotMap.h`, because it is pure arithmetic and other things need it too. This
library re-exports the name, so its own headers read as one vocabulary.

### A scale, for example

The rule is separate from the painting, so a consumer with its own canvas — a waveform, a cabinet
curve — reuses the rule and not a picture:

```cpp
namespace grid = felitronics::eqview::grid;

const auto labels = grid::labelSet (grid::decadeWidth (map), 50.0);   // 50 px of room per number

grid::forEachTick (map, [&] (double hz, int step)
{
    paintLine (map.freqToX (hz), grid::isDecade (step) ? heavy : light);

    if (grid::isLabelled (step, labels))
        paintNumber (hz);
});
```

Widen the window and 30 · 70 · 300 · 700 appear between the usual 20 · 50 · 100 · 200; narrow it
and they leave before they can collide; squeeze it into a thumbnail and only the decades remain.

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j && ctest --test-dir build
```

A sibling `../felitronics-core` checkout is used when present; otherwise the pinned tag is fetched,
which is exactly what CI does — so a green CI run proves the pin, not somebody's working copy.

The tests are written from theory rather than from the code: the ruler *is* the set k·10ⁿ, a decade
*is* a constant width on a logarithmic axis, a matched filter *is* exact at its centre frequency. A
test that only agrees with the current implementation cannot tell you the implementation is wrong.

## Status

**0.x — a moving API.** Pin an exact tag and bump it deliberately. The shape settles once a second
product ships against it; until then the only consumer is TabbyEQ, which is where these pieces grew
up.

## Licence

AGPL-3.0-or-later — see [LICENSE](LICENSE).

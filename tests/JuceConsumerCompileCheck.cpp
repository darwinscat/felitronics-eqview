// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-eqview — see LICENSE.
//
// The drawing layer has no unit test — pixels are checked by eye, against a product's own screens —
// but it still has to COMPILE, and it has to compile under the flags a real consumer uses
// (juce_recommended_warning_flags, warnings as errors). That is what this is: one translation unit
// that includes the JUCE-facing header first and alone, renders every piece once into an offscreen
// image, and exits. It catches a missing include, a signature that drifted, a narrowing conversion
// — the things that would otherwise surface inside somebody's plugin build.

#include <felitronics/eqview/PlotSurface.h>

#include <cstdio>

int main()
{
    using namespace felitronics::eqview;

    PlotSurface surface;
    surface.map.width      = 800.0f;
    surface.map.height     = 500.0f;
    surface.map.plotBottom = 480.0f;
    surface.theme.zeroGlow = juce::Colour (0x0d9170ff);

    juce::Image img (juce::Image::ARGB, 900, 520, true);
    juce::Graphics g (img);

    surface.paintFrequencyRuler (g, 0.0f, surface.map.height);
    surface.paintLevelGrid      (g, 0.0f, surface.map.width, 6.0, 30.0);
    surface.paintFrequencyLabels (g, surface.map.height - 14.0f);

    surface.paintGainColumn (g, 30.0, 6.0, { 740, 0, 24, 12 }, { 30.0f, 470.0f },
                             juce::Colour (0xffff8822));

    surface.paintValueColumn (g, [&surface] (double db) { return surface.map.specDbToY (db); },
                              0.0, -90.0, 10.0, { 806, 0, 28, 12 }, { 8.0f, 484.0f },
                              juce::Colour (0x66ffffff));

    // A degenerate map must draw nothing rather than hang or divide by zero.
    PlotSurface empty;
    empty.map.width = 0.0f;
    empty.paintFrequencyRuler (g, 0.0f, 10.0f);
    empty.paintLevelGrid (g, 0.0f, 0.0f, 0.0, 0.0);

    std::printf ("eqview JUCE consumer compile check: every piece drew.\n");
    return 0;
}

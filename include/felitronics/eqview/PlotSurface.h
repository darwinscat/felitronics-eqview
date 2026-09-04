// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Darwin's Cat — Oleh Tsymaienko & Alisa Lafoks. Part of felitronics-eqview — see LICENSE.

#pragma once

#include <juce_graphics/juce_graphics.h>

#include <felitronics/analysis/PlotMap.h>
#include <felitronics/eqview/PlotGrid.h>

#include <cmath>

//==============================================================================
// PlotSurface — the ground an EQ view is read against: the logarithmic ruler, the level lines
// across it, and the numbers along its edges. The bottom layer of the view, and the first one here
// that needs a Graphics.
//
// It draws and owns nothing else: no component, no state, no timer. The caller hands it a PlotMap
// (where frequencies and levels land, from felitronics-core) and a Theme (what things look like),
// and calls the pieces it wants in the order it wants them — a product with no level column simply
// never asks for one. The rules behind the pieces live in PlotGrid: which ticks exist, which are
// the scale's joints, and how many numbers fit before they collide.
//
// WHY THE THEME IS A PARAMETER: the family's products do not share a palette. TabbyEQ's violet sits
// a few LSBs off the brand's on purpose, and a product built on this must not have to fight the
// library for its own look — so every colour and every font arrives from outside, and the defaults
// here are only what makes a bare call legible.
namespace felitronics::eqview
{

//==============================================================================
// What the surface is drawn IN. Fonts come as juce::Font rather than as a size, so a product whose
// chrome is set in its own face (they are, in this family) is not told what type to use.
struct PlotTheme
{
    juce::Colour grid       { 0x18ffffff };   // the ruler's own weight — the decade lines
    juce::Colour gridFine   {};               // the steps between them; transparent = grid at half alpha
    juce::Colour gridZero   { 0x40ffffff };   // the line a bipolar scale is symmetric about
    juce::Colour zeroGlow   {};               // an optional bloom behind it; transparent = none
    juce::Colour axisText   { 0x66ffffff };   // the numbers along the axis

    juce::Font   labelFont  { juce::FontOptions (11.0f) };
    float        tracking   = 0.0f;           // em, for a display face; 0 draws in one glyph run

    juce::Colour fineInk() const noexcept
    {
        return gridFine.isTransparent() ? grid.withMultipliedAlpha (0.5f) : gridFine;
    }
};

//==============================================================================
// How much room the numbers want, in pixels. These are the two dials worth having: a number needs
// clear space beside it, and a ruler stops being a scale when its steps are closer than a hatch.
struct PlotMetrics
{
    double labelGapPx          = 50.0;   // between neighbouring frequency numbers, horizontally
    double rowGapPx            = 25.0;   // between neighbouring numbers down a column
    float  fineRulerMinDecade  = 62.0f;  // below this, the ruler shows only its captioned steps
};

//==============================================================================
// The surface itself: a map, a theme, some metrics, and the pieces you can draw with them. Cheap to
// build — make one per paint pass, from the geometry that pass is using.
struct PlotSurface
{
    PlotMap     map;
    PlotTheme   theme;
    PlotMetrics metrics;

    //--------------------------------------------------------------------------
    // The logarithmic ruler: every 1…9 step of every decade, with the DECADES at the grid's own
    // weight and everything between them at the fine one. They are the scale's joints; weighting
    // 50/200/500 with them makes a log axis read as an evenly stepped one. A captioned step always
    // gets its line however narrow the plot — a number with no line under it is not a scale — and
    // the rest steps aside when a decade is thinner than the metrics allow.
    void paintFrequencyRuler (juce::Graphics& g, float top, float bottom) const
    {
        const bool fine = grid::decadeWidth (map) >= metrics.fineRulerMinDecade;

        grid::forEachTick (map, [&] (double hz, int step)
        {
            if (! fine && ! grid::isCaptioned (step))
                return;

            g.setColour (grid::isDecade (step) ? theme.grid : theme.fineInk());
            g.drawVerticalLine ((int) map.freqToX (hz), top, bottom);
        });
    }

    // The frequency numbers, along the bottom. How many of them there are is a question about
    // width: the densest rung of the ladder whose closest pair still keeps `labelGapPx` between
    // them. `boxTop` is where the row of text starts; each number sits just right of its own line.
    void paintFrequencyLabels (juce::Graphics& g, float boxTop, int boxWidth = 34, int boxHeight = 12) const
    {
        const auto rung = grid::labelSet (grid::decadeWidth (map), metrics.labelGapPx);

        g.setColour (theme.axisText);
        grid::forEachTick (map, [&] (double hz, int step)
        {
            if (! grid::isLabelled (step, rung))
                return;

            drawLabel (g, formatHz (hz),
                       juce::Rectangle<int> ((int) map.freqToX (hz) + 2, (int) boxTop, boxWidth, boxHeight),
                       juce::Justification::left);
        });
    }

    //--------------------------------------------------------------------------
    // The level lines across the plot, every `step` dB of a scale that runs ±`range` about zero.
    // The outermost pair is skipped on purpose: sitting exactly on the top and bottom edges, they
    // read as a frame the curves bump into rather than as part of the scale. The zero line gets the
    // gridZero ink, and an optional bloom behind it.
    void paintLevelGrid (juce::Graphics& g, float x0, float x1, double step, double range) const
    {
        if (! (step > 0.0) || ! (range > 0.0))
            return;

        for (double db = -range; db <= range + 0.01; db += step)
        {
            if (std::abs (std::abs (db) - range) < 0.01)
                continue;

            const float y    = map.dbToY (db);
            const bool  zero = std::abs (db) < 0.01;

            if (zero && ! theme.zeroGlow.isTransparent())
            {
                g.setColour (theme.zeroGlow);
                g.fillRect (x0, y - 2.5f, x1 - x0, 5.0f);
            }

            g.setColour (zero ? theme.gridZero : theme.grid);
            g.drawHorizontalLine ((int) y, x0, x1);
        }
    }

    //--------------------------------------------------------------------------
    // A column of numbers down a linear scale — the same piece twice over, because that is what it
    // is: the EQ's gain scale and the analyzer's level scale differ in where they sit, what ink
    // they use and what they measure, not in how a column of numbers works.
    //
    //   `toY`      how this column's values land on the plot (dbToY for the gain scale, specDbToY
    //              for the level one) — the two scales share a surface and are NOT the same map;
    //   `box`      x, width and height of the text cells; each number is centred on its own line;
    //   `visible`  the vertical band the column may write in, so a number never lands under the
    //              product's own furniture (a range picker at the top, a toolbar lane at the foot);
    //   `signed_`  writes "+6" as well as "-6", the way a gain scale reads and a level scale does not.
    //
    // The step climbs PlotGrid's ×1 · ×2 · ×3 (· ×6) ladder until the numbers clear `rowGapPx`, so a
    // short plot loses numbers instead of stacking them into a column of digits.
    template <typename ToY>
    void paintValueColumn (juce::Graphics& g, ToY&& toY, double from, double to, double baseStep,
                           juce::Rectangle<int> box, juce::Range<float> visible, juce::Colour ink,
                           bool signed_ = false, juce::Justification just = juce::Justification::right) const
    {
        if (! (baseStep > 0.0))
            return;

        const double pxPerUnit = std::abs ((double) (toY (0.0) - toY (1.0)));
        const double step      = grid::labelStep (baseStep, pxPerUnit, metrics.rowGapPx);
        const double direction = to >= from ? 1.0 : -1.0;

        g.setColour (ink);
        for (double v = from; direction * (v - to) <= 0.01; v += direction * step)
        {
            const float y = toY (v);
            if (! visible.contains (y))
                continue;

            const auto text = (signed_ && v > 0.0 ? juce::String ("+") : juce::String())
                                + juce::String ((int) std::lround (v));
            drawLabel (g, text, box.withY ((int) y - box.getHeight() / 2), just);
        }
    }

    // The gain scale's own column, counted OUTWARD FROM ZERO rather than up from -range: that keeps
    // the row symmetric about the zero line at every rung of the ladder, and every number lands on a
    // grid line. Zero itself is left to the line, which is louder than any number would be.
    void paintGainColumn (juce::Graphics& g, double range, double baseStep, juce::Rectangle<int> box,
                          juce::Range<float> visible, juce::Colour ink) const
    {
        if (! (baseStep > 0.0) || ! (range > 0.0))
            return;

        const double pxPerDb = std::abs ((double) (map.dbToY (0.0) - map.dbToY (1.0)));
        const double step    = grid::labelStep (baseStep, pxPerDb, metrics.rowGapPx);

        g.setColour (ink);
        for (double mag = step; mag <= range - 0.01; mag += step)
            for (const double db : { mag, -mag })
            {
                const float y = map.dbToY (db);
                if (! visible.contains (y))
                    continue;

                drawLabel (g, (db > 0.0 ? "+" : "") + juce::String ((int) std::lround (db)),
                           box.withY ((int) y - box.getHeight() / 2), juce::Justification::right);
            }
    }

    //--------------------------------------------------------------------------
    // "20" · "200" · "2k" · "20k" — the way an audio axis has always spelled itself.
    static juce::String formatHz (double hz)
    {
        return hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String ((int) std::lround (hz));
    }

private:
    // One label, in the theme's type. Tracking is spelled out here rather than left to the caller so
    // that a product with a display face gets the same spacing on the axis as on its chrome.
    void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> box,
                    juce::Justification just) const
    {
        g.setFont (theme.labelFont);

        if (theme.tracking <= 0.0f)
        {
            g.drawText (text, box, just);
            return;
        }

        juce::GlyphArrangement ga;
        ga.addLineOfText (theme.labelFont, text, 0.0f, 0.0f);

        const float spread = theme.labelFont.getHeight() * theme.tracking;
        for (int i = 1; i < ga.getNumGlyphs(); ++i)
            ga.getGlyph (i).moveBy (spread * (float) i, 0.0f);

        const auto  bounds = ga.getBoundingBox (0, -1, true);
        const auto  area   = box.toFloat();
        const float x = just.testFlags (juce::Justification::right) ? area.getRight() - bounds.getWidth()
                      : just.testFlags (juce::Justification::left)  ? area.getX()
                                                                    : area.getCentreX() - bounds.getWidth() * 0.5f;
        const float y = area.getCentreY()
                          + (theme.labelFont.getAscent() - theme.labelFont.getDescent()) * 0.5f;

        ga.moveRangeOfGlyphs (0, -1, x - bounds.getX(), y);
        ga.draw (g);
    }
};

} // namespace felitronics::eqview

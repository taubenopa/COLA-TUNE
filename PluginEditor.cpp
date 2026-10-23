#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour colaRed   (0xffe10600);
    const juce::Colour colaHot   (0xffff3b1f);
    const juce::Colour colaOrange(0xffff8a1f);

    juce::Font boldFont (float h) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }

    float levelToUnit (float linear)
    {
        const float db = juce::Decibels::gainToDecibels (linear, -80.f);
        return juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f);
    }

    juce::String noteName (int midi)
    {
        return juce::String (ColaScales::noteNames[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

//==============================================================================
ColaLookAndFeel::ColaLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b0b0d));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colaRed);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colours::white);
}

juce::Font ColaLookAndFeel::getComboBoxFont (juce::ComboBox&) { return boldFont (14.f); }
juce::Font ColaLookAndFeel::getPopupMenuFont() { return boldFont (14.f); }

void ColaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void ColaLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.f);
    g.setColour (juce::Colour (0xff111114).withAlpha (0.9f));
    g.fillRoundedRectangle (r, 8.f);
    const float pulse = 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.004f);
    g.setColour (colaRed.withAlpha (box.isPopupActive() ? 1.f : 0.55f + 0.2f * pulse));
    g.drawRoundedRectangle (r, 8.f, 1.6f);

    juce::Path arrow;
    const float cx = (float) w - 16.f, cy = (float) h * 0.5f;
    arrow.addTriangle (cx - 5.f, cy - 2.f, cx + 5.f, cy - 2.f, cx, cy + 4.f);
    g.setColour (colaOrange);
    g.fillPath (arrow);
}

void ColaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 6.f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
    const float hover = s.isMouseOverOrDragging() ? 1.f : 0.f;

    // tick ring
    const int ticks = 25;
    for (int i = 0; i < ticks; ++i)
    {
        const float f = (float) i / (float) (ticks - 1);
        const float a = startAngle + f * (endAngle - startAngle);
        const bool lit = f <= pos + 0.001f;
        const float r1 = radius + 1.f, r2 = radius + (i % 6 == 0 ? 7.f : 4.f);
        g.setColour (lit ? colaHot.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.12f));
        g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1,
                    c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, lit ? 2.f : 1.2f);
    }

    const float bodyR = radius * 0.78f;
    // body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b30), c.x, c.y - bodyR,
                                             juce::Colour (0xff09090b), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f, 2.f);

    // track + value arc (glow)
    const float arcR = radius * 0.92f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, angle, true);
    const float breathe = 0.5f + 0.5f * std::sin (t * 3.f);
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (colaRed.withAlpha ((0.06f + 0.05f * breathe + 0.06f * hover) * (float) (4 - k)));
        g.strokePath (val, juce::PathStrokeType (3.f + (float) k * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setGradientFill (juce::ColourGradient (colaOrange, c.x - arcR, c.y, colaRed, c.x + arcR, c.y, false));
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    const float p1 = bodyR * 0.35f, p2 = bodyR * 0.92f;
    g.setColour (juce::Colours::white);
    g.drawLine (c.x + std::sin (angle) * p1, c.y - std::cos (angle) * p1,
                c.x + std::sin (angle) * p2, c.y - std::cos (angle) * p2, 3.f);
    g.setColour (colaHot);
    g.fillEllipse (c.x + std::sin (angle) * p2 - 3.f, c.y - std::cos (angle) * p2 - 3.f, 6.f, 6.f);
}

//==============================================================================
ColaTuneEditor::ColaTuneEditor (ColaTuneProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (900, 580);
    setResizable (false, false);

    heroImage = juce::ImageCache::getFromMemory (BinaryData::youngcola_png, BinaryData::youngcola_pngSize);

    // pre-render a red glow around the cut-out silhouette (once)
    if (heroImage.isValid())
    {
        heroGlow = juce::Image (juce::Image::ARGB, heroImage.getWidth() + glowPad * 2,
                                heroImage.getHeight() + glowPad * 2, true);
        juce::Graphics gg (heroGlow);
        gg.setOrigin (glowPad, glowPad);
        juce::DropShadow (colaRed.withAlpha (1.f), 26, {}).drawForImage (gg, heroImage);
    }

    auto setupKnob = [this] (juce::Slider& s, const juce::String& suffix, int decimals, double def)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        s.setTextValueSuffix (suffix);
        s.setNumDecimalPlacesToDisplay (decimals);
        s.setDoubleClickReturnValue (true, def);
        addAndMakeVisible (s);
    };
    setupKnob (retune, " ms", 0, 20.0);
    setupKnob (amount, " %", 0, 100.0);
    setupKnob (mix, " %", 0, 100.0);
    setupKnob (detune, " ct", 1, 0.0);
    setupKnob (gain, " dB", 1, 0.0);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    aRetune = std::make_unique<SA> (proc.apvts, "retune", retune);
    aAmount = std::make_unique<SA> (proc.apvts, "amount", amount);
    aMix    = std::make_unique<SA> (proc.apvts, "mix", mix);
    aDetune = std::make_unique<SA> (proc.apvts, "detune", detune);
    aGain   = std::make_unique<SA> (proc.apvts, "gain", gain);

    for (int i = 0; i < 12; ++i) keyBox.addItem (ColaScales::noteNames[i], i + 1);
    int si = 1;
    for (auto& s : ColaScales::all()) scaleBox.addItem (s.name, si++);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);

    if (auto* k = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("key")))
        keyBox.setSelectedItemIndex (k->getIndex(), juce::dontSendNotification);
    if (auto* sc = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("scale")))
        scaleBox.setSelectedItemIndex (sc->getIndex(), juce::dontSendNotification);

    keyBox.onChange   = [this] { setChoice ("key", keyBox.getSelectedItemIndex());     applyScale(); };
    scaleBox.onChange = [this] { setChoice ("scale", scaleBox.getSelectedItemIndex()); applyScale(); };

    histDet.fill (std::numeric_limits<float>::quiet_NaN());
    histCor.fill (std::numeric_limits<float>::quiet_NaN());
    for (auto& b : bubbles)
        b = { rng.nextFloat() * 900.f, rng.nextFloat() * 580.f, 0.4f + rng.nextFloat() * 1.2f,
              1.5f + rng.nextFloat() * 5.f, rng.nextFloat() * 6.28f, 0.1f + rng.nextFloat() * 0.25f };

    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
}

ColaTuneEditor::~ColaTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ColaTuneEditor::setChoice (const juce::String& id, int index)
{
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        *c = index;
}

void ColaTuneEditor::applyScale()
{
    const int root = juce::jmax (0, keyBox.getSelectedItemIndex());
    const int sc   = juce::jmax (0, scaleBox.getSelectedItemIndex());
    const auto& notes = ColaScales::all()[(size_t) sc].notes;

    for (int i = 0; i < 12; ++i)
    {
        const int rel = ((i - root) % 12 + 12) % 12;
        const bool on = std::find (notes.begin(), notes.end(), rel) != notes.end();
        if (auto* b = dynamic_cast<juce::AudioParameterBool*> (proc.apvts.getParameter ("n" + juce::String (i))))
            *b = on;
    }
}

bool ColaTuneEditor::noteEnabled (int pc) const
{
    return proc.apvts.getRawParameterValue ("n" + juce::String (((pc % 12) + 12) % 12))->load() > 0.5f;
}

void ColaTuneEditor::resized()
{
    keyBox.setBounds (590, 30, 80, 30);
    scaleBox.setBounds (680, 30, 200, 30);

    const int y = 412, h = 150, w = 108;
    juce::Slider* knobs[] = { &retune, &amount, &mix, &detune, &gain };
    for (int i = 0; i < 5; ++i)
        knobs[i]->setBounds (340 + i * w, y, w, h);
}

//==============================================================================
juce::Rectangle<float> ColaTuneEditor::keyRect (int note) const
{
    static const int whiteIdx[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const int blackLeft[12] = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };
    const auto kb = keyArea.toFloat();
    const float ww = kb.getWidth() / 7.f;
    if (whiteIdx[note] >= 0)
        return { kb.getX() + (float) whiteIdx[note] * ww, kb.getY(), ww, kb.getHeight() };
    const float bw = ww * 0.62f;
    return { kb.getX() + (float) (blackLeft[note] + 1) * ww - bw * 0.5f, kb.getY(), bw, kb.getHeight() * 0.6f };
}

void ColaTuneEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! keyArea.contains (e.getPosition())) return;
    const auto pt = e.position;
    int hit = -1;
    for (int n : { 1, 3, 6, 8, 10 })                  // black keys are on top
        if (keyRect (n).contains (pt)) { hit = n; break; }
    if (hit < 0)
        for (int n : { 0, 2, 4, 5, 7, 9, 11 })
            if (keyRect (n).contains (pt)) { hit = n; break; }
    if (hit < 0) return;

    if (auto* p = proc.apvts.getParameter ("n" + juce::String (hit)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.f : 1.f);
        p->endChangeGesture();
    }
}

//==============================================================================
void ColaTuneEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.001, 0.1, now - lastTime);
    lastTime = now;
    time += dt;

    inLvl  = levelToUnit (proc.uiIn.load());
    outLvl = levelToUnit (proc.uiOut.load());
    const float target = outLvl;
    lvl += (target > lvl ? 0.4f : 0.08f) * (target - lvl);

    voiced  = proc.uiVoiced.load();
    detMidi = proc.uiDetectedMidi.load();
    corMidi = proc.uiCorrectedMidi.load();

    histDet[(size_t) histPos] = voiced ? detMidi : std::numeric_limits<float>::quiet_NaN();
    histCor[(size_t) histPos] = voiced ? corMidi : std::numeric_limits<float>::quiet_NaN();
    histPos = (histPos + 1) % histN;
    if (voiced) viewCenter += 0.06f * (corMidi - viewCenter);

    // spectrum
    {
        const int wpos = proc.scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = proc.scopeRing[(wpos - fftSize + i) & (ColaTuneProcessor::scopeSize - 1)].load (std::memory_order_relaxed);
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float sr = (float) proc.uiSampleRate.load();
        for (int b = 0; b < numBars; ++b)
        {
            const float f0 = 80.f * std::pow (14000.f / 80.f, (float) b / numBars);
            const float f1 = 80.f * std::pow (14000.f / 80.f, (float) (b + 1) / numBars);
            const int i0 = juce::jlimit (1, fftSize / 2 - 2, (int) (f0 * fftSize / sr));
            const int i1 = juce::jlimit (i0 + 1, fftSize / 2 - 1, (int) std::ceil (f1 * fftSize / sr));
            float m = 0.f;
            for (int i = i0; i <= i1; ++i) m = std::max (m, fftData[(size_t) i]);
            const float db = juce::Decibels::gainToDecibels (m / (fftSize * 0.25f), -100.f);
            const float v = juce::jlimit (0.f, 1.f, (db + 75.f) / 65.f);
            bars[(size_t) b] = v > bars[(size_t) b] ? bars[(size_t) b] + 0.6f * (v - bars[(size_t) b])
                                                    : bars[(size_t) b] * 0.90f;
        }
    }

    // bubbles
    for (auto& b : bubbles)
    {
        b.y -= (b.vy + lvl * 4.f) * 60.f * dt;
        b.x += std::sin ((float) time * 1.5f + b.ph) * 0.4f;
        if (b.y < -10.f) { b.y = 590.f; b.x = rng.nextFloat() * 900.f; }
    }

    // random glitch bursts on the logo
    if (glitchFrames > 0) --glitchFrames;
    else if (rng.nextFloat() < 0.008f + lvl * 0.02f) glitchFrames = 4 + rng.nextInt (6);

    repaint();
}

//==============================================================================
void ColaTuneEditor::paint (juce::Graphics& g)
{
    const float t = (float) time;
    drawBackground (g, t);
    drawHero (g, t);
    drawGraph (g, t);
    drawKeys (g, t);
    drawMeters (g, t);
    drawTitle (g, t);

    // knob captions
    g.setFont (boldFont (12.f));
    const char* names[] = { "RETUNE SPEED", "AMOUNT", "MIX", "DETUNE", "OUTPUT" };
    juce::Slider* knobs[] = { &retune, &amount, &mix, &detune, &gain };
    for (int i = 0; i < 5; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (names[i], knobs[i]->getX(), knobs[i]->getY() - 14, knobs[i]->getWidth(), 14, juce::Justification::centred);
    }

    g.setFont (boldFont (10.f));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawText ("KEY", keyBox.getX(), keyBox.getY() - 13, 60, 12, juce::Justification::left);
    g.drawText ("SCALE", scaleBox.getX(), scaleBox.getY() - 13, 60, 12, juce::Justification::left);
}

void ColaTuneEditor::drawBackground (juce::Graphics& g, float t)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff050506));

    const juce::Colour cols[] = { colaRed, colaOrange, juce::Colour (0xff7a0010) };
    for (int i = 0; i < 3; ++i)
    {
        const float cx = b.getWidth()  * (0.5f + 0.45f * std::sin (t * 0.30f + (float) i * 2.1f));
        const float cy = b.getHeight() * (0.5f + 0.45f * std::cos (t * 0.23f + (float) i * 1.7f));
        const float r = 330.f + 160.f * lvl;
        g.setGradientFill (juce::ColourGradient (cols[i].withAlpha (0.26f + 0.30f * lvl), cx, cy,
                                                 cols[i].withAlpha (0.f), cx + r, cy, true));
        g.fillRect (b);
    }

    // bubbles
    for (auto& bb : bubbles)
    {
        g.setColour (juce::Colours::white.withAlpha (bb.a * (0.6f + lvl)));
        g.drawEllipse (bb.x - bb.r, bb.y - bb.r, bb.r * 2.f, bb.r * 2.f, 1.f);
        g.setColour (juce::Colours::white.withAlpha (bb.a * 0.8f));
        g.fillEllipse (bb.x - bb.r * 0.45f, bb.y - bb.r * 0.55f, bb.r * 0.4f, bb.r * 0.4f);
    }

    // scanlines + vignette
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    const int off = (int) (t * 20.f) % 3;
    for (int y = off; y < getHeight(); y += 3) g.fillRect (0, y, getWidth(), 1);

    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                             juce::Colours::black.withAlpha (0.75f), 0.f, 0.f, true));
    g.fillRect (b);
}

void ColaTuneEditor::drawTitle (juce::Graphics& g, float t)
{
    const auto area = juce::Rectangle<int> (28, 4, 380, 54);
    const float shift = std::sin (t * 1.4f) * 90.f;
    juce::ColourGradient grad (colaRed, 28.f + shift, 0.f, juce::Colours::white, 330.f + shift, 0.f, false);
    grad.addColour (0.35, colaOrange);
    grad.addColour (0.7, colaHot);

    g.setFont (boldFont (46.f));
    // glow
    for (int k = 1; k <= 3; ++k)
    {
        g.setColour (colaRed.withAlpha (0.10f + 0.1f * lvl));
        g.drawText ("COLA-TUNE", area.translated (0, 0).expanded (k), juce::Justification::centredLeft);
    }

    if (glitchFrames > 0)
    {
        const int slice = rng.nextInt (40);
        for (int pass = 0; pass < 2; ++pass)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (area.getX(), area.getY() + slice, area.getWidth(), 6 + rng.nextInt (14));
            g.setColour ((pass == 0 ? juce::Colour (0xff00f0ff) : juce::Colour (0xffff0055)).withAlpha (0.8f));
            g.drawText ("COLA-TUNE", area.translated ((pass == 0 ? -1 : 1) * (4 + rng.nextInt (8)), 0),
                        juce::Justification::centredLeft);
        }
    }
    g.setGradientFill (grad);
    g.drawText ("COLA-TUNE", area, juce::Justification::centredLeft);

    g.setFont (boldFont (11.f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("YOUNGCOLA EDITION  //  UNDERGROUND AUTO-TUNE", 30, 56, 420, 14, juce::Justification::left);
}

void ColaTuneEditor::drawHero (juce::Graphics& g, float t)
{
    const auto area = heroArea.toFloat();
    const auto c = area.getCentre();

    // frame
    g.setColour (juce::Colour (0xff0a0a0c).withAlpha (0.55f));
    g.fillRoundedRectangle (area, 18.f);

    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (area, 18.f);
    g.reduceClipRegion (clip);

    // pulse glow
    g.setGradientFill (juce::ColourGradient (colaRed.withAlpha (0.30f + 0.5f * lvl), c.x, c.y,
                                             colaRed.withAlpha (0.f), c.x, c.y - 200.f, true));
    g.fillRect (area);

    // radial spectrum
    const float rot = t * 0.25f;
    for (int i = 0; i < numBars; ++i)
    {
        const float a = rot + (float) i * juce::MathConstants<float>::twoPi / numBars;
        const float v = bars[(size_t) i];
        const float r0 = 112.f, r1 = r0 + 6.f + 34.f * v;
        g.setColour (juce::Colour::fromHSV (0.02f + 0.06f * v, 1.f, 1.f, 0.35f + 0.65f * v));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0,
                    c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, 3.f);
    }

    // rotating dashed rings
    for (int ring = 0; ring < 2; ++ring)
    {
        const float rr = ring == 0 ? 146.f : 98.f;
        const float dir = ring == 0 ? 1.f : -1.f;
        for (int k = 0; k < 3; ++k)
        {
            const float a0 = dir * t * (ring == 0 ? 0.9f : 1.4f) + (float) k * juce::MathConstants<float>::twoPi / 3.f;
            juce::Path p;
            p.addCentredArc (c.x, c.y, rr, rr, 0.f, a0, a0 + 1.0f, true);
            g.setColour ((ring == 0 ? colaOrange : colaHot).withAlpha (0.35f + 0.5f * lvl));
            g.strokePath (p, juce::PathStrokeType (1.5f + 3.f * lvl, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // YOUNGCOLA photo: floats, tilts and breathes with the output level
    if (heroImage.isValid())
    {
        const auto src = heroImage.getBounds().toFloat();
        const auto dest = juce::RectanglePlacement (juce::RectanglePlacement::centred)
                              .appliedTo (src, area.reduced (14.f, 18.f));
        const float sc = dest.getWidth() / src.getWidth() * (1.f + 0.05f * lvl);
        const float bob = std::sin (t * 1.8f) * 5.f - lvl * 6.f;
        const float tilt = std::sin (t * 0.9f) * 0.04f + (std::sin (t * 23.f) * 0.012f * lvl);

        const auto cx = dest.getCentreX(), cy = dest.getCentreY() + bob;
        auto place = [&] (float padPx)
        {
            return juce::AffineTransform::translation (-src.getWidth() * 0.5f - padPx, -src.getHeight() * 0.5f - padPx)
                       .scaled (sc).rotated (tilt).translated (cx, cy);
        };

        g.setOpacity (0.45f + 0.55f * lvl + 0.1f * std::sin (t * 4.f));
        if (heroGlow.isValid()) g.drawImageTransformed (heroGlow, place ((float) glowPad), false);
        g.setOpacity (1.f);
        g.drawImageTransformed (heroImage, place (0.f), false);
    }

    g.setColour (colaRed.withAlpha (0.6f));
    g.drawRoundedRectangle (area, 18.f, 1.5f);
}

void ColaTuneEditor::drawGraph (juce::Graphics& g, float t)
{
    const auto area = graphArea.toFloat();
    g.setColour (juce::Colour (0xff09090b).withAlpha (0.65f));
    g.fillRoundedRectangle (area, 14.f);

    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addRoundedRectangle (area, 14.f);
    g.reduceClipRegion (clip);

    const float range = 18.f;
    const float lowM = viewCenter - range * 0.5f;
    auto yOf = [&] (float m) { return area.getBottom() - (m - lowM) / range * area.getHeight(); };

    for (int k = (int) std::floor (lowM) - 1; k <= (int) std::ceil (lowM + range) + 1; ++k)
    {
        const bool on = noteEnabled (k);
        const float y = yOf ((float) k);
        if (on)
        {
            g.setColour (colaRed.withAlpha (0.07f));
            g.fillRect (area.getX(), yOf ((float) k + 0.5f), area.getWidth(), yOf ((float) k - 0.5f) - yOf ((float) k + 0.5f));
        }
        g.setColour (on ? colaHot.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        if (on)
        {
            g.setFont (boldFont (10.f));
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (noteName (k), (int) area.getX() + 8, (int) y - 6, 40, 12, juce::Justification::left);
        }
    }

    auto buildPath = [&] (const std::array<float, histN>& arr)
    {
        juce::Path p;
        bool pen = false;
        for (int i = 0; i < histN; ++i)
        {
            const float v = arr[(size_t) ((histPos + i) % histN)];
            if (std::isnan (v)) { pen = false; continue; }
            const float x = area.getX() + (float) i / (histN - 1) * area.getWidth();
            const float y = juce::jlimit (area.getY(), area.getBottom(), yOf (v));
            if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
        }
        return p;
    };

    const auto pDet = buildPath (histDet), pCor = buildPath (histCor);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.strokePath (pDet, juce::PathStrokeType (1.4f));
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (colaOrange.withAlpha (0.07f * (float) (4 - k)));
        g.strokePath (pCor, juce::PathStrokeType (2.f + (float) k * 3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (colaHot);
    g.strokePath (pCor, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // big note read-out
    auto ro = juce::Rectangle<int> ((int) area.getRight() - 170, (int) area.getY() + 6, 160, 60);
    if (voiced)
    {
        const int nearest = (int) std::lround (corMidi);
        const float cents = (corMidi - (float) nearest) * 100.f;
        g.setFont (boldFont (50.f));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawText (noteName (nearest), ro.removeFromTop (46), juce::Justification::centredRight);
        g.setFont (boldFont (12.f));
        g.setColour (colaOrange);
        g.drawText ((cents >= 0 ? "+" : "") + juce::String ((int) std::lround (cents)) + " ct", ro, juce::Justification::centredRight);
    }
    else
    {
        g.setFont (boldFont (50.f));
        g.setColour (juce::Colours::white.withAlpha (0.18f + 0.1f * std::sin (t * 3.f)));
        g.drawText ("--", ro, juce::Justification::centredRight);
    }

    g.setColour (colaRed.withAlpha (0.5f));
    g.drawRoundedRectangle (area, 14.f, 1.2f);
}

void ColaTuneEditor::drawKeys (juce::Graphics& g, float t)
{
    const int active = voiced ? ((int) std::lround (corMidi) % 12 + 12) % 12 : -1;
    const float pulse = 0.5f + 0.5f * std::sin (t * 8.f);

    auto drawKey = [&] (int n, bool black)
    {
        auto r = keyRect (n).reduced (black ? 0.f : 1.5f, 0.f);
        const bool on = noteEnabled (n);
        juce::ColourGradient grad = on
            ? juce::ColourGradient (black ? juce::Colour (0xffff7a3a) : juce::Colour (0xffff4d2e), r.getX(), r.getY(),
                                    black ? juce::Colour (0xffb01208) : juce::Colour (0xff8f0e0a), r.getX(), r.getBottom(), false)
            : juce::ColourGradient (black ? juce::Colour (0xff0a0a0c) : juce::Colour (0xff2c2c31), r.getX(), r.getY(),
                                    black ? juce::Colour (0xff1c1c20) : juce::Colour (0xff131316), r.getX(), r.getBottom(), false);
        juce::Path p; p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 6.f, 6.f, false, false, true, true);
        if (on)
        {
            g.setColour (colaRed.withAlpha (0.12f + 0.1f * lvl));
            g.fillPath (p, juce::AffineTransform::scale (1.08f, 1.04f, r.getCentreX(), r.getY()));
        }
        g.setGradientFill (grad);
        g.fillPath (p);
        if (n == active)
        {
            g.setColour (juce::Colours::white.withAlpha (0.30f + 0.25f * pulse));
            g.fillPath (p);
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (1.4f));
        if (! black)
        {
            g.setFont (boldFont (11.f));
            g.setColour (juce::Colours::white.withAlpha (on ? 0.9f : 0.35f));
            g.drawText (ColaScales::noteNames[n], r.withTrimmedTop (r.getHeight() - 20.f), juce::Justification::centred);
        }
    };

    for (int n : { 0, 2, 4, 5, 7, 9, 11 }) drawKey (n, false);
    for (int n : { 1, 3, 6, 8, 10 })       drawKey (n, true);
}

void ColaTuneEditor::drawMeters (juce::Graphics& g, float t)
{
    auto drawMeter = [&] (float level, int y, const char* label)
    {
        g.setFont (boldFont (10.f));
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (label, 22, y - 1, 28, 12, juce::Justification::left);
        const int segs = 30;
        for (int i = 0; i < segs; ++i)
        {
            const float f = (float) i / segs;
            const bool lit = f < level;
            g.setColour (lit ? juce::Colour::fromHSV (0.09f - 0.09f * f, 1.f, 1.f, 0.95f)
                             : juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (52.f + (float) i * 9.f, (float) y, 7.f, 10.f, 2.f);
        }
    };
    drawMeter (inLvl,  490, "IN");
    drawMeter (outLvl, 506, "OUT");

    juce::ColourGradient grad (colaRed, 20.f + std::sin (t) * 60.f, 0.f, juce::Colours::white, 300.f, 0.f, false);
    grad.addColour (0.5, colaOrange);
    g.setGradientFill (grad);
    g.setFont (boldFont (28.f));
    g.drawText ("YOUNGCOLA", 20, 524, 300, 34, juce::Justification::centred);
}

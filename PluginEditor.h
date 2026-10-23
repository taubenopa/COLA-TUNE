#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

//==============================================================================
// Custom look: glowing cola-red knobs, dark glass combo boxes
//==============================================================================
class ColaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ColaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

//==============================================================================
class ColaTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ColaTuneEditor (ColaTuneProcessor&);
    ~ColaTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void applyScale();
    void setChoice (const juce::String& id, int index);

    juce::Rectangle<float> keyRect (int note) const;
    bool noteEnabled (int pc) const;

    void drawBackground (juce::Graphics&, float t);
    void drawTitle (juce::Graphics&, float t);
    void drawHero (juce::Graphics&, float t);
    void drawGraph (juce::Graphics&, float t);
    void drawKeys (juce::Graphics&, float t);
    void drawMeters (juce::Graphics&, float t);

    ColaTuneProcessor& proc;
    ColaLookAndFeel laf;

    juce::Slider retune, amount, mix, detune, gain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aRetune, aAmount, aMix, aDetune, aGain;
    juce::ComboBox keyBox, scaleBox;

    juce::Image heroImage, heroGlow;
    int glowPad = 40;

    // layout
    juce::Rectangle<int> heroArea { 20, 80, 300, 400 };
    juce::Rectangle<int> graphArea { 340, 80, 540, 200 };
    juce::Rectangle<int> keyArea { 340, 298, 540, 88 };

    // animation state
    double time = 0.0, lastTime = 0.0;
    float lvl = 0.f, inLvl = 0.f, outLvl = 0.f;
    bool voiced = false;
    float detMidi = 0.f, corMidi = 0.f, viewCenter = 60.f;
    int glitchFrames = 0;
    juce::Random rng;

    static constexpr int histN = 260;
    std::array<float, histN> histDet, histCor;
    int histPos = 0;

    struct Bubble { float x, y, vy, r, ph, a; };
    std::array<Bubble, 90> bubbles;

    static constexpr int fftOrder = 10, fftSize = 1 << fftOrder, numBars = 48;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, numBars> bars {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColaTuneEditor)
};

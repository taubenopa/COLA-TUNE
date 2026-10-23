#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ColaTuneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "retune", 1 }, "Retune Speed",
                NormalisableRange<float> (1.f, 400.f, 0.1f, 0.4f), 20.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "detune", 1 }, "Detune",
                NormalisableRange<float> (-50.f, 50.f, 0.1f), 0.f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Output",
                NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));

    StringArray keys, scales;
    for (auto* n : ColaScales::noteNames) keys.add (n);
    for (auto& s : ColaScales::all()) scales.add (s.name);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keys, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { "n" + String (i), 1 },
                    String ("Note ") + ColaScales::noteNames[i], true));
    return layout;
}

//==============================================================================
ColaTuneProcessor::ColaTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "COLATUNE", createLayout())
{
    pRetune = apvts.getRawParameterValue ("retune");
    pAmount = apvts.getRawParameterValue ("amount");
    pMix    = apvts.getRawParameterValue ("mix");
    pDetune = apvts.getRawParameterValue ("detune");
    pGain   = apvts.getRawParameterValue ("gain");
    for (int i = 0; i < 12; ++i) pNote[i] = apvts.getRawParameterValue ("n" + juce::String (i));
    for (auto& s : scopeRing) s.store (0.f);
}

bool ColaTuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == l.getMainInputChannelSet();
}

void ColaTuneProcessor::prepareToPlay (double sr, int)
{
    detector.prepare (sr);
    shifter.prepare (sr);
    grainTarget = shifter.defaultGrain();
    offsetSemis = targetOffset = 0.f; voiced = false; wetGate = 0.f; currentNote = -1; histCount = 0;
    gainSmooth.reset (sr, 0.02);
    gainSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    setLatencySamples ((int) (0.006 * sr));
    uiSampleRate.store (sr);
}

//==============================================================================
void ColaTuneProcessor::updateTarget (const bool* mask, float amount, float detuneSemis)
{
    if (! detector.voiced)
    {
        voiced = false; targetOffset = 0.f; currentNote = -1; histCount = 0;
        return;
    }

    const float midi = 69.f + 12.f * std::log2 (detector.hz / 440.f);
    midiHist[2] = midiHist[1]; midiHist[1] = midiHist[0]; midiHist[0] = midi;
    if (histCount < 3) ++histCount;
    const float a = midiHist[0], b = midiHist[1], c = midiHist[2];
    const float m = histCount < 3 ? midi : std::max (std::min (a, b), std::min (std::max (a, b), c)); // median-of-3

    // nearest allowed note
    const int base = (int) std::lround (m);
    int bestNote = -1; float bestDist = 1e9f;
    for (int k = -7; k <= 7; ++k)
    {
        const int cand = base + k;
        if (! mask[((cand % 12) + 12) % 12]) continue;
        const float dist = std::abs (m - (float) cand);
        if (dist < bestDist) { bestDist = dist; bestNote = cand; }
    }

    float target = m;
    if (bestNote >= 0)
    {
        // hysteresis: don't flip between two notes on the boundary
        if (currentNote >= 0 && currentNote != bestNote && mask[((currentNote % 12) + 12) % 12]
            && std::abs (m - (float) currentNote) < bestDist + 0.25f)
            bestNote = currentNote;
        currentNote = bestNote;
        target = (float) bestNote;
    }

    voiced = true;
    detectedMidiState = m;
    targetOffset = (target - m) * amount + detuneSemis;
    grainTarget = shifter.grainForPeriod (detector.periodSamples);
}

void ColaTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin (2, buffer.getNumChannels());
    const int n   = buffer.getNumSamples();
    if (nCh == 0) return;
    for (int c = nCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    const float sr = (float) getSampleRate();
    const float retuneMs = pRetune->load();
    const float amount   = pAmount->load() * 0.01f;
    const float mix      = pMix->load() * 0.01f;
    const float detuneSt = pDetune->load() * 0.01f;
    gainSmooth.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    bool mask[12];
    for (int i = 0; i < 12; ++i) mask[i] = pNote[i]->load() > 0.5f;

    const float retuneCoef  = retuneMs <= 1.5f ? 1.f : 1.f - std::exp (-1.f / (retuneMs * 0.001f * sr));
    const float releaseCoef = 1.f - std::exp (-1.f / (0.040f * sr));
    const float gateCoef    = 1.f - std::exp (-1.f / (0.005f * sr));

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = nCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPeak = 0.f, outPeak = 0.f;
    int sw = scopeWrite.load (std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        const float in0 = ch0[i];
        const float in1 = ch1 ? ch1[i] : in0;
        inPeak = std::max (inPeak, std::max (std::abs (in0), std::abs (in1)));

        if (detector.push (0.5f * (in0 + in1)))
            updateTarget (mask, amount, detuneSt);

        offsetSemis += (voiced ? retuneCoef : releaseCoef) * (targetOffset - offsetSemis);
        wetGate     += gateCoef * ((voiced ? 1.f : 0.f) - wetGate);

        shifter.write (in0, in1);
        float wet0, dry0, wet1 = 0.f, dry1 = 0.f;
        shifter.read (0, wet0, dry0);
        shifter.read (1, wet1, dry1);
        shifter.advance (std::exp2 (offsetSemis / 12.f), grainTarget);

        const float p0 = wetGate * wet0 + (1.f - wetGate) * dry0;
        const float p1 = wetGate * wet1 + (1.f - wetGate) * dry1;
        const float g  = gainSmooth.getNextValue();
        const float o0 = (mix * p0 + (1.f - mix) * dry0) * g;
        const float o1 = (mix * p1 + (1.f - mix) * dry1) * g;

        ch0[i] = o0;
        if (ch1) ch1[i] = o1;

        outPeak = std::max (outPeak, std::max (std::abs (o0), std::abs (o1)));
        scopeRing[sw & (scopeSize - 1)].store (0.5f * (o0 + o1), std::memory_order_relaxed);
        ++sw;
    }
    scopeWrite.store (sw, std::memory_order_release);

    uiIn.store (std::max (inPeak, uiIn.load() * 0.85f));
    uiOut.store (std::max (outPeak, uiOut.load() * 0.85f));
    uiVoiced.store (voiced);
    uiDetectedMidi.store (voiced ? detectedMidiState : 0.f);
    uiCorrectedMidi.store (voiced ? detectedMidiState + offsetSemis : 0.f);
}

//==============================================================================
juce::AudioProcessorEditor* ColaTuneProcessor::createEditor() { return new ColaTuneEditor (*this); }

void ColaTuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ColaTuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ColaTuneProcessor(); }

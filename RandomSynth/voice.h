#pragma once
#include "synth_dc.h"
#include "mixer.h"
#include "input_i2s.h"
#include "synth_waveform.h"
#include <Arduino.h>
#include <Audio.h>
#include "synth_karplusstronger.h"
#include "interpolate.h"
#include "AudioInputToInt.h"

#define STRING 0
#define SINE 1
#define WAVETABLE 2


AudioInputI2S mic;

class Voice {
public:
  AudioSynthKarplusStronger string;
  AudioAmplifier stringAmplitude;
  AudioSynthWaveformModulated sine;
  AudioMixer4 fmModulator;
  AudioSynthWaveformModulated oscillatorOne;
  AudioSynthWaveformModulated oscillatorTwo;
  AudioSynthWaveformModulated oscillatorThree;
  AudioSynthWaveformModulated oscillatorFour;
  AudioSynthWaveform lfo;
  AudioSynthWaveform lfo2;
  AudioEffectEnvelope lfo2Envelope;
  AudioMixer4 filterModBlend;
  AudioMixer4 voiceMixer;
  AudioMixer4 waveMixer;
  AudioFilterLadder voiceFilter;
  AudioEffectEnvelope filterEnvelope;
  AudioEffectEnvelope voiceEnvelope;
  AudioEffectEnvelope fmEnvelope;
  AudioSynthWaveformDc filterAmount;
  AudioInterpolate interpolator[2];
  AudioAmplifier filterAttenuation;
  AudioInputToInt reader;
  AudioInputToInt lfo3Reader;
  AudioSynthWaveform lfo3;

  bool isSustain = false;
  char oscOneIndex = 0;
  char oscTwoIndex = 0;

  struct PitchParameters {
    float baseFrequency = 0;  // The fundamental frequency of the note
    float detuneAmount = 1;   // Amount to detune the second oscillator
    int octaveOffset = 0;     // Octave shift
    float vibratoAmount = 1;
    float pitchBend = 1;
  };

  PitchParameters pitchParams;

  // Connections within a voice
  AudioConnection* patchCords[25];

  Voice() {
    patchCords[0] = new AudioConnection(fmModulator, 0, fmEnvelope, 0);
    patchCords[1] = new AudioConnection(fmEnvelope, 0, sine, 0);
    patchCords[2] = new AudioConnection(string, 0, fmModulator, 0);
    patchCords[3] = new AudioConnection(sine, 0, fmModulator, 1);
    patchCords[4] = new AudioConnection(oscillatorOne, 0, fmModulator, 2);
    patchCords[5] = new AudioConnection(oscillatorTwo, 0, fmModulator, 3);

    patchCords[6] = new AudioConnection(string, 0, stringAmplitude, 0);
    patchCords[7] = new AudioConnection(stringAmplitude, 0, voiceMixer, 0);
    patchCords[8] = new AudioConnection(sine, 0, voiceMixer, 1);
    patchCords[9] = new AudioConnection(oscillatorOne, 0, interpolator[0], 0);
    patchCords[10] = new AudioConnection(oscillatorTwo, 0, interpolator[0], 1);
    patchCords[11] = new AudioConnection(oscillatorThree, 0, interpolator[1], 0);
    patchCords[12] = new AudioConnection(oscillatorFour, 0, interpolator[1], 1);

    patchCords[13] = new AudioConnection(interpolator[0], 0, waveMixer, 0);
    patchCords[14] = new AudioConnection(interpolator[1], 0, waveMixer, 1);
    patchCords[15] = new AudioConnection(waveMixer, 0, voiceMixer, 2);
    patchCords[16] = new AudioConnection(voiceMixer, 0, voiceFilter, 0);
    patchCords[17] = new AudioConnection(filterAmount, 0, filterModBlend, 0);
    patchCords[18] = new AudioConnection(lfo2, 0, lfo2Envelope, 0);
    // patchCords[18] = new AudioConnection(lfo2, 0, voiceFilter, 1);
    patchCords[19] = new AudioConnection(lfo2Envelope, 0, filterModBlend, 1);
    patchCords[20] = new AudioConnection(filterModBlend, 0, filterEnvelope, 0);
    patchCords[21] = new AudioConnection(filterEnvelope, 0, voiceFilter, 1);
    patchCords[22] = new AudioConnection(voiceFilter, 0, filterAttenuation, 0);
    patchCords[22] = new AudioConnection(filterAttenuation, 0, voiceEnvelope, 0);
    patchCords[23] = new AudioConnection(lfo, reader);
    patchCords[24] = new AudioConnection(lfo3, lfo3Reader);

    lfo.begin(WAVEFORM_TRIANGLE);
    lfo.frequency(2);
    lfo.amplitude(1);

    lfo2.begin(WAVEFORM_TRIANGLE);
    lfo2.frequency(10);
    lfo2.amplitude(1);

    oscillatorOne.begin(WAVEFORM_ARBITRARY);
    oscillatorTwo.begin(WAVEFORM_ARBITRARY);
    oscillatorThree.begin(WAVEFORM_ARBITRARY);
    oscillatorFour.begin(WAVEFORM_ARBITRARY);
    sine.begin(WAVEFORM_SINE);
    // sine.amplitude(1);

    fmModulator.gain(STRING, 0.0);
    fmModulator.gain(SINE, 0.0);
    fmModulator.gain(WAVETABLE, 0.0);

    waveMixer.gain(0, 0.5);
    waveMixer.gain(1, 0.5);

    voiceMixer.gain(STRING, 0.0);
    voiceMixer.gain(SINE, 0.0);
    voiceMixer.gain(WAVETABLE, 0.0);

    sine.frequency(1);
    sine.amplitude(1);

    oscillatorOne.frequencyModulation(1);
    oscillatorTwo.frequencyModulation(1);
    oscillatorThree.frequencyModulation(1);
    oscillatorFour.frequencyModulation(1);
    sine.frequencyModulation(1);
  }

  void noteOn(float noteFrequency, float velocity) {
    float amplitude = velocity / 127;
    pitchParams.baseFrequency = noteFrequency;
    string.noteOn(noteFrequency, 1);
    stringAmplitude.gain(amplitude);
    sine.frequency(noteFrequency);
    sine.amplitude(amplitude);
    oscillatorOne.begin(amplitude, noteFrequency, WAVEFORM_ARBITRARY);
    oscillatorTwo.begin(amplitude, noteFrequency, WAVEFORM_ARBITRARY);
    oscillatorThree.begin(amplitude, noteFrequency, WAVEFORM_ARBITRARY);
    oscillatorFour.begin(amplitude, noteFrequency, WAVEFORM_ARBITRARY);

    voiceEnvelope.noteOn();
    filterEnvelope.noteOn();
    fmEnvelope.noteOn();
    lfo2Envelope.noteOn();
    isSustain = true;
    lastUsedTimestamp = millis();
  }

  void noteOff() {
    // string.noteOff(0);
    isSustain = false;
    voiceEnvelope.noteOff();
    filterEnvelope.noteOff();
    fmEnvelope.noteOff();
    lfo2Envelope.noteOff();
  }

  void setAmplitude(float input) {
    float amplitude = input / 127;
    stringAmplitude.gain(amplitude);
    sine.amplitude(amplitude);
    oscillatorOne.amplitude(amplitude);
    oscillatorTwo.amplitude(amplitude);
    oscillatorThree.amplitude(amplitude);
    oscillatorFour.amplitude(amplitude);
    Serial.println(amplitude);
  }



  void wavetableMorph(float value) {
    interpolator[0].setInterpolationFactor(value);
    interpolator[1].setInterpolationFactor(value);
  }

  bool isActive() {
    return voiceEnvelope.isActive();
  }

  unsigned long getLastUsedTimestamp() const {
    return lastUsedTimestamp;
  }

  void updateOscillatorFrequencies() {
    float baseFreq = pitchParams.baseFrequency * pow(2.0, pitchParams.octaveOffset);  // Base frequency with octave offset
    // float vibratoEffect = 1.0 + (sin(lfoPhase) * vibratoDepth);                       // LFO-based vibrato effect                        // Pitch bend effect

    // Apply vibrato and pitch bend uniformly to base frequency
    baseFreq *= pitchParams.vibratoAmount * pitchParams.pitchBend;
    // baseFreq *= pitchParams.pitchBend;

    // Apply detuning to oscillator 2
    float freqOsc1 = baseFreq / pitchParams.detuneAmount;  // Oscillator 1 frequency
    float freqOsc2 = baseFreq * pitchParams.detuneAmount;  // Oscillator 2 frequency with detuning

    oscillatorOne.frequency(freqOsc1);
    oscillatorTwo.frequency(freqOsc1);
    oscillatorThree.frequency(freqOsc2);
    oscillatorFour.frequency(freqOsc2);
    sine.frequency(baseFreq);
    string.setPitch(baseFreq);
  }

  void updateVibrato() {
    pitchParams.vibratoAmount = 1 + ((float)reader.getIntValue() / 1000000);
    Serial.println(reader.getIntValue());
    updateOscillatorFrequencies();
  }

  void vibratoOff() {
    pitchParams.vibratoAmount = 1;
  }

  // Apply a pitch bend, then recalculate oscillator frequencies
  void applyPitchBend(float bendAmount) {
    pitchParams.pitchBend = bendAmount;  // Pitch bend amount in semitones
    Serial.println(pitchParams.pitchBend);
    updateOscillatorFrequencies();
  }

  void applyDetune(float detune) {
    pitchParams.detuneAmount = detune;
    updateOscillatorFrequencies();
  }

  // Additional methods for voice control can be added here
private:

  unsigned long lastUsedTimestamp;
};

#pragma once
#include "effect_delay_ext.h"
#include <stdint.h>
#include "mixer.h"
#include "effect_delay.h"
#include "input_adc.h"
#include "voice.h"
#include "envelopeFollower.h"
#include <Audio.h>
#include <string>
#include <vector>
#include <functional>
#include "wavetables.h"

#define GRANULAR_MEMORY_SIZE 12800  // enough for 290 ms at 44.1 kHz

template<int numVoices>

class Synth {
private:
  // static constexpr int numVoices = 8;  // Number of voices, up to 64 but probably less
  static constexpr int numSubmixers = (numVoices + 3) / 4;
  static constexpr int numMixers = (numVoices + 11) / 16;
  static constexpr int channelsPerMixer = 4;
  const float PER_CHANNEL_GAIN = 0.2;
  Voice voices[numVoices];  // Array of voice objects
  int voiceNote[numVoices];
  float frequency;
  AudioMixer4 submixers[numSubmixers];
  AudioMixer4 mixers[numMixers];
  AudioMixer4 masterMixer;
  AudioAmplifier globalVolume;
  AudioAmplifier dummy;
  AudioSynthWaveform lfo;
  AudioMixer4 feedback;
  AudioEffectDelay delay;
  AudioEffectGranular granular;

  int16_t granularMemory[GRANULAR_MEMORY_SIZE];

  float bendAmount = 0;      
  float maxPitchBend = 2.0;  // Max pitch bend amount in semitones
  float detuneFactor = 0;
  float pitchBendMultiplier = 1;
  int mostRecentVoice = 0;

  AudioOutputI2S output;

public:

  struct SynthParameter {
    String name;
    std::function<void(float)> setterFunction;
    float preferredValue, weighting;
    bool modulatable;
    float currentValue;

    // Constructor to initialize a SynthParameter
    SynthParameter(const String name, std::function<void(float)> setterFunction, float preferredValue, float weighting, bool modulatable, float currentValue = 0.0f)
      : name(name), setterFunction(setterFunction), preferredValue(preferredValue), weighting(weighting), modulatable(modulatable), currentValue(currentValue) {}
  };
  std::vector<SynthParameter> parameters;

  Synth() {
    for (int i = 0; i < numVoices; i++) {
      voiceNote[i] = -1;  // Indicate that the voice is not playing any note
    }
  }

  struct MacroControl {
    std::vector<std::pair<std::function<void(float)>, std::pair<float, float>>> controls;

    // Adds a parameter control to the macro, including its value mapping range
    void addControl(std::function<void(float)> setter, float minVal, float maxVal) {
      controls.emplace_back(setter, std::make_pair(minVal, maxVal));
    }

    // Applies the macro control, mapping the input value to each parameter's range
    void apply(float value) {
      for (auto& control : controls) {
        float mappedValue = map(value, 0.0f, 127.0f, control.second.first, control.second.second);
        control.first(mappedValue);  // Call the setter function with the mapped value
      }
    }
  };

  MacroControl macroOne;
  MacroControl macroTwo;

  void begin() {

    // mixers.resize((numVoices + channelsPerMixer - 1) / channelsPerMixer);

    // Connect voices to the first level mixers
    for (int i = 0; i < numVoices; i++) {
      new AudioConnection(voices[i].voiceEnvelope, 0, submixers[i / channelsPerMixer], i % channelsPerMixer);
      submixers[i / channelsPerMixer].gain(i % channelsPerMixer, PER_CHANNEL_GAIN);
    }

    for (int i = 0; i < numSubmixers; i++) {
      new AudioConnection(submixers[i], 0, mixers[i / channelsPerMixer], i % channelsPerMixer);
      mixers[i / channelsPerMixer].gain(i % channelsPerMixer, 0.5);
    }

    for (int i = 0; i < numMixers; i++) {
      new AudioConnection(mixers[i], 0, masterMixer, i % channelsPerMixer);
      masterMixer.gain(i % channelsPerMixer, 0.25);
    }

    if (numMixers > 1) {
      new AudioConnection(masterMixer, 0, feedback, 0);
    } else if (numSubmixers > 1) {
      new AudioConnection(mixers[0], 0, feedback, 0);
    } else {
      new AudioConnection(submixers[0], 0, feedback, 0);
    }
    new AudioConnection(feedback, 0, delay, 0);
    new AudioConnection(delay, 0, feedback, 1);
    new AudioConnection(delay, 0, granular, 0);
    new AudioConnection(granular, 0, feedback, 2);
    feedback.gain(0, 1);
    granular.begin(granularMemory, GRANULAR_MEMORY_SIZE);
    granular.beginPitchShift(200);
    granular.setSpeed(0.5);
    new AudioConnection(feedback, 0, globalVolume, 0);
    new AudioConnection(globalVolume, 0, output, 0);
    new AudioConnection(globalVolume, 0, output, 1);

    populateParameters();
    // new AudioConnection(reverb, 0, output, 0);
  }

  void noteOn(int noteNumber, int velocity) {
    int voiceIndex = findOldestVoice();
    mostRecentVoice = voiceIndex;
    voiceNote[voiceIndex] = noteNumber;
    if (voiceIndex != -1) {
      voices[voiceIndex].noteOn(midiNoteToFrequency[noteNumber] * pitchBendMultiplier, velocity);
    }
  }

  void noteOff(int noteNumber) {
    for (int i = 0; i < numVoices; i++) {
      if (voiceNote[i] == noteNumber) {
        voices[i].noteOff();
        voiceNote[i] = -1;
        if (i == mostRecentVoice) {
          mostRecentVoice = -1;
        }
      }
    }
  }

  void noteAftertouch(int noteNumber, float aftertouchReading) {
    // Convert aftertouchReading from a range (e.g., 0.0 to 1.0) to a desired volume range.
    // This conversion depends on how aftertouch readings are scaled and the range you want.
    // Assuming aftertouchReading is already normalized between 0.0 and 1.0,
    // and you want to scale it to a volume range, for example, 0.5 (soft) to 1.0 (loud).
    float volume = aftertouchReading / 127;  // Adjust the formula as needed.
    // setDelayTime(aftertouchReading);
    // setDetune(aftertouchReading);
    // Iterate over the voices to find the voice(s) playing the given noteNumber.
    for (int i = 0; i < numVoices; i++) {
      if (voiceNote[i] == noteNumber) {
        // Assuming your Voice class has a setVolume method to adjust its volume.
        // voices[i].voiceFilter.frequency(frequency * (1 + (3 - (3 /aftertouchReading)) ));
        // voices[i].stringAmplitude.gain(volume);
        voices[i].wavetableMorph(volume);
        // // voices[i].voiceEnvelope.sustain(volume);
        // voices[i].filterEnvelope.sustain(volume);
      }
    }
  }

  void setOscBlend(float value) {
    float gain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceMixer.gain(STRING, 1 - gain);
      voices[i].voiceMixer.gain(WAVETABLE, gain * 0.4);
    }
  }

  void setMaxPitchBend(float semitones) {
    maxPitchBend = semitones;
  }

  void setVolume(float value) {
    globalVolume.gain(value / 127);
  }

  int findOldestVoice() {
    int oldestVoiceIndex = -1;
    unsigned long oldestTimestamp = ULONG_MAX;

    for (int i = 0; i < numVoices; ++i) {
      if (!voices[i].isActive()) {
        return i;  // Return the first inactive voice
      }

      unsigned long timestamp = voices[i].getLastUsedTimestamp();
      if (oldestVoiceIndex == -1 || timestamp < oldestTimestamp) {
        oldestVoiceIndex = i;
        oldestTimestamp = timestamp;
      }
    }

    return oldestVoiceIndex;  // Return the oldest active voice
  }

  const float midiNoteToFrequency[128] = {
    8.18, 8.66, 9.18, 9.72, 10.30, 10.91, 11.56, 12.25, 12.98, 13.75, 14.57, 15.43,
    16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87,
    32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 58.27, 61.74,
    65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47,
    130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.65, 220.00, 233.08,
    246.94, 261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00,
    466.16, 493.88, 523.25, 554.37, 587.33, 622.25, 659.26, 698.46, 739.99, 783.99, 830.61,
    880.00, 932.33, 987.77, 1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98,
    1567.98, 1661.22, 1760.00, 1864.66, 1975.53, 2093.00, 2217.46, 2349.32, 2489.02, 2637.02,
    2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07, 4186.01, 4434.92, 4698.64,
    4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13, 8372.02,
    8869.84, 9397.27, 9956.06, 10548.08, 11175.30, 11839.82, 12543.85
  };

  void createRandomPatch() {
    Serial.println("Creating a random patch");
    for (auto& param : parameters) {
      float randomValue = weightedRandom(0, 127, param.preferredValue, param.weighting);
      param.setterFunction(randomValue);
      param.currentValue = randomValue;
    }

    assignRandomParametersToMacros();
  }
  // Parameter Functions

  //Amp ADSR
  void setAmpAttack(float value) {
    int attack = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.attack(attack);
    }
  }
  void setAmpDecay(float value) {
    float decay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.decay(decay);
    }
  }
  void setAmpSustain(float value) {
    float sustain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.sustain(sustain);
    }
  }
  void setAmpRelease(float value) {
    float release = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.release(release);
    }
  }

  //FM ADSR
  void setFmAttack(float value) {
    int attack = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmEnvelope.attack(attack);
    }
  }
  void setFmDecay(float value) {
    float decay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmEnvelope.decay(decay);
    }
  }
  void setFmSustain(float value) {
    float sustain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmEnvelope.sustain(sustain);
    }
  }
  void setFmRelease(float value) {
    float release = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmEnvelope.release(release);
    }
  }

  //FM Controls
  void setStringFm(float value) {
    float gain = value * 0.001;
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmModulator.gain(STRING, gain);
    }
  }
  void setSineFm(float value) {
    float gain = value * 0.001;
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmModulator.gain(SINE, gain);
    }
  }
  void setWavetableFm(float value) {
    float gain = value * 0.001;
    for (int i = 0; i < numVoices; i++) {
      voices[i].fmModulator.gain(WAVETABLE, gain);
    }
  }
  void setOctaveControl(float value) {
    for (int i = 0; i < numVoices; i++) {
      voices[i].sine.frequencyModulation(map(value, 0, 127, 0, 8));
    }
  }

  //Filter ADSR
  void setFilterAttack(float value) {
    int attack = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterEnvelope.attack(attack);
    }
  }
  void setFilterDecay(float value) {
    float decay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterEnvelope.decay(decay);
    }
  }
  void setFilterSustain(float value) {
    float sustain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterEnvelope.sustain(sustain);
    }
  }
  void setFilterRelease(float value) {
    float release = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterEnvelope.release(release);
    }
  }

  //LFO DADSR
  void setLfoDelay(float value) {
    int delay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2Envelope.delay(delay);
    }
  }
  void setLfoAttack(float value) {
    int attack = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2Envelope.attack(attack);
    }
  }
  void setLfoDecay(float value) {
    float decay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2Envelope.decay(decay);
    }
  }
  void setLfoSustain(float value) {
    float sustain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2Envelope.sustain(sustain);
    }
  }
  void setLfoRelease(float value) {
    float release = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2Envelope.release(release);
    }
  }

  //Filter Controls
  void setFilterFrequency(float value) {

    float minFreq = 20;     // minimum frequency
    float maxFreq = 20000;  // maximum frequency

    frequency = minFreq * pow((maxFreq / minFreq), (value / 127.0));

    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceFilter.frequency(frequency);
    }
  }
  void setFilterResonance(float value) {
    float gainReductionFactor = 1;
    float resonance = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceFilter.resonance(resonance);
      if (resonance > 0.7) {
        voices[i].filterAttenuation.gain(1.0 - (resonance - 0.7) * gainReductionFactor);
      } else {
        voices[i].filterAttenuation.gain(1);
      }
    }
  }
  void setFilterEnvelope(float value) {
    float amount = (value / 64) - 1;
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterAmount.amplitude(amount);
    }
  }
  void setFilterModBlend(float value) {
    float blend = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].filterModBlend.gain(0, blend);
      voices[i].filterModBlend.gain(1, 1 - blend);
    }
  }

  //Modulation
  void setLfoAmount(float value) {
    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2.amplitude(value / 127);
    }
  }
  void setLfoRate(float value) {
    // Convert MIDI value (0-127) to a useful LFO rate using exponential scaling
    float minLfoHz = 0.1f;                              // Minimum LFO frequency in Hz
    float maxLfoHz = 20.0f;                             // Maximum LFO frequency in Hz
    float exponent = (value / 127.0f) * 3.0f;           // Exponentially scale value over 3 octaves
    float lfoRateHz = minLfoHz * powf(2.0f, exponent);  // Calculate LFO frequency

    for (int i = 0; i < numVoices; i++) {
      voices[i].lfo2.frequency(lfoRateHz);
    }
  }
  void setVibrato(float value) {

    float rate = (value / 35) + 2;  // Map to a reasonable rate range
    float depth = value / 70;       // Map to a reasonable depth range

    // Set the vibrato parameters


    for (int i = 0; i < numVoices; i++) {
      if (value == 0) {
        voices[i].vibratoOff();
      } else {
        voices[i].lfo.frequency(rate);   // Set the vibrato rate
        voices[i].lfo.amplitude(depth);  // Set the vibrato depth
        voices[i].updateVibrato();
      }
    }
  }

  //Oscillators
  void setStartWavetable(float value) {
    int selector = value;
    for (int i = 0; i < numVoices; i++) {
      voices[i].oscillatorOne.arbitraryWaveform(waveform[selector], 800);
      voices[i].oscillatorThree.arbitraryWaveform(waveform[selector], 800);
    }
  }
  void setEndWavetable(float value) {
    int selector = value;
    for (int i = 0; i < numVoices; i++) {
      voices[i].oscillatorTwo.arbitraryWaveform(waveform[selector], 800);
      voices[i].oscillatorFour.arbitraryWaveform(waveform[selector], 800);
    }
  }
  void setDetuneAmount(float detune) {
    for (int i = 0; i < numVoices; i++) {
      voices[i].pitchParams.detuneAmount = detune;
      // voices[i].applyFrequencyToOscillators();
    }
  }
  void blendThreeSourcesNormalized(int value) {
    // Map the input value to distinct ranges for each oscillator blend
    float normalizedValue = value / 127.0f;
    float gainString = 0.0f, gainWavetable = 0.0f, gainSine = 0.0f;

    // Phase 1: Pure String to Blend of String and Wavetable
    if (value <= 31) {
      gainString = 1.0f;
      gainWavetable = 0.0f;
      gainSine = 0.0f;
    } else if (value <= 60) {
      // Transition from String to Wavetable
      gainString = 1.0f - ((value - 31) / 29.0f);
      gainWavetable = (value - 31) / 29.0f;
      gainSine = 0.0f;
    } else if (value <= 90) {
      // Pure Wavetable
      gainString = 0.0f;
      gainWavetable = 1.0f;
      gainSine = 0.0f;
    } else if (value <= 127) {
      // Transition from Wavetable to Sine
      gainString = 0.0f;
      gainWavetable = 1.0f - ((value - 90) / 37.0f);
      gainSine = (value - 90) / 37.0f;
    }

    // Apply the calculated gains
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceMixer.gain(STRING, gainString);
      voices[i].voiceMixer.gain(WAVETABLE, gainWavetable);
      voices[i].voiceMixer.gain(SINE, gainSine);
    }
    //   // Use normalizedValue directly to distribute gain across the three sources
    //   float normalizedValue = value / 127.0;
    //   float gain1, gain2, gain3;

    //   // Dynamically adjust gains based on normalizedValue
    //   gain1 = (1 - normalizedValue) * 2 / 3;  // Decreases from 2/3 to 0 as normalizedValue goes from 0 to 1
    //   gain2 = normalizedValue / 2;            // Increases from 0 to 1/2 as normalizedValue goes from 0 to 1
    //   gain3 = 1 - (gain1 + gain2);            // Ensures total gain always equals 1

    //   // Apply the calculated gains

    //   for (int i = 0; i < numVoices; i++) {
    //     voices[i].voiceMixer.gain(SINE, gain1);
    //     voices[i].voiceMixer.gain(STRING, gain2);
    //     voices[i].voiceMixer.gain(WAVETABLE, gain3);
    //   }
  }

  //Pitch
  void pitchBend(float value) {

    bendAmount = (maxPitchBend / 8192) * value;
    pitchBendMultiplier = pow(2.0, bendAmount / 12);

    Serial.println(pitchBendMultiplier);
    // Apply pitch bend to most recent voice
    if (mostRecentVoice > -1) {
      voices[mostRecentVoice].applyPitchBend(pitchBendMultiplier);
    }

    // // Apply pitch bend to each voice
    // for (int i = 0; i < numVoices; i++) {
    //   if (voices[i].isSustain) {
    //     // Update the frequency for relevant components in the voice
    //     voices[mostRecentVoice].applyPitchBend(bentFrequency);
    //   }
    // }
  }
  void setDetune(float value) {
    // Assuming value is in the range [0, 127]
    float maxDetune = 0.005;  // Example maximum detune factor, adjust based on musical taste

    // Calculate detune factor, ensuring it scales linearly with input value
    // This maps value = 0 to no detune (factor of 1.0) and value = 127 to maximum detune
    detuneFactor = 1.0 + (value / 127.0) * maxDetune;

    // Apply the new detune factor to all voices
    for (int i = 0; i < numVoices; i++) {
      voices[i].applyDetune(detuneFactor);
    }
  }
  void setOctaveOffset(int offset) {
    for (int i = 0; i < numVoices; i++) {
      voices[i].pitchParams.octaveOffset = offset;
      // voices[i].applyFrequencyToOscillators();
    }
  }

  //Effects
  void setDelayTime(float value) {
    float minDelayMs = 1.0;                                 // Minimum delay in milliseconds
    float maxDelayMs = 200.0;                               // Maximum delay in milliseconds
    float exponent = (value / 127.0f) * 3.0f;               // Adjust exponent scaling as needed
    float delayTimeMs = minDelayMs * powf(2.0f, exponent);  // Calculate delay time

    // Ensure delayTimeMs does not exceed maxDelayMs
    delayTimeMs = min(delayTimeMs, maxDelayMs);

    // Set the delay time here
    delay.delay(0, delayTimeMs);
  }

  void setDelayFeedback(float value) {
    feedback.gain(1, value * 0.006299212598);
  }
  void setGranularFeedback(float value) {
    feedback.gain(2, value * 0.006299212598);
  }


  void populateParameters() {
    // Helper lambda to avoid repeating the class reference
    auto bindFn = [this](auto fn) {
      return [this, fn](float value) {
        (this->*fn)(value);
      };
    };

    // Amp ADSR
    parameters.emplace_back("Amp Attack", bindFn(&Synth::setAmpAttack), 10, 0.7, false);
    parameters.emplace_back("Amp Decay", bindFn(&Synth::setAmpDecay), 40, 0.5, false);
    parameters.emplace_back("Amp Sustain", bindFn(&Synth::setAmpSustain), 100, 0.5, false);
    parameters.emplace_back("Amp Release", bindFn(&Synth::setAmpRelease), 30, 0.5, false);

    // FM ADSR
    parameters.emplace_back("FM Attack", bindFn(&Synth::setFmAttack), 10, 0.5, true);
    parameters.emplace_back("FM Decay", bindFn(&Synth::setFmDecay), 20, 0.5, true);
    parameters.emplace_back("FM Sustain", bindFn(&Synth::setFmSustain), 0, 0.5, false);
    parameters.emplace_back("FM Release", bindFn(&Synth::setFmRelease), 50, 0.5, true);

    // FM Controls
    parameters.emplace_back("String FM", bindFn(&Synth::setStringFm), 0, 0.6, true);
    parameters.emplace_back("Sine FM", bindFn(&Synth::setSineFm), 0, 0.6, true);
    parameters.emplace_back("Wavetable FM", bindFn(&Synth::setWavetableFm), 0, 0.6, true);
    parameters.emplace_back("Octave Control", bindFn(&Synth::setOctaveControl), 0, 1, true);

    // Filter ADSR
    parameters.emplace_back("Filter Attack", bindFn(&Synth::setFilterAttack), 25, 0.6, true);
    parameters.emplace_back("Filter Decay", bindFn(&Synth::setFilterDecay), 25, 0.6, true);
    parameters.emplace_back("Filter Sustain", bindFn(&Synth::setFilterSustain), 80, 0.7, false);
    parameters.emplace_back("Filter Release", bindFn(&Synth::setFilterRelease), 25, 0.6, true);

    // LFO DADSR
    parameters.emplace_back("LFO Delay", bindFn(&Synth::setLfoDelay), 0, 0.6, true);
    parameters.emplace_back("LFO Attack", bindFn(&Synth::setLfoAttack), 20, 0.6, true);
    parameters.emplace_back("LFO Decay", bindFn(&Synth::setLfoDecay), 20, 0.6, true);
    parameters.emplace_back("LFO Sustain", bindFn(&Synth::setLfoSustain), 50, 0.7, true);
    parameters.emplace_back("LFO Release", bindFn(&Synth::setLfoRelease), 50, 0.6, true);

    // Filter Controls
    parameters.emplace_back("Filter Frequency", bindFn(&Synth::setFilterFrequency), 80, 0.7, true);
    parameters.emplace_back("Filter Resonance", bindFn(&Synth::setFilterResonance), 50, 0.8, true);
    parameters.emplace_back("Filter Envelope", bindFn(&Synth::setFilterEnvelope), 64, 0.7, true);
    parameters.emplace_back("Filter Mod Blend", bindFn(&Synth::setFilterModBlend), 64, 0.3, true);

    // Modulation
    parameters.emplace_back("LFO Amount", bindFn(&Synth::setLfoAmount), 60, 0.6, true);
    parameters.emplace_back("LFO Rate", bindFn(&Synth::setLfoRate), 60, 0.6, true);
    parameters.emplace_back("Vibrato", bindFn(&Synth::setVibrato), 5, 0.6, true);

    // Oscillators
    parameters.emplace_back("Start Wavetable", bindFn(&Synth::setStartWavetable), 64, 0.3, false);
    parameters.emplace_back("End Wavetable", bindFn(&Synth::setEndWavetable), 64, 0.3, false);
    parameters.emplace_back("Detune Amount", bindFn(&Synth::setDetuneAmount), 10, 0.6, true);
    parameters.emplace_back("Blend Three Sources", bindFn(&Synth::blendThreeSourcesNormalized), 64, 0.5, true);

    // Pitch
    // parameters.emplace_back("Octave Offset", bindFn(&Synth::setOctaveOffset), 0, 1.0, true);
    parameters.emplace_back("Detune", bindFn(&Synth::setDetune), 10, 0.6, true);

    // Effects
    parameters.emplace_back("Delay Time", bindFn(&Synth::setDelayTime), 60, 0.5, true);
    parameters.emplace_back("Delay Feedback", bindFn(&Synth::setDelayFeedback), 0, 0.6, true);
    parameters.emplace_back("Granular Feedback", bindFn(&Synth::setGranularFeedback), 0, 0.6, true);
    // More parameters can be added here following the same pattern
  }


  int weightedRandom(int minValue, int maxValue, int favoriteValue, float weight) {
    // weight: 0.0 (even distribution) to 1.0 (favoriteValue almost always picked)
    weight = constrain(weight, 0.0, 1.0);
    int range = maxValue - minValue + 1;

    // Generate two random numbers
    int randValue = random(minValue, maxValue + 1);
    int randBias = random(minValue, maxValue + 1);

    // Apply weight: the closer the randBias is to favoriteValue, the higher the chance randValue is used
    if (abs(favoriteValue - randBias) < (weight * range)) {
      return randValue;
    } else {
      return favoriteValue;
    }
  }

  //functions used by the sketch but maybe not needed in the library.

  void setAttack(float value) {
    int attack = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.attack(attack);
      voices[i].filterEnvelope.attack(attack);
    }
  }

  void setRelease(float value) {
    float release = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.release(release);
      voices[i].filterEnvelope.release(release);
    }
  }

  void setDecay(float value) {
    float decay = pow(value, 1.7);
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.decay(decay);
      voices[i].filterEnvelope.decay(decay);
    }
  }

  void setSustain(float value) {
    float sustain = value / 127;
    for (int i = 0; i < numVoices; i++) {
      voices[i].voiceEnvelope.sustain(sustain);
      voices[i].filterEnvelope.sustain(sustain);
    }
  }

  void assignMacroControls() {

    macroOne.addControl([this](float value) {
      this->setDetune(value);
    },
                        0, 127);  // Hypothetical range
    macroOne.addControl([this](float value) {
      this->setSineFm(value);
    },
                        0, 127);  // Another example
    macroOne.addControl([this](float value) {
      this->setFilterFrequency(value);
    },
                        60, 127);  // Hypothetical range

    macroTwo.addControl([this](float value) {
      this->setWavetableFm(value);
    },
                        127, 0);  // Another example
    macroTwo.addControl([this](float value) {
      this->blendThreeSourcesNormalized(value);
    },
                        0, 127);  // Another example
    macroTwo.addControl([this](float value) {
      this->blendThreeSourcesNormalized(value);
    },
                        0, 127);  // Another example
  }

  void assignRandomParametersToMacros() {
    const size_t numberOfParametersPerMacro = 2;  // Example: Assign 3 random parameters to each macro

    // Create a list of indices for modulatable parameters
    std::vector<int> modulatableIndices;
    for (int i = 0; i < parameters.size(); ++i) {
      if (parameters[i].modulatable) {
        modulatableIndices.push_back(i);
      }
    }

    // Shuffle and assign parameters to macroOne using Arduino's random function
    for (size_t i = 0; i < numberOfParametersPerMacro; ++i) {
      if (!modulatableIndices.empty()) {
        // Generate a random index within the range of modulatable parameters
        int randomIndex = random(0, modulatableIndices.size());

        // Assign the randomly selected modulatable parameter to macroOne
        int paramIndex = modulatableIndices[randomIndex];
        macroOne.addControl(parameters[paramIndex].setterFunction, parameters[paramIndex].currentValue, 127);  // Adjust range as needed

        // Remove the selected index to avoid repetition
        modulatableIndices.erase(modulatableIndices.begin() + randomIndex);
      }
    }

    // Refill the list of modulatable indices for macroTwo
    modulatableIndices.clear();
    for (int i = 0; i < parameters.size(); ++i) {
      if (parameters[i].modulatable) {
        modulatableIndices.push_back(i);
      }
    }

    // Shuffle and assign parameters to macroTwo using Arduino's random function
    for (size_t i = 0; i < numberOfParametersPerMacro; ++i) {
      if (!modulatableIndices.empty()) {
        // Generate a random index within the range of modulatable parameters
        int randomIndex = random(0, modulatableIndices.size());

        // Assign the randomly selected modulatable parameter to macroTwo
        int paramIndex = modulatableIndices[randomIndex];
        macroTwo.addControl(parameters[paramIndex].setterFunction, 0, 127);  // Adjust range as needed

        // Remove the selected index to avoid repetition
        modulatableIndices.erase(modulatableIndices.begin() + randomIndex);
      }
    }
  }


  void macroOneControl(float value) {
    macroOne.apply(value);
  }

  void macroTwoControl(float value) {
    macroTwo.apply(value);
  }
  // void openFilter(float value) {
  // }
};

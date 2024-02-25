#pragma once
#ifndef synth_karplusstronger_h_
#define synth_karplusstronger_h_

#include "Arduino.h"
#include "AudioStream.h"
#include "utility/dspinst.h"

class AudioSynthKarplusStronger : public AudioStream {
public:
  AudioSynthKarplusStronger() : AudioStream(1, inputQueueArray) {  // Adjusted for 2 inputs
    state = 0;
  }

  virtual void update(void);

  void noteOn(float frequency, float velocity) {
    if (velocity > 1.0f) {
      velocity = 0.0f;
    } else if (velocity <= 0.0f) {
      noteOff(1.0f);
      return;
    }

    baseFrequency = frequency;
    magnitude = velocity * 65535.0f;
    float len = AUDIO_SAMPLE_RATE_EXACT / frequency;
    bufferLen = len;
    bufferIndex = 0;
    state = 1;
  }

  void noteOff(float velocity) {
    state = 0;
  }

  void setPitch(float frequency) {
    if (state == 2 && frequency > 0) {
      // Only update pitch if the string is currently playing
      float len = AUDIO_SAMPLE_RATE_EXACT / frequency;
      bufferLen = len;
    }
  }

private:
  uint8_t state;  // 0=steady output, 1=begin on next update, 2=playing
  uint16_t bufferLen;
  uint16_t bufferIndex;
  int32_t magnitude;     // current output
  static uint32_t seed;  // must start at 1
  float buffer[536];     // TODO: dynamically use audio memory blocks
  audio_block_t *inputQueueArray[1];
  float decay = 0.999999999;
  const uint16_t MIN_BUFFER_LEN = 44100 / 27.5;
  float baseFrequency = 0;
};

#endif

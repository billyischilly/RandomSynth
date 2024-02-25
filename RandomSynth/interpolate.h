#pragma once
#include <Audio.h>

class AudioInterpolate : public AudioStream {
public:
  AudioInterpolate()
    : AudioStream(2, inputQueueArray) {}

  // Method to set the interpolation factor (0.0 to 1.0)
  void setInterpolationFactor(float factor) {
    if (factor < 0.0) factor = 0.0;
    if (factor > 1.0) factor = 1.0;
    interpolationFactor = factor;
  }

  virtual void update() {
    audio_block_t *block1, *block2;

    block1 = receiveReadOnly(0);  // Receive data from the first oscillator
    block2 = receiveReadOnly(1);  // Receive data from the second oscillator

    if (block1 && block2) {
      audio_block_t *output = allocate();
      if (output) {
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
          int32_t interpolatedValue = (int32_t)(block1->data[i] * (1.0 - interpolationFactor)) + (int32_t)(block2->data[i] * interpolationFactor);

          // Basic clipping protection
          if (interpolatedValue > 32767) interpolatedValue = 32767;
          else if (interpolatedValue < -32768) interpolatedValue = -32768;

          output->data[i] = (int16_t)interpolatedValue;
        }
        transmit(output);  // Send the mixed signal to the output
        release(output);
      }
      release(block1);
      release(block2);
    } else {
      if (block1) release(block1);
      if (block2) release(block2);
    }
  }

private:
  audio_block_t *inputQueueArray[2];
  float interpolationFactor = 0.5;  // Default to an even mix
};

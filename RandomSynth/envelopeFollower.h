#pragma once
#include <AudioStream.h>
#include <arm_math.h>  // For DSP functions, if needed

class AudioEffectEnvelopeFollower : public AudioStream {
public:
  AudioEffectEnvelopeFollower()
    : AudioStream(1, inputQueueArray) {}

  virtual void update() {
    audio_block_t *inBlock, *outBlock;

    // Get the input and output blocks
    inBlock = receiveReadOnly(0);
    if (!inBlock) return;
    outBlock = allocate();
    if (!outBlock) {
      release(inBlock);
      return;
    }

    // Process each sample
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      // Rectify the input signal
      float rectified = fabs(inBlock->data[i]);
      rectified = rectified - 200;
      if (rectified < 0) rectified = 0;
      rectified = rectified * 5;
      Serial.println(rectified);
      // Apply a simple low-pass filter for smoothing
      // This is a very basic example; you might need a more sophisticated filter
      env = (alpha * rectified) + ((1 - alpha) * env);
      // Output the smoothed signal
      outBlock->data[i] = env;
    }

    // Transmit the block and release memory
    transmit(outBlock);
    release(inBlock);
    release(outBlock);
  }

private:
  audio_block_t *inputQueueArray[1];
  float env = 0;         // Envelope value
  float alpha = 0.0001;  // Smoothing factor, adjust as needed
};

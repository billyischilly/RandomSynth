#pragma once
#include <Arduino.h>
#include <AudioStream.h>

class AudioInputToInt : public AudioStream {
public:
  AudioInputToInt()
    : AudioStream(1, inputQueueArray) {
    // any extra initialization
  }



  // Create a public function to get the int value
  int getIntValue() {
    return intValue;
  }
  virtual void update(void) {
    audio_block_t *block;
    block = receiveReadOnly(0);  // Receive audio data from channel 0

    if (block != nullptr) {
      // Process the audio data and convert it to an int value
      // For example, you can calculate the average value of the audio samples
      int sum = 0;
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        sum += block->data[i];
      }
      intValue = sum / AUDIO_BLOCK_SAMPLES;

      // Release the audio block
      release(block);
    }
  }

private:
  audio_block_t *inputQueueArray[1];
  int intValue;  // Store the int value here
};

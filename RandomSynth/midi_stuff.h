#pragma once
#define MODULATION_WHEEL 1
#define BREATH_CONTROLLER 2
#define FOOT_CONTROLLER 4
#define PORTAMENTO_TIME 5
#define DATA_ENTRY_MSB 6
#define VOLUME 7
#define BALANCE 8
#define PAN 10
#define EXPRESSION 11
#define EFFECT_CONTROL_1 12
#define EFFECT_CONTROL_2 13
#define GENERAL_PURPOSE_CONTROLLER_1 16
#define GENERAL_PURPOSE_CONTROLLER_2 17
#define GENERAL_PURPOSE_CONTROLLER_3 18
#define GENERAL_PURPOSE_CONTROLLER_4 19
#define SUSTAIN_PEDAL 64
#define PORTAMENTO 65
#define SOSTENUTO 66
#define SOFT_PEDAL 67
#define LEGATO_FOOTSWITCH 68
#define HOLD_2 69
#define SOUND_CONTROLLER_1 70   // usually controls timber
#define SOUND_CONTROLLER_2 71   // usually controls resonance
#define SOUND_CONTROLLER_3 72   // usually controls release time
#define SOUND_CONTROLLER_4 73   // usually controls attack time
#define SOUND_CONTROLLER_5 74   // usually controls brightness
#define SOUND_CONTROLLER_6 75
#define SOUND_CONTROLLER_7 76
#define SOUND_CONTROLLER_8 77
#define SOUND_CONTROLLER_9 78
#define SOUND_CONTROLLER_10 79
#define GENERAL_PURPOSE_CONTROLLER_5 80
#define GENERAL_PURPOSE_CONTROLLER_6 81
#define GENERAL_PURPOSE_CONTROLLER_7 82
#define GENERAL_PURPOSE_CONTROLLER_8 83
#define PORTAMENTO_CONTROL 84
#define EFFECTS_DEPTH 91
#define TREMOLO_DEPTH 92
#define CHORUS_DEPTH 93
#define CELESTE_DEPTH 94
#define PHASER_DEPTH 95
#define DATA_INCREMENT 96
#define DATA_DECREMENT 97
#define NON_REGISTERED_PARAMETER_NUMBER_LSB 98
#define NON_REGISTERED_PARAMETER_NUMBER_MSB 99
#define REGISTERED_PARAMETER_NUMBER_LSB 100
#define REGISTERED_PARAMETER_NUMBER_MSB 101
// ... and others based on your specific needs

class MPEChannel {
private:
  static const int MAX_MPE_CHANNELS = 15;  // Assuming channel 1 is reserved
  int mpeChannels[MAX_MPE_CHANNELS];
  bool channelInUse[MAX_MPE_CHANNELS];
  unsigned long noteTimestamps[MAX_MPE_CHANNELS];  // To keep track of when each note was last assigned
  unsigned long currentTimestamp;

public:
  MPEChannel()
    : currentTimestamp(0) {
    for (int i = 0; i < MAX_MPE_CHANNELS; i++) {
      mpeChannels[i] = -1;  // Initialize with -1 indicating no note is assigned
      channelInUse[i] = false;
      noteTimestamps[i] = 0;
    }
  }

  int assignChannel(int noteNumber) {
    currentTimestamp++;  // Increment timestamp for each assignment
    int oldestChannel = 0;
    unsigned long oldestTime = currentTimestamp;

    // Try to find an available channel
    for (int i = 0; i < MAX_MPE_CHANNELS; i++) {
      if (!channelInUse[i]) {
        mpeChannels[i] = noteNumber;
        channelInUse[i] = true;
        noteTimestamps[i] = currentTimestamp;
        return i + 2;  // +2 because channel 1 is reserved
      }

      // Track the oldest used channel for potential voice stealing
      if (noteTimestamps[i] < oldestTime) {
        oldestChannel = i;
        oldestTime = noteTimestamps[i];
      }
    }

    // Voice stealing: use the oldest channel if all channels are in use
    mpeChannels[oldestChannel] = noteNumber;
    noteTimestamps[oldestChannel] = currentTimestamp;
    return oldestChannel + 2;
  }

  int releaseChannel(int noteNumber) {
    for (int i = 0; i < MAX_MPE_CHANNELS; i++) {
      if (mpeChannels[i] == noteNumber) {
        channelInUse[i] = false;
        mpeChannels[i] = -1;
        return i + 2;  // Return the channel that was being used
      }
    }
    return -1;  // Return -1 if the note was not found
  }

  int getChannelAssignment(int noteNumber) {
    for (int i = 0; i < MAX_MPE_CHANNELS; i++) {
      if (mpeChannels[i] == noteNumber) {
        return i + 2;  // +2 because channel 1 is reserved
      }
    }
    return -1;  // Return -1 if the note is not assigned to any channel
  }
};

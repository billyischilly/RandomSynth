#include <synth.h>
#include <Wire.h>
#include <MIDI.h>


Synth<8> synth;
AudioControlSGTL5000 sgtl5000;


void setup() {
  Serial.begin(115200);
  AudioMemory(300);    // Allocate memory for audio processing
  sgtl5000.enable();   // Enable the audio shield
  sgtl5000.volume(1);  // Set volume
  synth.begin();
  delay(200);
  synth.createRandomPatch();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {
    // Read the incoming byte:
    char receivedChar = Serial.read();
    // Check if the received character is the trigger for randomization
    if (receivedChar == 'r') {    // Assuming 'r' is the trigger for creating a random patch
      synth.createRandomPatch();  // Call the function that creates a new random patch
    }
  }

  synth.noteOn(60, 100);
  delay(300);
  synth.noteOff(60);
  delay(300);
}

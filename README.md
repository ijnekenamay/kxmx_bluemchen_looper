# kxmx_bluemchen_LOOPER
EuroRack modular synthesizer equipped with daisy seed. Dual looper operating on kxmx bluemchen.

I referenced the looper code for DaisyPatch. https://github.com/misterinterrupt/PatchLooperExample

If you wish to use it, simply flash “looper.bin” via the Daisy Web programmer.

### Operation Method:
In IDOL state, pressing the encoder button once enters WAIT state. This state waits for the next clock signal. Upon detecting the clock, it immediately enters REC mode and begins recording the input audio.
Detecting the next clock signal stops recording, and the display shows LOOP state. This is the state where the automatically recorded sound is looping playback.
To overwrite the recorded audio, press the push button again to enter WAIT state. This will restart the process from the beginning and record new audio.
The previously recorded audio is completely discarded.
Rotary encoder: Only the switch function works.

### POT1, POT2:
This is a bonus feature. It slices the recorded audio into 2 to 32 segments and plays them randomly. Let's call it the Auto-Glitch function. If the POT value is 0, nothing happens.

### CVIN: 
Inputs a clock signal corresponding to the desired length of the recording in bars. (e.g., for 120 BPM, input 30 BPM)

### AUDIOIN: 
Inputs the sound from instruments like a synth.

### AUDIOOUT: 
In IDOL mode, you hear the input audio through the loop.
Once recording completes and the device enters LOOP mode, the dry signal is completely muted, and only the recorded sound plays.


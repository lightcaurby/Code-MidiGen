# Code-MidiGen

MidiGen is a small commandline tool to generate MIDI files for automating the accompaniment on the Yamaha arrangement keyboards. This project uses the MidiData library from [OpenMIDIProject](https://openmidiproject.osdn.jp/index_en.html).

The input files have three main parts written in textual format:
- global settings, such as tempo and transposing 
- accompaniment control, such as triggering chord changes, different variations of the accompaniment style, etc.
- lead melody to be used as a reference for recording vocals, for example

The output consists of four MIDI files:
- <title>_accmp.mid for controlling the accompaniment
- <title>_melody.mid for the reference lead melody
- <title>_vh.mid for controlling the vocal harmony
- <title>_chords.mid for controlling the vocal harmony  

Any DAW can be used to send the output MIDI files to the keyboard as part of a recording project. Typically, the keyboard operates in the slave mode while the DAW provides the MIDI master clock. 

Example run:

```
C:\git\Code-MidiGen> x64\release\MidiGen.exe -i "example\sydamessani on taivas.song.txt" -o example
```

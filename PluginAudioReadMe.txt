***** Plugins ****

Each folder contains the sources files of the plugin.

The files containning the actual design of the plugin are 

 <nameOfTheFolder>.h
 <nameOfTheFolder>.cpp

Concerning the cpp file, the implementation of plugin is determined by the methods 

- Constructor() : initialize mains variables
- prepareForPlay() : called just before the sound is played
- processAudioFrame() : where we process the buffer of sound
- userInterfaceChange() : manage button/slider changes


A .dll file is included to each folder to be used in audio Plugin clients.
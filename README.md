# WHISP (PLAYGROUND)



Playground to experiment with wav file demuxing, samplerate changing (through the r8b c++ library) and feeding audio file to whispercpp
(This is still a very early stage)



external libraries used:

1. https://github.com/avaneev/r8brain-free-src
2. https://github.com/Tracktion/choc (not really used but deserves mentioning)
3. https://github.com/ggerganov/whisper.cpp





WARNING:

You'll need a local copy of the whisper.cpp repo in order to have access to its headers, you will also have to include the relative path in your Xcode project. Everything else should be already included/linked.


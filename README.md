Hey, WiFi
=========

![](https://voice-engine.github.io/hey-wifi/img/scenario.svg)

Send WiFi settings through sound wave.  
The project is based on [quiet](https://github.com/quiet).

This is a fork of original [Hey, Wifi](https://github.com/voice-engine/hey-wifi) made in Python.

### Requirements
+ [quiet](https://github.com/quiet/quiet)
+ [alsa-lib](https://github.com/alsa-project/alsa-lib)

### Usage

```
  -v         Activate verbose (debug) messages
  -D default Specify ALSA PCM capture device
  -r 44100   Sample rate to use
  -c 1       Audio channels from capture device
  -B 16384   Buffer size to use
  -f q.json  quiet-profiles.json file
  -p wave    Quiet profile to use
  -x run.sh  Custom command or script to run
             (run.sh SSID PASSWORD)
```

Run the program with your required parameters, then use any device (like your mobile phone),
go to https://hey.voicen.io , enter your wireless AP details and keep your device close to
the microphone.  
Note that it may take few tries to get the config.

Program will try to use `nmcli` to configure the wireless access.
In case this is not available, create a script or use a program 
that allow you to configure the requested access.  
Program will be executed with arguments `program-name SSID PASSWORD`.

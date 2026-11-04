# OxC3 audio (id: 0x1C35)

OxC3 audio plays sound: it lists the machine's output devices, opens one, and streams a WAV file through it to the
speakers, with gain, 3D positioning and a listener. The backend is OpenAL Soft; the rest of the module is written
against its own types and does not name OpenAL.

This page is a map, not a reference: the API is the headers under `include/audio/` (the OpenAL specific types are in
`include/audio/openal_soft/`). Reading the file, WAV included, belongs to [formats](formats.md), and the allocator,
reference counted pointers and streams all come from [types](types.md).

## The objects

There are four, each a reference counted pointer made from a `RefPtrType` that the caller builds with the matching
`*_makeType` function (`AudioInterface_makeType` and so on) and passes to the create call. A create call refuses a type
that was not made for it.

- **`AudioInterface`**: the API itself (`AudioInterface_create`). It enumerates devices
  (`AudioInterface_getDeviceInfos`) and picks the preferred one with the capabilities a caller requires
  (`AudioInterface_getPreferredDevice`). A machine with no sound server or no device lists zero devices rather than
  failing.
- **`AudioDevice`**: one opened output (`AudioDeviceRef_create`), with the listener's position, orientation, velocity
  and gain. Its `AudioDeviceInfo` says which sample formats the hardware takes natively (`EAudioDeviceFlags_HasF32Ext`
  and its siblings) and whether it is the main output. A debug device is refused if the device lacks the OpenAL debug
  extension, rather than silently made without it.
- **`AudioStream`**: a sound read from a stream while it plays (`AudioDeviceRef_createStream`, or
  `AudioDeviceRef_createFromFile` to detect the file type from its first bytes). It can loop a number of times or
  forever, seek (`AudioStreamRef_seekTime`), play and stop. Only WAV is recognized; any other file is refused.
- **`AudioSource`**: where a stream is heard (`AudioDeviceRef_createSource`, with 2D and 3D variants). It holds the
  gain and, for spatial audio, the position and velocity. A source that is not spatial plays at the listener, wherever
  the listener is. Several sources can share one stream, but they play the same data at the same offset through one
  OpenAL voice: the gain, position and velocity set last apply to all of them, and releasing one mutes the stream.
  Sounds that need their own starting point or settings need their own streams.

A stream and a source each keep a reference to the device, and a source keeps one to its stream. A device keeps a
reference to its interface, so the interface lives until everything made from it is released. The device also tracks
its streams and pending sources, but only weakly.

## How a WAV file gets played

`AudioDeviceRef_createFromFile` reads the file's magic from the input stream, then calls `WAV_read`, which walks the
RIFF chunks through the stream and returns the format and where the sample data starts. The audio stream keeps its own
reference to the input stream, so the caller releases its reference whether the call succeeds or not. The file is never
loaded whole: playback keeps the stream's data in four OpenAL buffers of `streamLength` bytes (one second by default,
at least 64 KiB if set) and refills them as they drain. A seek takes effect at the next update, which drops the queued
buffers. A stream that played to its end stays there until a seek, and a seek while it isn't playing also resets its
loop count, so the next play makes every pass again.

Refilling happens in `AudioDeviceRef_update`, which the application calls regularly, not on a thread of the module's
own. `AudioDeviceRef_wait` calls it in a loop until every stream has finished, and with `waitForLoopingStream` set it
also waits on endless loops, which only end when someone stops them. The update, wait, play, stop and source creation
calls take the device's spin lock for up to a second and fail with an error rather than waiting forever.
The listener and source setters only record what changed; the next update applies it. Any change reaches the backend,
however small, and only a value exactly equal to the stored one is skipped.

WAV files of 8, 16, 24 and 32 bit PCM and 32 and 64 bit float, mono or stereo, are accepted. A sample format the
device cannot take natively is converted to 16 bit on the CPU while streaming, and a debug build logs that it did:
32 and 64 bit float unless `EAudioDeviceFlags` reports them, and 24 and 32 bit PCM always, because the OpenAL backend
reports neither. `flattenSound` on a stream folds stereo to mono.

## What is refused

- **Stereo with spatial audio.** OpenAL positions mono sources only, so a spatial source on a stereo stream is an
  error unless the stream was made with `flattenSound`. The CLI's `-split` option exists to prepare such files.
- **A pitch on a source for a stream.** A stream owns its pitch, set when it is created, so a pitch given to
  `AudioDeviceRef_createSource` for one is an error, as is a pitch update on a source that differs from the stream's,
  and a negative pitch is refused at stream creation.
- **Values the backend would refuse.** A negative or non finite gain and a non finite position, velocity or listener
  orientation are errors at the setters and at source creation, and nothing is stored.
- **A sound from another device.** A source is made on the device its stream was created on.
- **Inconsistent stream info.** A sample rate that does not match the bytes per second, a data range past the end of
  the stream, an unknown format, a `streamLength` under 64 KiB, a data length of 0, or a `loops` other than 1 without
  `EAudioStreamInfoFlags_IsLoop`
  fails `AudioDeviceRef_createStream`, which takes ownership of the info it is given.
- **A bad type or a missing object.** Every call checks the type id of the reference it is given.

## The CLI

`OxC3 audio devices` lists what `AudioInterface_getDeviceInfos` finds, and `OxC3 audio convert` rewrites WAV files
(bit depth and channel split) with `WAVFile_convert`. Both are documented in the
[Audio section of OxC3_tool.md](OxC3_tool.md#audio); the CLI does not play sound.

## Tests

- [test_audio_interface_main.c](../src/audio/test/interface/test_audio_interface_main.c) is `OxC3_audio_interface_test`:
  interface and device lifetime, device enumeration, the preferred device, the listener and source setters, the values
  they refuse, a source released before an update, a sound from another device, stream creation and seeking at the
  edges, a stereo spatial source being rejected, and a WAV opened at an offset inside a larger stream. It is automatic.
  With no audio device it checks what it can and skips the device dependent modules.
- [test_audio_functional_main.c](../src/audio/test/functional/test_audio_functional_main.c) is
  `OxC3_audio_functional_test` and needs a person listening. It plays the WAV files next to it in every bit depth,
  looped and once, then sweeps gain, source and listener position and orientation, a Doppler pass, several sources at
  once, stop and restart, and a seek into the middle of a long clip. The WAV files, a short and a long one per format,
  are credited in `attribs.txt` beside them. On Android, `build_android.py --interactive` runs it with the other
  functional suites (see the README), and the bundle packages only the short clips.

## Building

The module is the static library `OxC3_audio`, which links `OxC3_formats_wav`. It uses the `openal_soft` Conan package
(see [conanfile.py](../conanfile.py) and `packages/openal_soft/`) everywhere except the web, where Emscripten's own
OpenAL, backed by WebAudio, is linked instead. On Linux the build needs the ALSA and PipeWire development packages,
listed in the README.

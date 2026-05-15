# Audio Resources

The original audio assets (music tracks, SFX, voice clips) that ship with the
Melbits Pod are proprietary content of Melbot Studios, S.L. and are **not**
included in this portfolio publication.

What remains in this directory is the **build pipeline** for audio assets,
preserved as reference for the firmware's audio asset workflow:

| File | Role |
|------|------|
| `Makefile` | Drives batch conversion of `.wav` / `.mp3` source files into the firmware's custom `.ppa` packed audio format. |
| `convert_audio.bat` | Windows wrapper that invokes the Makefile through the bundled `make.exe`. |
| `convert_audio_DEBUG_wav.bat` | Same pipeline but emits intermediate `.wav` files for auditioning. |

The actual conversion logic lives in `../Buildscripts/audiocv.py`, which:

1. Decodes input `.mp3` / `.wav` via `ffmpeg` and `sox`
2. Resamples and quantizes to the target rate / bit depth
3. Encodes through the GSM 06.10 codec (see `../src/Audio/gsm.c`)
4. Packs the result into the on-device `.ppa` container consumed by the
   firmware's file server and audio playback subsystem

To rebuild the audio assets you would:

1. Drop source `.wav` / `.mp3` files into an `Audio/` subdirectory here.
2. Install `ffmpeg` and `sox` (e.g. `brew install ffmpeg sox`).
3. Run `make` (Unix) or `convert_audio.bat` (Windows).
4. Output `.ppa` files land in an `AudioConverted/` directory and are bundled
   into the firmware image at link time.

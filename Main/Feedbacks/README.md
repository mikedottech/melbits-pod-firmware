# Renoise Authoring Palette

This directory contains the **Renoise instrument palette** used to author
the POD's audio + LED + haptic feedback in [Renoise](https://www.renoise.com).

Each `.xrni` is one channel of the on-device synth, exposed to the
composer as a Renoise instrument so that tracks written in Renoise map
directly onto the firmware's voice/channel model:

| File | Maps to firmware channel |
|------|--------------------------|
| `SQR_*.xrni`, `TRI_*.xrni`, `NOISE_*.xrni` | Synth oscillators (square / triangle / noise) |
| `PCM_*.xrni` | PCM voice — streams GSM-encoded audio |
| `PWM_R_0.xrni`, `PWM_G_0.xrni`, `PWM_B_0.xrni` | RGB LED PWM channels |
| `PWM_Q1..Q4_0.xrni` | Quadrant LED PWM channels |
| `PWM_VIB_0.xrni` | Vibration motor PWM |
| `CMD_00_0.xrni` | Discrete command events |

`commands.txt` documents the discrete command opcodes used by the
`CMD_*` track type.

## What is missing

The actual song / pattern files (`.xrns`) that shipped in the product —
the music, LED choreography, and haptic sequences played for each game
event (incubation, send/receive seed, shake outcomes, battery alarm,
charge start, etc.) — are **proprietary content of Melbot Studios, S.L.**
and have been omitted from this portfolio publication, consistent with
the same policy applied to the PCM audio assets in
[`../Project/Resources/`](../Project/Resources/).

What remains here is the **authoring framework**: the bridge between
Renoise's instrument/track model and the firmware's synth/channel
model. This is the same material a sound designer would have loaded as
a template to start composing for the POD.

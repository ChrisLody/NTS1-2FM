# 2FM — Two-operator FM oscillators for the original Korg NTS-1

**2FM1** and **2FM2** are a pair of compact, complementary FM oscillators for the **original Korg NTS-1 (mkI)**, built with the Korg logue SDK 1.1.0. They share the same two-operator engine and level-dependent feedback character.

**2FM1** is an FM tone generator without a dedicated modulator envelope. Use the NTS-1 filter and built-in amplitude envelope to shape its sound.

**2FM2** adds an independent ADSR envelope to the modulator output. Its changing output level also changes the feedback character, producing lively evolving timbres reminiscent of early OPN-style FM synthesis.

Both oscillators use two sine operators with reset-to-zero phase on note-on, Shape-controlled modulation depth, and a bipolar square-like/saw-like feedback control. The NTS-1's native Shape LFO remains supported.

## Controls

| Control | 2FM1 | 2FM2 |
| --- | --- | --- |
| Shape (A) | Modulator output level / FM depth | Modulator output level / FM depth |
| Shift Shape (B) | Square-like feedback at minimum; zero at centre; saw-like feedback at maximum | Same |
| Edit 1: Ratio | Unconventional operator-ratio control (see below) | Same |
| Edit 2: Car Fine | Adds 0.000–0.100 to the carrier multiplier | Same |
| Edit 3: M Attack | — | Modulator envelope attack |
| Edit 4: M Decay | — | Modulator envelope decay |
| Edit 5: M Sustain | — | Modulator envelope sustain level |
| Edit 6: M Release | — | Modulator envelope release |

### Ratio control

The Ratio parameter is deliberately **not** a conventional signed modulator ratio. It spans **−100 to +100**:

| Ratio setting | Carrier multiplier | Modulator multiplier |
| ---: | ---: | ---: |
| −100 | 1.0 | 1.0 |
| −90 | 1.0 | 2.0 |
| −50 | 1.0 | 6.0 |
| 0 | 1.0 | 11.0 |
| +1 | 1.1 | 1.0 |
| +10 | 2.0 | 1.0 |
| +50 | 6.0 | 1.0 |
| +100 | 11.0 | 1.0 |

From −100 to 0, each step raises the **modulator** multiplier by 0.1. From +1 to +100, each step raises the **carrier** multiplier by 0.1 while the modulator returns to 1.0. The transition from 0 to +1 is intentionally discontinuous. Starting at −100 makes the original NTS-1's power-on defaults useful (1:1).

Carrier Fine is unipolar, from 0 to 100, adding 0.001 per step to the carrier multiplier.

### Feedback and Shape

The low half of Shift Shape rectifies the feedback signal for a square-like character; the high half uses ordinary self-feedback for saw-like tones and, at high settings, noise-like textures. Feedback is **level-dependent**: the previous modulator sample is fed back *after* output-level scaling. In 2FM1 that scaling comes from Shape and the native LFO; in 2FM2 it additionally includes the modulator ADSR.

The calibrated phase-feedback limits are **0.2 cycles (square-like)** and **0.5 cycles (saw-like)**. Shape's maximum phase-modulation depth is **1.0 cycle**.

### 2FM2 envelope

Attack, Decay, and Release range from instant to approximately 8 seconds, with a curved control response giving finer resolution for short times. Sustain runs from 0–100%. The modulator ADSR shapes FM timbre, **not** the carrier's final amplitude; use the NTS-1's own envelope to control volume. At startup the NTS-1 may initialise the ADSR edit parameters to zero, so configure them before expecting a sustained FM timbre.

The two oscillators are designed to complement each other. You can set up the tone in 2FM1 before selecting 2FM2 for envelope shaping, though how particular edit settings are retained on switching depends on the NTS-1's parameter handling; verify on your instrument.

## Installation

The compiled units are `2fm1.ntkdigunit` and `2fm2.ntkdigunit`. When an official release is attached, download both from **Releases** and add them to free user-oscillator slots using the original **NTS-1 Sound Librarian**, then **SEND ALL**. Back up existing custom units first.

**Compatibility:** Original NTS-1 digital (mkI), logue SDK 1.1-0; NTS-1 firmware **1.02 or newer** required. These are not NTS-1 mkII plugins.

## Building from source

The source for each oscillator is under `2fm1/` and `2fm2/`. Each contains:

- `twoopfm.cpp` — the oscillator implementation.
- `manifest.json` — the unit's metadata and edit-parameter definitions.
- `project.mk` — build target and source declarations.

These files are intended to be used **inside a copy of Korg's original NTS-1 `dummy-osc` template**, which supplies the common Makefile, linker files and unit wrapper. Those SDK template files and the ARM cross-compiler are intentionally *not* duplicated in this repository.

Install [Korg logue SDK](https://github.com/korginc/logue-sdk), initialise the `platform/ext/CMSIS` submodule, and configure the original NTS-1 ARM GCC 5.4 toolchain using Korg's instructions. For each oscillator, copy `platform/nutekt-digital/dummy-osc` to `platform/nutekt-digital/2fm1` or `2fm2`, then overwrite its `twoopfm.cpp`, `manifest.json`, and `project.mk` with the corresponding files from this repository.

Using the **MSYS2 UCRT64** shell on Windows, from within each resulting SDK project directory, the tested build commands are:

```sh
make LDDIR=ld BUILDDIR=build
make LDDIR=ld BUILDDIR=build install
```

The packaged unit appears in the project directory. Note that the `LDDIR=ld` workaround is useful with the legacy Windows linker and MSYS2 paths.

For help configuring the SDK, see [Korg's NTS-1 platform documentation](https://github.com/korginc/logue-sdk/tree/main/platform/nutekt-digital).

## Status

Source and documentation are available here while the first packaged binary release is being prepared. The two engines have been tested on an original NTS-1; users should still exercise normal care when trying third-party oscillator code.

## Credits and licence

Created by [ChrisLody](https://github.com/ChrisLody). Built against the [Korg logue SDK](https://github.com/korginc/logue-sdk). Korg's SDK is BSD 3-Clause licensed and remains copyright KORG INC.; it is not bundled here.

Source in this repository is available under the BSD 3-Clause licence; see [LICENSE](LICENSE). This project is independent of, and not endorsed by, Korg.

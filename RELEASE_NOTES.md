# 2FM v1.0.0 — First release

Two complementary two-operator FM oscillators for the **original Korg NTS-1 (mkI)**.

**2FM1** is a direct FM tone generator with no internal modulator envelope. Use its sounds with the NTS-1 filter, envelope and effects.

**2FM2** adds a dedicated four-stage modulator envelope (Attack, Decay, Sustain, Release), so timbre can change throughout the note.

Both units feature:

- Shape-controlled modulator level/FM depth, with support for the NTS-1's built-in Shape LFO.
- Bipolar Shift Shape feedback: square-like tones on one side, saw-like tones progressing to noisy textures on the other.
- Output-level-dependent self-feedback, for lively, evolving FM character.
- A two-sided ratio control that independently raises the modulator or carrier ratio from 1:1 to 11:1, in 0.1 steps.
- Unipolar carrier fine tuning (0 to +0.1 ratio).
- Phase reset on each note-on.

**Download:** attach `2fm1.ntkdigunit` and `2fm2.ntkdigunit` to this release. Load them using the original NTS-1 Sound Librarian. Firmware **1.02 or newer** is required. These files are **not compatible with the NTS-1 mkII**.

Full source code and build instructions are in the repository README.

**Release checklist:** both compiled binaries built from the published source; verify each filename and manifest; back up existing NTS-1 user slots before transferring. Do not mark as public until tested.

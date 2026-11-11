#include "userosc.h"

// ============================================================
// 2FM1 - TWO-OPERATOR FM TONE GENERATOR
// Final square-side soft shaping (drive 10), without ADSR
//
// Shape (A):       Modulator level / FM depth
// Shift Shape (B): Square-like / Saw-like feedback
// Edit parameter 1: Ratio
// Edit parameter 2: Carrier fine
//
// Square mode: soft-square shaping increases as B moves from
// centre towards its negative extreme, reaching drive 10.
// Only the signal sent to the carrier is shaped; feedback uses
// the unshaped modulator after Shape/LFO level scaling.
// Saw mode is unchanged. Both phases reset on note-on.
// ============================================================

// Operator phases.
static float carrier_phase = 0.0f;
static float modulator_phase = 0.0f;

// Frequency controls.
static int ratio_setting = -100;
static int carrier_fine_setting = 0;

// Shape and native NTS-1 LFO.
static float shape_level = 0.0f;
static float previous_lfo = 0.0f;

// Smoothed Shift Shape (B) feedback control.
static float feedback_target = -1.0f;
static float feedback_current = -1.0f;

// Previous sample after Shape/LFO scaling, before square shaping.
static float previous_modulator = 0.0f;

// Final calibration, matching 2FM2.
static const float square_feedback_max = 0.35f;
static const float saw_feedback_max = 0.5f;
static const float fm_depth_max = 1.0f;
static const float square_drive_max = 10.0f;

// Odd-symmetric, peak-preserving soft-square shaper.
// The output sent to the feedback loop intentionally bypasses this.
static inline float soft_square(const float x, const float drive)
{
    const float ax = (x < 0.0f) ? -x : x;
    return x * (1.0f + drive) / (1.0f + drive * ax);
}

static inline float wrap_phase(float phase)
{
    while (phase >= 1.0f)
        phase -= 1.0f;

    while (phase < 0.0f)
        phase += 1.0f;

    return phase;
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;

    carrier_phase = 0.0f;
    modulator_phase = 0.0f;

    ratio_setting = -100;
    carrier_fine_setting = 0;

    shape_level = 0.0f;
    previous_lfo = 0.0f;

    feedback_target = -1.0f;
    feedback_current = -1.0f;
    previous_modulator = 0.0f;
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
    const float increment = osc_w0f_for_note(
        params->pitch >> 8,
        params->pitch & 0xFF
    );

    // Negative Ratio settings raise the modulator; positive
    // settings raise the carrier. Fine adjusts the carrier only.
    float carrier_ratio = 1.0f;
    float modulator_ratio = 1.0f;

    if (ratio_setting <= 0)
        modulator_ratio = 1.0f + (ratio_setting + 100) * 0.1f;
    else
        carrier_ratio = 1.0f + ratio_setting * 0.1f;

    carrier_ratio += carrier_fine_setting * 0.001f;

    const float carrier_increment = increment * carrier_ratio;
    const float modulator_increment = increment * modulator_ratio;

    const float target_lfo = q31_to_f32(params->shape_lfo);
    const float lfo_step = (frames > 0)
        ? (target_lfo - previous_lfo) / frames
        : 0.0f;
    float lfo = previous_lfo;

    for (uint32_t i = 0; i < frames; i++)
    {
        // Smooth B, but do not low-pass the feedback audio.
        feedback_current +=
            0.004f * (feedback_target - feedback_current);

        float magnitude = feedback_current;
        if (magnitude < 0.0f)
            magnitude = -magnitude;

        // Cubic response on BOTH halves of B.
        const float feedback_curve = magnitude * magnitude * magnitude;
        const float feedback_max = (feedback_current < 0.0f)
            ? square_feedback_max
            : saw_feedback_max;
        const float feedback_amount = feedback_curve * feedback_max;

        // Rectify previous output only on the negative (square) side.
        // The positive (saw) side retains the original self-feedback.
        float feedback_input = previous_modulator;
        if (feedback_current < 0.0f && feedback_input < 0.0f)
            feedback_input = -feedback_input;

        const float mod_phase = wrap_phase(
            modulator_phase + feedback_input * feedback_amount
        );
        const float modulator = osc_sinf(mod_phase);

        // Shape the modulator sent to the carrier on the square side
        // only. Checking both current and target avoids shaping
        // leaking into the saw side during B-knob smoothing.
        float modulator_for_carrier = modulator;
        if (feedback_target < 0.0f && feedback_current < 0.0f)
        {
            const float drive = square_drive_max * magnitude;
            modulator_for_carrier = soft_square(modulator, drive);
        }

        const float depth =
            clip01f(shape_level + lfo) * fm_depth_max;

        // The carrier hears the shaped modulator on the square side.
        const float modulator_output = modulator_for_carrier * depth;

        // Feedback instead receives the UNshaped level-scaled sample.
        // This retains output-level-dependent feedback while keeping
        // the new square waveshaper outside the feedback loop.
        previous_modulator = modulator * depth;

        const float fm_phase = wrap_phase(
            carrier_phase + modulator_output
        );

        const float sample = 0.7f * osc_sinf(fm_phase);
        yn[i] = f32_to_q31(sample);

        carrier_phase += carrier_increment;
        modulator_phase += modulator_increment;
        carrier_phase = wrap_phase(carrier_phase);
        modulator_phase = wrap_phase(modulator_phase);
        lfo += lfo_step;
    }

    previous_lfo = target_lfo;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
    (void)params;

    carrier_phase = 0.0f;
    modulator_phase = 0.0f;
    previous_modulator = 0.0f;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
    switch (index)
    {
        // Ratio: displayed as -100 to +100, SDK sends 0 to 200.
        case k_user_osc_param_id1:
        {
            int setting = (int)value - 100;
            if (setting < -100) setting = -100;
            if (setting > 100) setting = 100;
            ratio_setting = setting;
            break;
        }

        // Carrier fine: 0 to +0.1 in 0.001 steps.
        case k_user_osc_param_id2:
        {
            if (value > 100) value = 100;
            carrier_fine_setting = (int)value;
            break;
        }

        // Shape (A): modulator level / FM depth.
        case k_user_osc_param_shape:
            shape_level = param_val_to_f32(value);
            break;

        // Shift Shape (B): negative square, centre zero, positive saw.
        case k_user_osc_param_shiftshape:
            if (value > 1023) value = 1023;
            if (value < 512)
                feedback_target = ((int)value - 512) / 512.0f;
            else
                feedback_target = ((int)value - 512) / 511.0f;
            break;

        default:
            break;
    }
}
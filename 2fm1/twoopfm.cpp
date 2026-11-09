#include "userosc.h"

// ============================================================
// 2FM1 - TWO-OPERATOR FM TONE GENERATOR
// Level-dependent feedback version (matched to 2FM2)
//
// Shape (A):       Modulator level / FM depth
// Shift Shape (B): Square / Saw feedback
// Parameter 1:     Ratio
// Parameter 2:     Carrier Fine
//
// No ADSR in 2FM1: Shape alone controls the modulator level.
// That level now also controls the strength of self-feedback.
// ============================================================

// Operator phases
static float carrier_phase = 0.0f;
static float modulator_phase = 0.0f;

// Frequency controls
static int ratio_setting = -100;
static int carrier_fine_setting = 0;

// Shape and native LFO
static float shape_level = 0.0f;
static float previous_lfo = 0.0f;

// Feedback
static float feedback_target = -1.0f;
static float feedback_current = -1.0f;

// Stores the modulator output AFTER Shape/LFO scaling.
static float previous_modulator = 0.0f;

// User-tested feedback limits (identical to 2FM2).
static const float square_feedback_max = 0.2f;
static const float saw_feedback_max = 0.5f;
static const float fm_depth_max = 1.0f;

// ============================================================
// PHASE WRAPPING
// ============================================================

static inline float wrap_phase(float phase)
{
    while (phase >= 1.0f)
        phase -= 1.0f;

    while (phase < 0.0f)
        phase += 1.0f;

    return phase;
}

// ============================================================
// INITIALISATION
// ============================================================

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

// ============================================================
// AUDIO GENERATION
// ============================================================

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
    const float increment = osc_w0f_for_note(
        params->pitch >> 8,
        params->pitch & 0xFF
    );

    // -100 to 0: Increase modulator ratio from 1 to 11.
    // +1 to +100: Increase carrier ratio from 1.1 to 11.
    float carrier_ratio = 1.0f;
    float modulator_ratio = 1.0f;

    if (ratio_setting <= 0)
        modulator_ratio = 1.0f + (ratio_setting + 100) * 0.1f;
    else
        carrier_ratio = 1.0f + ratio_setting * 0.1f;

    // Carrier Fine adds 0.000 to 0.100.
    carrier_ratio += carrier_fine_setting * 0.001f;

    const float carrier_increment = increment * carrier_ratio;
    const float modulator_increment = increment * modulator_ratio;

    // NTS-1 native LFO interpolation, unchanged.
    const float target_lfo = q31_to_f32(params->shape_lfo);
    const float lfo_step = (frames > 0)
        ? (target_lfo - previous_lfo) / frames
        : 0.0f;
    float lfo = previous_lfo;

    for (uint32_t i = 0; i < frames; i++)
    {
        // Smooth B knob movements, without filtering the feedback audio.
        feedback_current +=
            0.004f * (feedback_target - feedback_current);

        float magnitude = feedback_current;
        if (magnitude < 0.0f)
            magnitude = -magnitude;

        // Preserve the user-tested squared feedback curve.
        const float feedback_curve = magnitude * magnitude;
        const float feedback_max = (feedback_current < 0.0f)
            ? square_feedback_max
            : saw_feedback_max;
        const float feedback_amount = feedback_curve * feedback_max;

        // Feedback uses the previous level-scaled modulator output.
        // Square side rectifies it; saw side uses it directly.
        float feedback_input = previous_modulator;
        if (feedback_current < 0.0f && feedback_input < 0.0f)
            feedback_input = -feedback_input;

        const float mod_phase = wrap_phase(
            modulator_phase + feedback_input * feedback_amount
        );
        const float modulator = osc_sinf(mod_phase);

        // Shape and native LFO set the output level of the modulator.
        const float depth =
            clip01f(shape_level + lfo) * fm_depth_max;

        // CHANGE: Feed the same Shape-scaled output into BOTH
        // the carrier's phase input and the modulator feedback history.
        // In 2FM2 this is additionally multiplied by the ADSR;
        // 2FM1 deliberately has no envelope.
        const float modulator_output = modulator * depth;
        previous_modulator = modulator_output;

        const float fm_phase = wrap_phase(
            carrier_phase + modulator_output
        );

        const float sample = 0.5f * osc_sinf(fm_phase);
        yn[i] = f32_to_q31(sample);

        carrier_phase += carrier_increment;
        modulator_phase += modulator_increment;
        carrier_phase = wrap_phase(carrier_phase);
        modulator_phase = wrap_phase(modulator_phase);
        lfo += lfo_step;
    }

    previous_lfo = target_lfo;
}

// ============================================================
// NOTE ON
// ============================================================

void OSC_NOTEON(const user_osc_param_t * const params)
{
    (void)params;

    carrier_phase = 0.0f;
    modulator_phase = 0.0f;
    previous_modulator = 0.0f;
}

// ============================================================
// PARAMETER HANDLING
// ============================================================

void OSC_PARAM(uint16_t index, uint16_t value)
{
    switch (index)
    {
        // Ratio: -100 to +100
        case k_user_osc_param_id1:
        {
            int setting = (int)value - 100;
            if (setting < -100) setting = -100;
            if (setting > 100) setting = 100;
            ratio_setting = setting;
            break;
        }

        // Carrier Fine: 0 to 100
        case k_user_osc_param_id2:
        {
            if (value > 100) value = 100;
            carrier_fine_setting = (int)value;
            break;
        }

        // Shape: FM depth and feedback scaling
        case k_user_osc_param_shape:
        {
            shape_level = param_val_to_f32(value);
            break;
        }

        // Shift Shape: square / saw feedback
        case k_user_osc_param_shiftshape:
        {
            if (value > 1023) value = 1023;
            if (value < 512)
                feedback_target = ((int)value - 512) / 512.0f;
            else
                feedback_target = ((int)value - 512) / 511.0f;
            break;
        }

        default:
            break;
    }
}
#include "userosc.h"

// ============================================================
// 2FM2 - TWO-OPERATOR FM WITH MODULATOR ADSR
// Final square-side soft shaping (drive 10)
//
// Shape (A):       Modulator level / FM depth
// Shift Shape (B): Square / Saw feedback
// Parameter 1:     Ratio
// Parameter 2:     Carrier Fine
// Parameter 3:     Modulator Attack
// Parameter 4:     Modulator Decay
// Parameter 5:     Modulator Sustain
// Parameter 6:     Modulator Release
//
// Square mode: soft-square shaping grows as Shift Shape moves
// from centre to the negative extreme, reaching a drive of 10.
// The shaper affects only the carrier modulation signal.
// Feedback continues to use the unshaped modulator after its
// envelope and Shape/LFO scaling. Saw mode is unchanged.
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

// Feedback controls
static float feedback_target = -1.0f;
static float feedback_current = -1.0f;

// Level-scaled, unshaped modulator sample for self-feedback.
static float previous_modulator = 0.0f;

// Calibrated maximum feedback and FM depth.
static const float square_feedback_max = 0.35f;
static const float saw_feedback_max = 0.5f;
static const float fm_depth_max = 1.0f;

// Final fixed square-side drive; no additional edit parameter.
static const float square_drive_max = 10.0f;

// Odd-symmetric, peak-preserving soft-square shaper.
// Applied outside the feedback loop on the negative B side only.
static inline float soft_square(const float x, const float drive)
{
    const float ax = (x < 0.0f) ? -x : x;
    return x * (1.0f + drive) / (1.0f + drive * ax);
}

// ============================================================
// MODULATOR ENVELOPE
// ============================================================

static const float sample_rate = 48000.0f;

enum EnvelopeStage
{
    ENV_OFF,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
};

static EnvelopeStage env_stage = ENV_OFF;
static float env_level = 0.0f;
static float attack_time = 0.0f;
static float decay_time = 0.0f;
static float release_time = 0.0f;
static float sustain_level = 0.0f;
static float attack_step = 1.0f;
static float decay_step = 1.0f;
static float release_step = 1.0f;

static inline float envelope_time(uint16_t value)
{
    if (value == 0)
        return 0.0f;

    if (value > 100)
        value = 100;

    const float x = value * 0.01f;
    return 0.002f + 7.998f * x * x * x;
}

static void update_envelope_rates()
{
    if (attack_time > 0.0f)
        attack_step = 1.0f / (attack_time * sample_rate);
    else
        attack_step = 1.0f;

    if (decay_time > 0.0f)
        decay_step = (1.0f - sustain_level) /
                     (decay_time * sample_rate);
    else
        decay_step = 1.0f;
}

static inline float advance_envelope()
{
    switch (env_stage)
    {
        case ENV_ATTACK:
            env_level += attack_step;
            if (env_level >= 1.0f)
            {
                env_level = 1.0f;
                if (decay_time > 0.0f)
                    env_stage = ENV_DECAY;
                else
                {
                    env_level = sustain_level;
                    env_stage = ENV_SUSTAIN;
                }
            }
            break;

        case ENV_DECAY:
            env_level -= decay_step;
            if (env_level <= sustain_level)
            {
                env_level = sustain_level;
                env_stage = ENV_SUSTAIN;
            }
            break;

        case ENV_SUSTAIN:
            env_level = sustain_level;
            break;

        case ENV_RELEASE:
            env_level -= release_step;
            if (env_level <= 0.0f)
            {
                env_level = 0.0f;
                env_stage = ENV_OFF;
            }
            break;

        case ENV_OFF:
        default:
            env_level = 0.0f;
            break;
    }

    return env_level;
}

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

    env_stage = ENV_OFF;
    env_level = 0.0f;
    attack_time = 0.0f;
    decay_time = 0.0f;
    sustain_level = 0.0f;
    release_time = 0.0f;
    release_step = 1.0f;

    update_envelope_rates();
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

    // Ratio mapping: negative settings raise the modulator;
    // positive settings raise the carrier.
    float carrier_ratio = 1.0f;
    float modulator_ratio = 1.0f;

    if (ratio_setting <= 0)
        modulator_ratio = 1.0f + (ratio_setting + 100) * 0.1f;
    else
        carrier_ratio = 1.0f + ratio_setting * 0.1f;

    carrier_ratio += carrier_fine_setting * 0.001f;

    const float carrier_increment = increment * carrier_ratio;
    const float modulator_increment = increment * modulator_ratio;

    // Native NTS-1 LFO.
    const float target_lfo = q31_to_f32(params->shape_lfo);
    const float lfo_step = (frames > 0)
        ? (target_lfo - previous_lfo) / frames
        : 0.0f;
    float lfo = previous_lfo;

    for (uint32_t i = 0; i < frames; i++)
    {
        // Smooth the B knob, not the audio feedback.
        feedback_current +=
            0.004f * (feedback_target - feedback_current);

        float magnitude = feedback_current;
        if (magnitude < 0.0f)
            magnitude = -magnitude;

        // Retain the user's cubic feedback control curve.
        const float feedback_curve = magnitude * magnitude * magnitude;
        const float feedback_max = (feedback_current < 0.0f)
            ? square_feedback_max
            : saw_feedback_max;
        const float feedback_amount = feedback_curve * feedback_max;

        // Square mode rectifies the PREVIOUS level-scaled
        // modulator output; saw mode uses it unchanged.
        float feedback_input = previous_modulator;
        if (feedback_current < 0.0f && feedback_input < 0.0f)
            feedback_input = -feedback_input;

        // Feedback shapes the modulator before its envelope.
        const float mod_phase = wrap_phase(
            modulator_phase + feedback_input * feedback_amount
        );
        const float modulator = osc_sinf(mod_phase);

        // The negative B side increasingly soft-squares the
        // signal sent to the carrier. The positive (saw) side
        // bypasses shaping entirely. The target check prevents
        // temporary shaping when B is switched to the saw side.
        float modulator_for_carrier = modulator;
        if (feedback_target < 0.0f && feedback_current < 0.0f)
        {
            const float drive = square_drive_max * magnitude;
            modulator_for_carrier = soft_square(modulator, drive);
        }

        // Advance the modulator ADSR.
        const float envelope = advance_envelope();

        // Shape and the native LFO determine depth; the modulator
        // ADSR controls its level over time.
        const float depth =
            clip01f(shape_level + lfo) * fm_depth_max;

        // The carrier receives the shaped signal on the square
        // side, and the unmodified modulator on the saw side.
        const float modulator_output =
            modulator_for_carrier * envelope * depth;

        // Feedback uses the unshaped output after envelope and
        // depth scaling: level-dependent feedback is preserved.
        previous_modulator = modulator * envelope * depth;

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

// ============================================================
// NOTE ON AND NOTE OFF
// ============================================================

void OSC_NOTEON(const user_osc_param_t * const params)
{
    (void)params;

    carrier_phase = 0.0f;
    modulator_phase = 0.0f;
    previous_modulator = 0.0f;

    env_level = 0.0f;
    env_stage = ENV_ATTACK;

    if (attack_time <= 0.0f)
    {
        env_level = 1.0f;
        env_stage = ENV_DECAY;

        if (decay_time <= 0.0f)
        {
            env_level = sustain_level;
            env_stage = ENV_SUSTAIN;
        }
    }
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
    (void)params;

    if (release_time <= 0.0f || env_level <= 0.0f)
    {
        env_level = 0.0f;
        env_stage = ENV_OFF;
    }
    else
    {
        release_step = env_level / (release_time * sample_rate);
        env_stage = ENV_RELEASE;
    }
}

// ============================================================
// PARAMETER HANDLING
// ============================================================

void OSC_PARAM(uint16_t index, uint16_t value)
{
    switch (index)
    {
        case k_user_osc_param_id1:
        {
            int setting = (int)value - 100;
            if (setting < -100) setting = -100;
            if (setting > 100) setting = 100;
            ratio_setting = setting;
            break;
        }

        case k_user_osc_param_id2:
        {
            if (value > 100) value = 100;
            carrier_fine_setting = (int)value;
            break;
        }

        case k_user_osc_param_id3:
            attack_time = envelope_time(value);
            update_envelope_rates();
            break;

        case k_user_osc_param_id4:
            decay_time = envelope_time(value);
            update_envelope_rates();
            break;

        case k_user_osc_param_id5:
            if (value > 100) value = 100;
            sustain_level = value * 0.01f;
            update_envelope_rates();
            break;

        case k_user_osc_param_id6:
        {
            release_time = envelope_time(value);
            if (env_stage == ENV_RELEASE)
            {
                if (release_time <= 0.0f)
                {
                    env_level = 0.0f;
                    env_stage = ENV_OFF;
                }
                else
                {
                    release_step = env_level /
                                   (release_time * sample_rate);
                }
            }
            break;
        }

        case k_user_osc_param_shape:
            shape_level = param_val_to_f32(value);
            break;

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
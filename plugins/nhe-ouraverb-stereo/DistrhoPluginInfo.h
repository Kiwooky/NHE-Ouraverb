#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

// Ouraverb Stereo: the same source as plugins/nhe-ouraverb, built with two inputs.
// Rename these together with the TTL (see docs/lv2-and-mod-rules.md).
#define DISTRHO_PLUGIN_BRAND       "New Horizon"
#define DISTRHO_PLUGIN_NAME        "Ouraverb Stereo"
#define DISTRHO_PLUGIN_URI         "urn:mod-cookbook:nhe-ouraverb-stereo"
#define OURAVERB_LABEL             "OuraverbStereo"
#define OURAVERB_DESCRIPTION       "Outdoor spaces, stereo in: fjords, canyons, valleys, forests and a stepped amphitheatre, with a canopy you can pull over the sky."
#define OURAVERB_UNIQUE_ID         d_cconst('n', 'O', 'u', 'S')

#define DISTRHO_PLUGIN_HAS_UI       0
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
#define DISTRHO_PLUGIN_NUM_INPUTS   2   // stereo in: one engine per input
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2

// Control ports, in TTL order (indices start after the audio ports).
// The bypass port is always last. Place values only ever get APPENDED.
enum Parameters {
    kPlace = 0,
    kExpanse,
    kSurface,
    kAir,
    kBounce,
    kCover,
    kCoverDensity,
    kCoverHeight,
    kMix,
    kTone,            // settings only for now: which pitches the place throws back
    kTails,
    kInputLevel,      // output: playing level, for the face's orbit ripples
    kBypass,
    kParameterCount
};

// Port groups: how MOD's settings panel sections the controls
enum PortGroups {
    kGroupLocation = 0,   // Place, Expanse, Surface
    kGroupDistance,       // Cover height, Air, Bounce, Tone
    kGroupCover,          // Ceiling cover, Cover density
    kGroupMix,            // Mix, Tails
    kGroupCount
};

#endif

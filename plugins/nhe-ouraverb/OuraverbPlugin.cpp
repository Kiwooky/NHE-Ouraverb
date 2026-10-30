// Ouraverb: outdoor spaces. A reverb about boundaries, made of tails.
// Mountains, fjords and amphitheatres don't make a dense wash like a room: they
// send back a few distinct echoes from distant surfaces, each darker than the last.
// Imagined, not a recreation: every mapping below is ours [guess, tuned by test].
//
// Traits (docs/effect-profile.md): [every], [buffer], [tail], [feedback], [mix], stereo out.
//
// Mono (1 in, 2 out) and Stereo (2 in, 2 out) are built from this one file; DistrhoPluginInfo.h
// picks which. Stereo runs two engines: the left input's engine makes the left output, the right
// input's makes the right, each skipping the side nobody hears. That is exactly two mono
// instances with one output taken from each, at a fraction of the cost; with the same signal on
// both inputs it is sample for sample the mono plugin.
//
// Signal flow (per engine: one input, stereo out):
//   in -> 60 Hz high-pass -> one long input line
//   -> the PLACE (geometry, from a table):
//        * up to 10 taps: other surfaces (headlands, ledges, trunks), each with its
//          own time, level, pan and air loss (farther = darker)
//        * a step train (Colosseum): evenly spaced reflections off the seating rows,
//          spacing growing row by row, which a clap hears as a falling chirp
//        * the walls: two cross-fed loop lines, one per wall, so the echo ping-pongs
//          left-right. A floor tap inside the loop (water, sand) thickens every lap.
//          Bounce sets how many laps survive
//   -> SURFACE (snow 0 .. foliage ~3 .. rock 10): reflectivity, damping, diffusion (a diffuse
//      part next to the mirror part) and scatter (tiny offsets on tap times)
//   -> COVER: a short filtered feedback comb (sky to floor and back). Cover blends it
//      in and closes the sky, Cover height tunes it (20 m flutters, 2 m drones), Cover
//      density sets canvas (0: dark, leaky) .. ice (10: bright, glassy)
//   -> TONE (settings only for now): which pitches the place throws back. A surface only
//      reflects wavelengths smaller than itself (343 m/s / f: 100 Hz = 3.4 m), so hills,
//      trees and glass send the highs back crisp while the lows wrap around them and roll on.
//      Bright (6..10): reflections lose their lows below ~450 Hz, and those lows feed a slow,
//      diffuse rumble that rolls on with Bounce (rolling thunder). Warm (0..4): the highs are
//      absorbed instead and the low mids come back fuller. 5 = exactly as before (no-op)
//   -> soft limit on the wet path -> Mix
// Expanse (2 .. 200 %) scales every time in the place; changing it glides like tape.
// A Fjord at 3 % is a city street: two building fronts 13 m apart, a slap-flutter.
// Changing Place ducks the wet, swaps the geometry and fades back in.
#include "DistrhoPlugin.hpp"

#include <cmath>
#include <cstring>
#include <cstdint>

START_NAMESPACE_DISTRHO

static const uint32_t kLineSize = 1u << 20;    // [buffer] 10.9 s at 96 kHz (the longest tap at Expanse 200 % is 9.9 s)
static const uint32_t kLineMask = kLineSize - 1;
static const uint32_t kCombSize = 1u << 16;    // 0.68 s at 96 kHz (Cover height 60 m round trip: 0.35 s)
static const uint32_t kCombMask = kCombSize - 1;
static const uint32_t kApSize   = 2048;        // up to 21 ms at 96 kHz
static const uint32_t kRumSize  = 1u << 17;    // 1.36 s at 96 kHz: the rumble's two lines
static const uint32_t kRumMask  = kRumSize - 1;
static const int      kMaxTaps  = 10;
static const int      kMaxSteps = 16;
static const float    kSound    = 343.0f;      // m/s

static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float onePoleCoef(float hz, float sr) { return 1.0f - std::exp(-6.2831853f * hz / sr); }

// rational tanh: flat beyond |x| = 3, no libm call per sample
static inline float softClip(float x)
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// [feedback] linear below k, approaches k + r smoothly, never above it
static inline float softKnee(float x, float k, float r)
{
    const float a = std::fabs(x);
    if (a <= k) return x;
    const float y = k + r * softClip((a - k) / r);
    return x < 0.0f ? -y : y;
}

// ---------------------------------------------------------------------------------
// Places: geometry as data. Times in seconds at Expanse 100 %. Append only: MOD
// saves the Place index in pedalboards and presets.
struct Tap { float t, g, pan; };
struct Scene {
    const char* name;
    float wallL, wallR;        // round trip to each wall (the loop lap times)
    float wallGain;            // how loud the walls answer
    float floorT, floorG;      // the floor bounce riding each lap
    int   nTaps;
    Tap   taps[kMaxTaps];
    int   nSteps;              // step train (0 = none; even, the rows alternate left/right)
    float stepT0, stepDt, stepStretch, stepG, stepDecay;
    float trim;                // wet level, so scenes sit near each other [measured]
};

static const Scene kScenes[] = {
    // Fjord: two rock walls about 1 km apart, you a little off-centre, a water floor.
    { "Fjord", 2.6f, 3.2f, 0.80f, 0.007f, 0.60f, 4,
      { {0.42f, 0.20f, -0.3f}, {1.35f, 0.15f, 0.2f}, {3.70f, 0.30f, -0.6f}, {4.40f, 0.22f, 0.7f} },
      0, 0, 0, 0, 0, 0, 1.00f },
    // Canyon: walls 150-250 m away, ledges and side gullies answering in between.
    { "Canyon", 0.90f, 1.25f, 0.85f, 0.004f, 0.30f, 6,
      { {0.31f, 0.40f, -0.5f}, {0.47f, 0.35f, 0.6f}, {0.66f, 0.30f, -0.2f},
        {1.70f, 0.30f, 0.4f}, {2.20f, 0.22f, -0.7f}, {2.90f, 0.15f, 0.1f} },
      0, 0, 0, 0, 0, 0, 0.80f },
    // Valley: distant, rolling slopes; many soft answers over seconds.
    { "Valley", 2.1f, 2.9f, 0.50f, 0.012f, 0.25f, 6,
      { {0.90f, 0.20f, -0.3f}, {1.60f, 0.35f, 0.5f}, {2.40f, 0.30f, -0.6f},
        {3.10f, 0.25f, 0.3f}, {3.90f, 0.18f, -0.2f}, {4.60f, 0.12f, 0.7f} },
      0, 0, 0, 0, 0, 0, 1.00f },
    // Forest: trunks all around, close; a short haze rather than echoes.
    { "Forest", 0.060f, 0.085f, 0.35f, 0.003f, 0.20f, 10,
      { {0.013f, 0.40f, -0.5f}, {0.021f, 0.35f, 0.5f}, {0.034f, 0.30f, -0.2f}, {0.047f, 0.30f, 0.6f},
        {0.062f, 0.25f, -0.7f}, {0.081f, 0.22f, 0.3f}, {0.110f, 0.18f, -0.4f}, {0.150f, 0.14f, 0.5f},
        {0.200f, 0.10f, 0.0f}, {0.270f, 0.08f, -0.3f} },
      0, 0, 0, 0, 0, 0, 0.75f },
    // Colosseum: a stepped bowl 190 m across, no lid. The seating rows (about 0.72 m
    // each, 4.2 ms round trip) answer one after another: a chirp falling from ~240 Hz.
    { "Colosseum", 0.55f, 0.58f, 0.55f, 0.006f, 0.20f, 4,
      { {0.018f, 0.40f, -0.6f}, {0.029f, 0.35f, 0.6f}, {0.050f, 0.30f, 0.0f}, {1.10f, 0.30f, 0.2f} },
      16, 0.035f, 0.0042f, 0.025f, 0.45f, 0.90f, 0.85f },
};
static const int kNumScenes = (int)(sizeof(kScenes) / sizeof(kScenes[0]));

// fixed "random" offsets per tap and step for Scatter, in -1..1
static const float kRnd[kMaxTaps + kMaxSteps] = {
     0.62f, -0.41f,  0.18f, -0.87f,  0.35f, -0.12f,  0.93f, -0.66f,  0.07f, -0.29f,
    -0.54f,  0.81f, -0.23f,  0.47f, -0.95f,  0.14f,  0.70f, -0.38f,  0.26f, -0.75f,
     0.58f, -0.06f,  0.89f, -0.49f,  0.33f, -0.17f };

// Schroeder allpass (lattice form). g = 0 is a plain delay of len samples.
struct Allpass {
    float buf[kApSize];
    uint32_t pos = 0, len = 1;
    void clear() { std::memset(buf, 0, sizeof(buf)); pos = 0; }
    inline float process(float x, float g)
    {
        uint32_t r = pos + kApSize - len; if (r >= kApSize) r -= kApSize;
        const float d = buf[r];
        const float w = x + g * d + 1e-18f;
        buf[pos] = w;
        if (++pos == kApSize) pos = 0;
        return d - g * w;
    }
};

static inline float readLin(const float* buf, uint32_t mask, uint32_t w, float d)
{
    const uint32_t ip = (uint32_t)d;
    const float t = d - (float)ip;
    const float a = buf[(w - ip) & mask];
    const float b = buf[(w - ip - 1) & mask];
    return a + t * (b - a);
}

static const float kApWetMs[2][4] = { { 4.7f, 6.3f, 8.9f, 11.3f }, { 5.1f, 6.9f, 9.4f, 12.1f } };
static const float kApLoopMs[2][2] = { { 5.3f, 7.9f }, { 6.1f, 8.7f } };
static const float kApRumMs[2] = { 13.1f, 17.3f };   // smear the rumble so it rolls, never echoes
static const float kRumOut = 2.0f;                   // rumble level at Tone 10 [tuned by test]

// ---------------------------------------------------------------------------------
// One complete Ouraverb: input line, place, walls, Tone, Surface, cover, mix.
// process() renders one or both output sides; a side nobody hears is skipped (wantL /
// wantR) wherever that can't change the other side.
class Engine
{
public:
    Engine()
    {
        fIn  = new float[kLineSize];
        fLpL = new float[kLineSize];
        fLpR = new float[kLineSize];
        fRumA = new float[kRumSize];
        fRumB = new float[kRumSize];
        clearAll();
    }

    ~Engine()
    {
        delete[] fIn; delete[] fLpL; delete[] fLpR; delete[] fRumA; delete[] fRumB;
    }

    void activate(float sr)
    {
        fSr = sr;
        fSmooth  = onePoleCoef(1.0f / (6.2831853f * 0.010f), fSr);   // 10 ms: gains
        fDistSm  = onePoleCoef(1.0f / (6.2831853f * 0.300f), fSr);   // 300 ms: Expanse glides like tape
        fHgtSm   = onePoleCoef(1.0f / (6.2831853f * 0.120f), fSr);   // 120 ms: Cover height sweeps (the flange)
        fXfDown  = onePoleCoef(1.0f / (6.2831853f * 0.012f), fSr);   // scene change: duck ~40 ms
        fXfUp    = onePoleCoef(1.0f / (6.2831853f * 0.050f), fSr);   // ...and back in ~150 ms
        fHpIn    = onePoleCoef(60.0f, fSr);
        fHpLoop  = onePoleCoef(90.0f, fSr);
        fItdLoop = (uint32_t)(0.0006f * fSr);
        fXover   = onePoleCoef(450.0f, fSr);                         // Tone's split: below this, lows wrap around
        fRumLp   = onePoleCoef(400.0f, fSr);                         // the rumble stays a rumble
        fRumHp   = onePoleCoef(25.0f, fSr);
        for (int c = 0; c < 2; ++c) fApRum[c].len = (uint32_t)(kApRumMs[c] * 0.001f * fSr);                        // 29 samples at 48 kHz, 58 at 96
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) fApWet[c][k].len = (uint32_t)(kApWetMs[c][k] * 0.001f * fSr);
            for (int k = 0; k < 2; ++k) fApLoop[c][k].len = (uint32_t)(kApLoopMs[c][k] * 0.001f * fSr);
            fApLoopSamples[c] = (float)(fApLoop[c][0].len + fApLoop[c][1].len);
        }
        clearAll();
        fFirst = true;
    }

    // outL / outR may be null for a side that isn't wanted. A template, so each combination
    // compiles to its own loop with the unwanted side's work removed (mono: both wanted, the same
    // loop as before the stereo version existed)
    template <bool wantL, bool wantR>
    void process(const float* in, float* outL, float* outR, uint32_t frames, const float* P)
    {
        const int   sceneT  = (int)clampf(std::floor(P[kPlace] + 0.5f), 0.0f, (float)(kNumScenes - 1));
        const float distT   = clampf(P[kExpanse], 2.0f, 200.0f) * 0.01f;
        // s: 0 = rock .. 1 = snow. Written (10 - d) * 0.1, which is exactly 0 at Surface 10.
        // 1 - d * 0.1 is not: ARM fuses it into a multiply-add that lands a hair below zero,
        // a clamp around it got the unfused copy, and pow() of the negative gave NaN (build 1, under qemu)
        const float s       = (10.0f - clampf(P[kSurface], 0.0f, 10.0f)) * 0.1f;
        // Air: 0 = dense (the journey eats the top end) .. 10 = open (clear air), as on the face
        const float air     = 10.0f - clampf(P[kAir], 0.0f, 10.0f);
        const float bnc     = clampf(P[kBounce], 0.0f, 10.0f) * 0.1f;
        const float coverT  = clampf(P[kCover], 0.0f, 100.0f) * 0.01f;
        const float hgtT    = clampf(P[kCoverHeight], 1.0f, 60.0f);
        const float mat     = clampf(P[kCoverDensity], 0.0f, 10.0f) * 0.1f;
        const float mix     = clampf(P[kMix], 0.0f, 100.0f) * 0.01f;
        const bool  bypass  = P[kBypass] > 0.5f;
        const bool  tails   = P[kTails] > 0.5f;
        // Tone: t = -1 (warm) .. 0 (as before) .. +1 (bright). Written (d - 5) / 5: exactly 0 at 5
        const float t       = (clampf(P[kTone], 0.0f, 10.0f) - 5.0f) * 0.2f;
        const float br      = t > 0.0f ? t : 0.0f, wm = t < 0.0f ? -t : 0.0f;
        // early reflections may lift a band a little; inside the loop gains stay <= 1 (it recirculates)
        // two cascaded first-order shelves, each with the square root of the gain: exact at the
        // band edges, steeper between (a two-pole subtractive split leaked the lows through its phase)
        const float eLoT    = std::sqrt(1.0f - 0.9f * br + 0.3f * wm);   // lows: 0.1 (bright) .. 1.3 (warm), per stage
        const float eHiT    = std::sqrt(1.0f + 0.25f * br - 0.75f * wm); // highs: 1.25 (bright) .. 0.25 (warm), per stage
        const float lLoT    = 1.0f - 0.6f * br;                          // per lap
        const float lHiT    = 1.0f - 0.6f * wm;                          // per lap (it compounds)
        const float rumT    = br;                                        // rumble level

        // Surface 10 = rock, ~3 = foliage, 0 = fresh snow; s runs the other way [guess]
        const float refl    = 1.0f - 0.75f * std::pow(std::fmax(s, 0.0f), 1.3f);           // how much each reflection returns
        const float fcSurf  = 18000.0f * std::exp(-3.4f * s);                          // damping per reflection
        const float diffT   = clampf(s / 0.6f, 0.0f, 1.0f);               // share that comes back diffuse
        const float apLoopG = 0.55f * diffT;                              // smearing inside the loop
        const float scatT   = 0.08f * s;                                  // up to 8 % offsets on tap times

        // Bounce: per-wall feedback. Cover closes the sky, so less energy escapes upward.
        const float fbT     = clampf(0.95f * std::pow(bnc, 0.6f) * refl * (1.0f + 0.25f * coverT), 0.0f, 0.93f);

        // Canopy: canvas (0) .. ice (10)
        const float matRefl = 0.55f + 0.35f * mat;
        const float combFbT = coverT * matRefl;
        const float combLp  = onePoleCoef(1200.0f * std::exp(2.015f * mat), fSr);   // 1.2 .. 9 kHz
        const float hgtSmpT = 2.0f * hgtT / kSound * fSr;                 // sky and back
        const float kin     = std::sqrt(1.0f - combFbT * combFbT);        // keeps the comb's average level flat

        // [mix] dry stays at unity up to 50 %, wet reaches full at 50 %
        const float dryMix  = mix < 0.5f ? 1.0f : 2.0f * (1.0f - mix);
        const float wetMix  = mix < 0.5f ? 2.0f * mix : 1.0f;
        const float inT     = bypass ? 0.0f : 1.0f;                       // [tail] mute only the input...
        const float wetT    = (bypass && !tails) ? 0.0f : wetMix;         // [tail] ...so the tail rings out
        const float dryT    = bypass ? 1.0f : dryMix;                     // [mix] dry to unity in bypass

        if (fFirst) {                    // [every] controls arrive with the first run()
            fFirst = false;
            fSceneCur = sceneT; loadScene();
            fDist = distT; fScat = scatT; fHgt = hgtSmpT; fXf = 1.0f;
            fInG = inT; fWet = wetT; fDry = dryT; fFb = fbT; fCover = coverT; fCombFb = combFbT; fDiff = diffT;
            fELo = eLoT; fEHi = eHiT; fLLo = lLoT; fLHi = lHiT; fRum = rumT;
        }

        // Air and surface losses per tap, from the tap's distance (once per block)
        const Scene& sc = kScenes[fSceneCur];
        for (int i = 0; i < fNTaps; ++i)
            fTapA[i] = lossCoef(sc.taps[i].t * fDist, air, fcSurf);
        const float stepA = lossCoef(sc.stepT0 * fDist, air, fcSurf);
        const float loopA[2] = { lossCoef(sc.wallL * fDist, air, fcSurf), lossCoef(sc.wallR * fDist, air, fcSurf) };
        const float wallG = sc.wallGain * refl;
        const float floorG = sc.floorG * refl;
        const float floorNorm = 1.0f / (1.0f + floorG);   // the floor tap can add up in phase: keep the lap gain under 1
        const float tapRefl = refl;
        const float trim = sc.trim;
        const float maxD = (float)kLineSize - 8.0f;

        // the rumble: two long smeared lines, lengths follow Expanse, decay follows Bounce
        const float rs  = std::sqrt(fDist);
        const float rtR = clampf((5.0f + 12.0f * bnc) * rs, 0.5f, 16.0f);             // seconds: lows outlast the echoes
        const uint32_t rdA = (uint32_t)clampf(0.29f * rs * fSr, 0.03f * fSr, (float)kRumSize - 8.0f) - fApRum[0].len;
        const uint32_t rdB = (uint32_t)clampf(0.43f * rs * fSr, 0.045f * fSr, (float)kRumSize - 8.0f) - fApRum[1].len;
        const float rgA = std::pow(10.0f, -3.0f * (float)(rdA + fApRum[0].len) / (fSr * rtR));
        const float rgB = std::pow(10.0f, -3.0f * (float)(rdB + fApRum[1].len) / (fSr * rtR));
        const bool toneOn = t != 0.0f || std::fabs(fELo - 1.0f) + std::fabs(fEHi - 1.0f) + std::fabs(fLLo - 1.0f)
                                         + std::fabs(fLHi - 1.0f) + fRum > 1e-5f;
        const bool rumOn = toneOn && (rumT > 0.0f || fRum > 1e-5f || fRumE > 1e-9f);
        // with Tone off centre both sides' early echoes feed the rumble, which both sides hear:
        // then compute both, so a one-sided engine stays exactly the full one
        const bool needL = wantL || toneOn, needR = wantR || toneOn;

        // Most of the time Expanse and Surface sit still: then every tap is a whole-sample
        // read worked out once per block (half the cost on the Duo). Interpolate only while gliding.
        const bool still = fXf > 0.999f && sceneT == fSceneCur
                        && std::fabs(distT - fDist) < 1e-5f && std::fabs(scatT - fScat) < 1e-6f;
        const bool diffOn = diffT > 0.0f || fDiff > 1e-4f;   // Surface 10 (bare rock): skip the diffuser
        if (still) {
            fDist = distT; fScat = scatT;
            const float ds = fDist * fSr;
            for (int k = 0; k < fNTaps; ++k) {
                const float d = clampf((fTapT[k] + fTapR[k] * fScat) * ds, 1.0f, maxD);
                fTapDi[k][0] = (uint32_t)(d + fTapItdL[k] + 0.5f);
                fTapDi[k][1] = (uint32_t)(d + fTapItdR[k] + 0.5f);
            }
            for (int k = 0; k < fNSteps; ++k)
                fStepDi[k] = (uint32_t)(clampf((fStepT[k] + fStepR[k] * fScat) * ds, 1.0f, maxD) + 0.5f);
            for (int c = 0; c < 2; ++c) {
                const float d = clampf((c ? fWallR : fWallL) * ds - fApLoopSamples[1 - c], 1.0f, maxD);
                fLoopDi[c][0] = (uint32_t)(d + 0.5f);
                fLoopDi[c][1] = (uint32_t)(clampf(d + fFloorT * ds, 1.0f, maxD) + 0.5f);
            }
        }

        for (uint32_t i = 0; i < frames; ++i) {
            fDist   += fDistSm * (distT - fDist);
            fScat   += fDistSm * (scatT - fScat);
            fHgt    += fHgtSm * (hgtSmpT - fHgt);
            fInG    += fSmooth * (inT - fInG);
            fWet    += fSmooth * (wetT - fWet);
            fDry    += fSmooth * (dryT - fDry);
            fFb     += fSmooth * (fbT - fFb);
            fCover  += fSmooth * (coverT - fCover);
            fCombFb += fSmooth * (combFbT - fCombFb);
            fDiff   += fSmooth * (diffT - fDiff);
            fELo    += fSmooth * (eLoT - fELo);
            fEHi    += fSmooth * (eHiT - fEHi);
            fLLo    += fSmooth * (lLoT - fLLo);
            fLHi    += fSmooth * (lHiT - fLHi);
            fRum    += fSmooth * (rumT - fRum);

            // scene change: duck, swap the geometry at the bottom, fade back in
            if (sceneT != fSceneCur) {
                fXf += fXfDown * (0.0f - fXf);
                if (fXf < 1e-3f) { fXf = 0.0f; fSceneCur = sceneT; loadScene(); }
            } else {
                fXf += fXfUp * (1.0f - fXf);
            }

            const float x = in[i];
            // [buffer] input line, high-passed (the outdoors doesn't return rumble)
            fHpInSt += fHpIn * (x - fHpInSt);
            fIn[fW] = fInG * (x - fHpInSt) + 1e-18f;

            const float dScale = fDist * fSr;
            float wl = 0.0f, wr = 0.0f;

            // other surfaces: taps with their own air loss
            // (the far ear hears each one up to 0.65 ms later: direction, not just level)
            for (int k = 0; k < fNTaps; ++k) {
                const float a = fTapA[k];
                float* st = fTapSt[k];
                const float g = fTapG[k] * tapRefl;
                float d = 0.0f;
                if (!still) d = clampf((fTapT[k] + fTapR[k] * fScat) * dScale, 1.0f, maxD);
                if (needL) {
                    const float vl = still ? fIn[(fW - fTapDi[k][0]) & kLineMask] : readLin(fIn, kLineMask, fW, d + fTapItdL[k]);
                    st[0] += a * (vl - st[0]); st[1] += a * (st[0] - st[1]);   // two poles: distance darkens properly
                    wl += g * fTapPL[k] * st[1];
                }
                if (needR) {
                    const float vr = still ? fIn[(fW - fTapDi[k][1]) & kLineMask] : readLin(fIn, kLineMask, fW, d + fTapItdR[k]);
                    st[2] += a * (vr - st[2]); st[3] += a * (st[2] - st[3]);
                    wr += g * fTapPR[k] * st[3];
                }
            }

            // seating rows: an evenly spaced train, spacing growing row by row
            if (fNSteps > 0) {
                float sl = 0.0f, sr = 0.0f;
                if (still) {
                    for (int k = 0; k < fNSteps; k += 2) {
                        sl += fIn[(fW - fStepDi[k]) & kLineMask] * fStepG[k];
                        sr += fIn[(fW - fStepDi[k + 1]) & kLineMask] * fStepG[k + 1];
                    }
                } else {
                    for (int k = 0; k < fNSteps; ++k) {
                        const float d = clampf((fStepT[k] + fStepR[k] * fScat) * dScale, 1.0f, maxD);
                        const float v = readLin(fIn, kLineMask, fW, d) * fStepG[k];
                        if (k & 1) sr += v; else sl += v;
                    }
                }
                fStepSt[0] += stepA * (sl - fStepSt[0]); fStepSt[2] += stepA * (fStepSt[0] - fStepSt[2]);
                fStepSt[1] += stepA * (sr - fStepSt[1]); fStepSt[3] += stepA * (fStepSt[1] - fStepSt[3]);
                wl += tapRefl * (0.9f * fStepSt[2] + 0.3f * fStepSt[3]);
                wr += tapRefl * (0.3f * fStepSt[2] + 0.9f * fStepSt[3]);
            }

            // Tone on the early reflections: split at ~450 Hz, reweigh the bands; the lows a bright
            // place won't reflect go to the rumble instead of vanishing
            float rumIn = 0.0f;
            if (toneOn) {
                const float lo = fELo - 1.0f, hi = fEHi - 1.0f;
                fEX[0] += fXover * (wl - fEX[0]); fEX[1] += fXover * (wr - fEX[1]);
                if (fELo < 1.0f) rumIn += (1.0f - fELo * fELo) * (fEX[0] + fEX[1]);   // what the place won't throw back
                wl += lo * fEX[0] + hi * (wl - fEX[0]);
                wr += lo * fEX[1] + hi * (wr - fEX[1]);
                fEX[2] += fXover * (wl - fEX[2]); fEX[3] += fXover * (wr - fEX[3]);
                wl += lo * fEX[2] + hi * (wl - fEX[2]);
                wr += lo * fEX[3] + hi * (wr - fEX[3]);
            }

            // the walls: two cross-fed lines, so the echo ping-pongs left-right
            float lapOut[2];
            for (int c = 0; c < 2; ++c) {
                const float* line = c ? fLpR : fLpL;
                float a;
                if (still) {
                    a = line[(fW - fLoopDi[c][0]) & kLineMask] + floorG * line[(fW - fLoopDi[c][1]) & kLineMask];
                } else {
                    const float lap = (c ? fWallR : fWallL) * dScale;
                    const float d = clampf(lap - fApLoopSamples[1 - c], 1.0f, maxD);   // line c is written through the other side's allpasses
                    a = readLin(line, kLineMask, fW, d);
                    a += floorG * readLin(line, kLineMask, fW, clampf(d + fFloorT * dScale, 1.0f, maxD));
                }
                fLoopLp[c] += loopA[c] * (a - fLoopLp[c]);                 // air + surface, per lap (two poles)
                fLoopLp2[c] += loopA[c] * (fLoopLp[c] - fLoopLp2[c]);
                fLoopHp[c] += fHpLoop * (fLoopLp2[c] - fLoopHp[c]);         // drain the lows: no boom
                lapOut[c] = fLoopLp2[c] - fLoopHp[c];
                if (toneOn) {                                              // Tone, every lap (gains <= 1)
                    fLX[c] += fXover * (lapOut[c] - fLX[c]);               // one first-order shelf
                    if (fLLo < 1.0f) rumIn += (1.0f - fLLo) * fLX[c];
                    lapOut[c] += (fLLo - 1.0f) * fLX[c] + (fLHi - 1.0f) * (lapOut[c] - fLX[c]);
                }
            }
            for (int c = 0; c < 2; ++c) {
                // what came back off this wall now crosses to the other one; the fresh
                // sound joins it, so both see the loop's smearing and land on time
                const float v = wallG * fIn[fW] + softClip(fFb * floorNorm * lapOut[c]);
                float* dst = c ? fLpL : fLpR;
                dst[fW] = fApLoop[c][1].process(fApLoop[c][0].process(v, apLoopG), apLoopG) + 1e-18f;
            }
            // each wall heard mostly on its own side; the far ear 0.6 ms later
            fXb[0][fXw] = lapOut[0]; fXb[1][fXw] = lapOut[1];
            const uint32_t xr = (fXw - fItdLoop) & 127u;
            fXw = (fXw + 1) & 127u;
            wl += 0.94f * lapOut[0] + 0.32f * fXb[1][xr];
            wr += 0.32f * fXb[0][xr] + 0.94f * lapOut[1];

            // Surface: below rock, part of each reflection comes back diffuse
            if (diffOn) {
                if (wantL) {
                    float dl = wl;
                    for (int k = 0; k < 4; ++k) dl = fApWet[0][k].process(dl, 0.6f);
                    wl += fDiff * (dl - wl);
                }
                if (wantR) {
                    float dr = wr;
                    for (int k = 0; k < 4; ++k) dr = fApWet[1][k].process(dr, 0.6f);
                    wr += fDiff * (dr - wr);
                }
            }

            // the rumble: lows that wrapped around the place, arriving by many paths (rolling thunder)
            if (rumOn) {
                const float a = fRumA[(fRw - rdA) & kRumMask], b = fRumB[(fRw - rdB) & kRumMask];
                fRumS[0] += fRumLp * (a - fRumS[0]); fRumS[1] += fRumLp * (b - fRumS[1]);
                const float na = 0.70710678f * (fRumS[0] + fRumS[1]), nb = 0.70710678f * (fRumS[0] - fRumS[1]);
                const float rin = 0.5f * fRum * rumIn;
                fRumA[fRw] = fApRum[0].process(rin + softClip(rgA * na), 0.6f) + 1e-18f;
                fRumB[fRw] = fApRum[1].process(0.7f * rin + softClip(rgB * nb), 0.6f) + 1e-18f;
                fRumH[0] += fRumHp * (a - fRumH[0]); fRumH[1] += fRumHp * (b - fRumH[1]);
                wl += kRumOut * (a - fRumH[0]); wr += kRumOut * (b - fRumH[1]);
                fRumE = std::fabs(a) + std::fabs(b);
            }
            fRw = (fRw + 1) & kRumMask;

            // Canopy: sky to floor and back, a filtered feedback comb
            if (fCover > 1e-4f) {
                const float hd[2] = { clampf(fHgt, 1.0f, (float)kCombSize - 4.0f), clampf(fHgt * 1.031f, 1.0f, (float)kCombSize - 4.0f) };
                float* wv[2] = { &wl, &wr };
                for (int c = 0; c < 2; ++c) {
                    if (!(c ? wantR : wantL)) continue;
                    const float z = readLin(fComb[c], kCombMask, fCw, hd[c]);
                    fCombSt[c] += combLp * (z - fCombSt[c]);
                    fComb[c][fCw] = kin * *wv[c] + fCombFb * fCombSt[c] + 1e-18f;
                    *wv[c] += fCover * (fCombSt[c] - *wv[c] * 0.3f);
                }
            } else {
                fComb[0][fCw] = fComb[1][fCw] = 0.0f;
                fCombSt[0] *= 0.99f; fCombSt[1] *= 0.99f;
            }
            fCw = (fCw + 1) & kCombMask;
            fW = (fW + 1) & kLineMask;

            // [feedback] limit the wet only; the dry passes untouched
            const float g = fWet * fXf * trim;
            if (wantL) outL[i] = fDry * x + softKnee(g * wl, 0.8f, 0.15f);
            if (wantR) outR[i] = fDry * x + softKnee(g * wr, 0.8f, 0.15f);
        }

        // [tail] Tails off: once the wet has faded, clear the lines in slices
        if (bypass && !tails && fWet < 1e-4f) {
            if (!fCleared) clearSlice();
        } else {
            fCleared = false; fClearPos = 0;
        }
        // land smoothers on their targets (float stalls just short of them)
        if (std::fabs(fDry - dryT) < 1e-4f) fDry = dryT;
        if (std::fabs(fWet - wetT) < 1e-4f) fWet = wetT;
        if (std::fabs(fInG - inT) < 1e-4f) fInG = inT;
    }

private:
    // one-pole coefficient for the losses on a path of t seconds:
    // air takes the top off with distance, the surface with every reflection
    inline float lossCoef(float t, float air, float fcSurf) const
    {
        const float fcAir = 20000.0f / (1.0f + 1.2f * air * t);
        const float fc = 1.0f / (1.0f / fcAir + 1.0f / fcSurf);
        return onePoleCoef(clampf(fc, 200.0f, 0.45f * fSr), fSr);
    }

    void loadScene()
    {
        const Scene& sc = kScenes[fSceneCur];
        fNTaps = sc.nTaps;
        for (int k = 0; k < fNTaps; ++k) {
            fTapT[k] = sc.taps[k].t;
            fTapR[k] = sc.taps[k].t * kRnd[k];
            fTapG[k] = sc.taps[k].g;
            const float th = (sc.taps[k].pan + 1.0f) * 0.25f * 3.14159265f;   // constant power
            fTapPL[k] = std::cos(th); fTapPR[k] = std::sin(th);
            const float itd = 0.00065f * fSr * sc.taps[k].pan;               // interaural delay, samples
            fTapItdL[k] = itd > 0.0f ? itd : 0.0f;                           // panned right: the left ear is late
            fTapItdR[k] = itd < 0.0f ? -itd : 0.0f;
        }
        fNSteps = sc.nSteps;
        float g = sc.stepG;
        for (int k = 0; k < fNSteps; ++k) {
            fStepT[k] = sc.stepT0 + sc.stepDt * (float)k * (1.0f + sc.stepStretch * (float)k);
            fStepR[k] = 0.15f * sc.stepDt * kRnd[kMaxTaps + k];   // rough-cut rows: a little scatter
            fStepG[k] = g; g *= sc.stepDecay;
        }
        fWallL = sc.wallL; fWallR = sc.wallR;
        fFloorT = sc.floorT;
    }

    void clearSmall()
    {
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < 4; ++k) fApWet[c][k].clear();
            for (int k = 0; k < 2; ++k) fApLoop[c][k].clear();
            fApRum[c].clear();
            fLoopLp[c] = fLoopLp2[c] = fLoopHp[c] = fCombSt[c] = 0.0f;
            fRumS[c] = fRumH[c] = 0.0f;
        }
        std::memset(fEX, 0, sizeof(fEX)); std::memset(fLX, 0, sizeof(fLX));
        std::memset(fStepSt, 0, sizeof(fStepSt));
        std::memset(fTapSt, 0, sizeof(fTapSt));
        fRumE = 0.0f;
    }

    void clearAll()
    {
        std::memset(fIn, 0, sizeof(float) * kLineSize);
        std::memset(fLpL, 0, sizeof(float) * kLineSize);
        std::memset(fLpR, 0, sizeof(float) * kLineSize);
        std::memset(fComb, 0, sizeof(fComb));
        std::memset(fXb, 0, sizeof(fXb));
        clearSmall();
        std::memset(fRumA, 0, sizeof(float) * kRumSize);
        std::memset(fRumB, 0, sizeof(float) * kRumSize);
        fHpInSt = 0.0f;
        fW = 0; fCw = 0; fRw = 0;
        fCleared = true; fClearPos = 0;
    }

    // [tail] a 12 MB memset in one block would blow the deadline: 64 k samples per line per block
    void clearSlice()
    {
        const uint32_t n = 1u << 16;
        std::memset(fIn + fClearPos, 0, sizeof(float) * n);
        std::memset(fLpL + fClearPos, 0, sizeof(float) * n);
        std::memset(fLpR + fClearPos, 0, sizeof(float) * n);
        fClearPos += n;
        if (fClearPos >= kLineSize) {
            std::memset(fComb, 0, sizeof(fComb));
            std::memset(fXb, 0, sizeof(fXb));
            clearSmall();
            std::memset(fRumA, 0, sizeof(float) * kRumSize);   // 1 MB, the last slice
            std::memset(fRumB, 0, sizeof(float) * kRumSize);
            fCleared = true; fClearPos = 0;
        }
    }

    float* fIn;
    float* fLpL;
    float* fLpR;
    float* fRumA;
    float* fRumB;
    Allpass fApRum[2];
    uint32_t fRw = 0;
    float fEX[4], fLX[4], fRumS[2], fRumH[2], fRumE = 0.0f;
    float fXover = 0.05f, fRumLp = 0.03f, fRumHp = 0.003f;
    float fELo = 1.0f, fEHi = 1.0f, fLLo = 1.0f, fLHi = 1.0f, fRum = 0.0f;
    float fComb[2][kCombSize];
    Allpass fApWet[2][4];
    Allpass fApLoop[2][2];
    float fApLoopSamples[2] = { 0.0f, 0.0f };
    float fXb[2][128];
    uint32_t fW = 0, fCw = 0, fClearPos = 0, fXw = 0, fItdLoop = 1;
    bool fCleared = true;

    // the current scene, unpacked
    int   fSceneCur = 0, fNTaps = 0, fNSteps = 0;
    float fTapT[kMaxTaps], fTapR[kMaxTaps], fTapG[kMaxTaps], fTapPL[kMaxTaps], fTapPR[kMaxTaps], fTapA[kMaxTaps], fTapSt[kMaxTaps][4], fTapItdL[kMaxTaps], fTapItdR[kMaxTaps];
    uint32_t fTapDi[kMaxTaps][2], fStepDi[kMaxSteps], fLoopDi[2][2];
    float fStepT[kMaxSteps], fStepR[kMaxSteps], fStepG[kMaxSteps], fStepSt[4];
    float fWallL = 1.0f, fWallR = 1.0f, fFloorT = 0.0f;
    float fLoopLp[2], fLoopLp2[2], fLoopHp[2], fCombSt[2], fHpInSt = 0.0f;

    float fSr = 48000.0f, fSmooth = 0.0f, fDistSm = 0.0f, fHgtSm = 0.0f, fXfDown = 0.0f, fXfUp = 0.0f, fHpIn = 0.0f, fHpLoop = 0.0f;
    float fDist = 1.0f, fScat = 0.0f, fHgt = 100.0f, fXf = 1.0f;
    float fInG = 1.0f, fWet = 0.7f, fDry = 1.0f, fFb = 0.0f, fCover = 0.0f, fCombFb = 0.0f, fDiff = 0.0f;
    bool  fFirst = true;
};

static const int kEngines = DISTRHO_PLUGIN_NUM_INPUTS;   // 1 = mono, 2 = stereo
static const uint32_t kScratch = 4096;                   // the right input, if the host shares its buffer

class OuraverbPlugin : public Plugin
{
public:
    OuraverbPlugin()
        : Plugin(kParameterCount, 0, 0)
    {
        fParams[kPlace] = 0.0f;
        fParams[kExpanse] = 100.0f;
        fParams[kSurface] = 8.0f;
        fParams[kAir] = 5.0f;
        fParams[kBounce] = 4.0f;
        fParams[kCover] = 0.0f;
        fParams[kCoverDensity] = 3.0f;
        fParams[kCoverHeight] = 20.0f;
        fParams[kMix] = 35.0f;
        fParams[kTone] = 5.0f;
        fParams[kTails] = 1.0f;
        fParams[kInputLevel] = 0.0f;
        fParams[kBypass] = 0.0f;
    }

protected:
    const char* getLabel()       const override { return OURAVERB_LABEL; }
    const char* getDescription() const override { return OURAVERB_DESCRIPTION; }
    const char* getMaker()       const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getLicense()     const override { return "GPL-3.0-or-later"; }
    uint32_t    getVersion()     const override { return d_version(1, 0, 11); }   // = lv2:minorVersion 2, microVersion 10
    int64_t     getUniqueId()    const override { return OURAVERB_UNIQUE_ID; }

    void initParameter(uint32_t index, Parameter& p) override
    {
        p.hints = kParameterIsAutomatable;
        switch (index) {
        case kPlace: {
            p.hints |= kParameterIsInteger;
            p.name = "Place"; p.symbol = "place";
            p.ranges.min = 0.0f; p.ranges.max = (float)(kNumScenes - 1); p.ranges.def = 0.0f;
            ParameterEnumerationValue* const v = new ParameterEnumerationValue[kNumScenes];
            for (int i = 0; i < kNumScenes; ++i) { v[i].label = kScenes[i].name; v[i].value = (float)i; }
            p.enumValues.count = kNumScenes;
            p.enumValues.restrictedMode = true;
            p.enumValues.values = v;
            break;
        }
        case kExpanse:
            p.hints |= kParameterIsLogarithmic;
            p.name = "Expanse"; p.symbol = "expanse"; p.unit = "%";
            p.ranges.min = 2.0f; p.ranges.max = 200.0f; p.ranges.def = 100.0f;
            break;
        case kSurface:
            p.name = "Surface"; p.symbol = "surface";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 8.0f;
            break;
        case kAir:
            p.name = "Air"; p.symbol = "air";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kBounce:
            p.name = "Bounce"; p.symbol = "bounce";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 4.0f;
            break;
        case kCover:
            p.name = "Cover"; p.symbol = "cover"; p.unit = "%";
            p.ranges.min = 0.0f; p.ranges.max = 100.0f; p.ranges.def = 0.0f;
            break;
        case kCoverDensity:
            p.name = "Cover density"; p.symbol = "cover_density";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 3.0f;
            break;
        case kCoverHeight:
            p.hints |= kParameterIsLogarithmic;
            p.name = "Cover height"; p.symbol = "cover_height"; p.unit = "m";
            p.ranges.min = 1.0f; p.ranges.max = 60.0f; p.ranges.def = 20.0f;
            break;
        case kMix:
            p.name = "Mix"; p.symbol = "mix"; p.unit = "%";
            p.ranges.min = 0.0f; p.ranges.max = 100.0f; p.ranges.def = 35.0f;
            break;
        case kTone:
            p.name = "Tone"; p.symbol = "tone";
            p.ranges.min = 0.0f; p.ranges.max = 10.0f; p.ranges.def = 5.0f;
            break;
        case kTails:                                          // [tail]
            p.hints |= kParameterIsInteger | kParameterIsBoolean;
            p.name = "Tails"; p.symbol = "tails";
            p.ranges.min = 0.0f; p.ranges.max = 1.0f; p.ranges.def = 1.0f;
            break;
        case kInputLevel:                                     // for the face's orbit ripples
            p.hints = kParameterIsOutput;
            p.name = "Input Level"; p.symbol = "input_level";
            p.ranges.min = 0.0f; p.ranges.max = 1.0f; p.ranges.def = 0.0f;
            break;
        case kBypass:                                         // [every]
            p.initDesignation(kParameterDesignationBypass);
            break;
        }
        switch (index) {
        case kPlace: case kExpanse: case kSurface:                 p.groupId = kGroupLocation; break;
        case kCoverHeight: case kAir: case kBounce: case kTone:   p.groupId = kGroupDistance; break;
        case kCover: case kCoverDensity:                           p.groupId = kGroupCover; break;
        case kMix: case kTails:                                    p.groupId = kGroupMix; break;
        default: break;
        }
    }

    void initPortGroup(uint32_t groupId, PortGroup& g) override
    {
        switch (groupId) {
        case kGroupLocation: g.name = "Location"; g.symbol = "location"; break;
        case kGroupDistance: g.name = "Distance"; g.symbol = "distance"; break;
        case kGroupCover:    g.name = "Cover";    g.symbol = "cover";    break;
        case kGroupMix:      g.name = "Mix";      g.symbol = "mix";      break;
        }
    }

    float getParameterValue(uint32_t i) const override { return i < kParameterCount ? fParams[i] : 0.0f; }
    void  setParameterValue(uint32_t i, float v) override { if (i < kParameterCount) fParams[i] = v; }

    void activate() override
    {
        const float sr = (float)getSampleRate();
        for (int e = 0; e < kEngines; ++e) fEng[e].activate(sr);
        fLevelRel = std::exp(-1.0f / (0.180f * sr));
        fLevel = 0.0f;
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
#if DISTRHO_PLUGIN_NUM_INPUTS == 2
        // the left input's engine makes the left output, the right input's the right. If the
        // host hands the right input in the left output's buffer, keep a copy before it's written
        for (uint32_t off = 0; off < frames; off += kScratch) {
            const uint32_t n = frames - off < kScratch ? frames - off : kScratch;
            const float* inR = inputs[1] + off;
            if (inputs[1] == outputs[0]) { std::memcpy(fScratch, inR, sizeof(float) * n); inR = fScratch; }
            fEng[0].process<true, false>(inputs[0] + off, outputs[0] + off, nullptr, n, fParams);
            fEng[1].process<false, true>(inR, nullptr, outputs[1] + off, n, fParams);
        }
#else
        fEng[0].process<true, true>(inputs[0], outputs[0], outputs[1], frames, fParams);
#endif
        // input level out (for the face), always on, whatever bypass: Trance Atlantic's follower.
        // Instant rise, ~180 ms fall (long enough for MOD's few-times-a-second updates to catch
        // each pluck), -48 dBFS -> 0 .. -12 dBFS -> 1, then ^1.5 so attacks read strong.
        // Stereo: the louder input. (Read after processing: fine, the outputs were written
        // from the inputs sample by sample, so an in-place buffer only matters for the level.)
        for (uint32_t i = 0; i < frames; ++i) {
            float a = std::fabs(inputs[0][i]);
            if (kEngines == 2) a = std::fmax(a, std::fabs(inputs[kEngines - 1][i]));
            fLevel = a > fLevel ? a : fLevel * fLevelRel;
        }
        fLevel += 1e-18f;
        const float dB = 20.0f * std::log10(fLevel + 1e-9f);
        const float lv = clampf((dB + 48.0f) / 36.0f, 0.0f, 1.0f);
        fParams[kInputLevel] = lv * std::sqrt(lv);
    }

private:
    float fParams[kParameterCount];
    Engine fEng[kEngines];
    float fScratch[kScratch];
    float fLevel = 0.0f, fLevelRel = 0.9999f;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OuraverbPlugin)
};

Plugin* createPlugin() { return new OuraverbPlugin(); }

END_NAMESPACE_DISTRHO

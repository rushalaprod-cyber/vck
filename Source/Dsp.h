#pragma once
// vck : moteur audio
//  - Vocoder a banc de filtres de Butterworth (jusqu'a 64 bandes), traite en tableaux
//    (vectorise), porteuse blanchie avec sortie bornee echantillon par echantillon
//    (aucun pic possible), changement du nombre de bandes en fondu enchaine
//  - Porteuse : synthe polyphonique anti-aliase (PolyBLEP), sub, unisson, glide, vibrato
//  - AUTO : suivi de hauteur YIN -> notes quantifiees + harmonies
//  - Doubleur 4 voix a decoupe synchrone (pas de filtrage en peigne), reglages par voix
//  - Effets : saturation anti-repliement, EQ, compresseur, ensemble, phaser, delai, reverbe
//  - Limiteur de sortie avec anticipation
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <complex>
#include <algorithm>

namespace vck
{
constexpr float pi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.f * std::log10 (std::max (g, 1.0e-9f)); }
inline float mtof (float m)      { return 440.f * std::exp2 ((m - 69.f) / 12.f); }
inline float ftom (float f)      { return 69.f + 12.f * std::log2 (std::max (f, 1.f) / 440.f); }

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 1u) : s (seed * 2654435761u + 1u) {}
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.f / 8388608.f) - 1.f; }
    inline float uni()  { return 0.5f * (next() + 1.f); }
};

//==============================================================================
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float fc, float Q, float fs)
    {
        g = std::tan (pi * std::clamp (fc, 5.f, 0.47f * fs) / fs);
        k = 1.f / Q;
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }
    inline void tick (float v0, float& v1, float& v2)
    {
        const float v3 = v0 - ic2;
        v1 = a1 * ic1 + a2 * v3;
        v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
    }
    inline float bp (float x) { float v1, v2; tick (x, v1, v2); return k * v1; }
    inline float lp (float x) { float v1, v2; tick (x, v1, v2); return v2; }
    inline float hp (float x) { float v1, v2; tick (x, v1, v2); return x - k * v1 - v2; }
};

struct OnePole
{
    float a = 0, y = 0;
    void setMs (float ms, float fs) { a = ms <= 0.f ? 0.f : std::exp (-1.f / (ms * 0.001f * fs)); }
    void setHz (float hz, float fs) { a = std::exp (-2.f * pi * hz / fs); }
    inline float process (float x) { y = x + a * (y - x); return y; }
};

struct EnvFollower
{
    float y1 = 0, y2 = 0, a = 0, r = 0;
    void set (float attMs, float relMs, float fs)
    {
        a = std::exp (-1.f / (std::max (0.05f, attMs) * 0.001f * fs));
        r = std::exp (-1.f / (std::max (0.5f, relMs) * 0.001f * fs));
    }
    void reset() { y1 = y2 = 0; }
    inline float process (float x)
    {
        x = std::abs (x);
        y1 = x + (x > y1 ? a : r) * (y1 - x);
        y2 = y1 + (y1 > y2 ? a : r) * (y2 - y1);
        return y2;
    }
};

struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); }
    int size() const { return mask + 1; }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float d) const
    {
        const float pos = (float) w - 1.f - std::max (1.f, d);
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)],
                    x1 = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - 1 - d) & mask)]; }
};

// Petit lisseur de parametre (evite le "zipper" quand on tourne un bouton)
struct Smooth
{
    float v = 0, t = 0, a = 0.999f;
    void init (float x, float ms, float fs) { v = t = x; a = std::exp (-1.f / (ms * 0.001f * fs)); }
    inline float next() { v = t + a * (v - t); return v; }
};

//==============================================================================
// Detecteur de hauteur YIN (signal decime a ~12 kHz)
class PitchTracker
{
public:
    void prepare (double fs)
    {
        decim = std::max (1, (int) std::round (fs / 12000.0));
        fsd = (float) fs / (float) decim;
        aa1.set (0.38f * fsd, 0.54f, (float) fs);
        aa2.set (0.38f * fsd, 1.31f, (float) fs);
        ring.assign (RING, 0.f);
        frame.assign ((size_t) W + 1100, 0.f);
        d.assign (1100, 0.f);
        setRange (60.f, 1100.f);
        reset();
    }
    void setRange (float fmin, float fmax)
    {
        tauMax = std::clamp ((int) (fsd / std::max (40.f, fmin)), 8, (int) d.size() - 2);
        tauMin = std::max (2, (int) (fsd / std::max (fmin * 2.f, fmax)));
    }
    void setThreshold (float t) { thr = t; }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.f);
        aa1.reset(); aa2.reset(); wpos = 0; cnt = 0; hopCnt = 0;
        freq = 0.f; voiced = false; hist[0] = hist[1] = hist[2] = 0.f;
    }
    float hopSeconds() const { return (float) (HOP * decim) / (fsd * (float) decim); }
    inline bool push (float x)
    {
        x = aa2.lp (aa1.lp (x));
        if (++cnt < decim) return false;
        cnt = 0;
        ring[(size_t) wpos] = x; wpos = (wpos + 1) & (RING - 1);
        if (++hopCnt < HOP) return false;
        hopCnt = 0;
        analyse();
        return true;
    }
    float freq = 0.f, confidence = 0.f, level = 0.f;
    bool voiced = false;

private:
    static constexpr int RING = 4096, W = 400, HOP = 96;
    void analyse()
    {
        const int n = W + tauMax;
        float energy = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float v = ring[(size_t) ((wpos - n + i) & (RING - 1))];
            frame[(size_t) i] = v; if (i < W) energy += v * v;
        }
        level = std::sqrt (energy / (float) W);
        if (level < 0.0008f) { setResult (0.f, 0.f); return; }
        d[0] = 1.f;
        float run = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.f;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
            run += s;
            d[(size_t) tau] = run > 0.f ? s * (float) tau / run : 1.f;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < thr)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            float m = 1.f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < m) { m = d[(size_t) tau]; best = tau; }
            if (m > thr * 2.2f || best < 0) { setResult (0.f, 0.f); return; }
        }
        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = d[(size_t) best - 1], s1 = d[(size_t) best], s2 = d[(size_t) best + 1];
            const float den = s0 + s2 - 2.f * s1;
            if (std::abs (den) > 1e-9f) t += 0.5f * (s0 - s2) / den;
        }
        setResult (fsd / t, 1.f - d[(size_t) best]);
    }
    void setResult (float f, float conf)
    {
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f;
        if (f <= 0.f) { voiced = false; confidence = 0.f; return; }
        float a = hist[0] > 0 ? hist[0] : f, b = hist[1] > 0 ? hist[1] : f, c = f;
        freq = std::max (std::min (a, b), std::min (std::max (a, b), c));
        voiced = true; confidence = conf;
    }
    Svf aa1, aa2;
    std::vector<float> ring, frame, d;
    float hist[3] {};
    int decim = 4, wpos = 0, cnt = 0, hopCnt = 0, tauMax = 200, tauMin = 11;
    float fsd = 12000.f, thr = 0.14f;
};

//==============================================================================
// Synthe porteur
struct SynthParams
{
    int wave = 0;            // 0 SAW, 1 SQUARE, 2 PULSE, 3 SUPER, 4 BUZZ, 5 NOISE
    int unison = 1;
    float detune = 0.2f, octave = 0.f, glideMs = 0.f;
    int mode = 0;            // 0 POLY, 1 MONO, 2 LEGATO
    float vibrato = 0.f, vibRate = 5.6f;
    float attackMs = 5.f, releaseMs = 120.f;
    float noise = 0.f, sub = 0.f, velSens = 0.7f, bend = 0.f;
    float width = 0.6f, harmLevel = 0.8f, harmWidth = 0.6f;
};

class CarrierSynth
{
public:
    static constexpr int maxVoices = 16, maxUnison = 7;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (int i = 0; i < maxVoices; ++i) { voices[(size_t) i] = Voice(); voices[(size_t) i].seed (i); }
        stack.clear(); stack.reserve (128);
        vibPhase = 0;
    }
    void setParams (const SynthParams& p) { prm = p; }
    void allNotesOff() { for (auto& v : voices) v.gate = false; stack.clear(); }
    bool isAuto() const { return autoMode; }

    void noteOn (int note, float vel)
    {
        if (autoMode) return;
        vel = 1.f - prm.velSens + prm.velSens * vel;
        if (prm.mode != 0)
        {
            removeFromStack (note);
            stack.push_back (note);
            auto& v = voices[0];
            const bool held = v.gate && v.note >= 0;
            const bool slide = prm.mode == 1 ? (v.note >= 0) : held;
            v.start (note, vel, slide && prm.glideMs > 0.f, ++counter, prm.mode == 2 && held, prm.octave);
            return;
        }
        int idx = -1;
        for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].note == note && voices[(size_t) i].active()) { idx = i; break; }
        if (idx < 0) for (int i = 0; i < maxVoices; ++i) if (! voices[(size_t) i].active()) { idx = i; break; }
        if (idx < 0)
        {
            float best = 2.f; uint32_t oldest = UINT32_MAX;
            for (int i = 0; i < maxVoices; ++i) if (! voices[(size_t) i].gate && voices[(size_t) i].env < best) { best = voices[(size_t) i].env; idx = i; }
            if (idx < 0) for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].order < oldest) { oldest = voices[(size_t) i].order; idx = i; }
        }
        auto& v = voices[(size_t) idx];
        const bool slide = prm.glideMs > 0.f && lastNote >= 0;
        if (slide) v.logF = std::log (mtof ((float) lastNote + prm.octave));
        v.start (note, vel, slide, ++counter, false, prm.octave);
        lastNote = note;
    }

    void noteOff (int note)
    {
        if (autoMode) return;
        if (prm.mode != 0)
        {
            removeFromStack (note);
            auto& v = voices[0];
            if (v.note != note) return;
            if (! stack.empty()) v.start (stack.back(), v.vel, prm.glideMs > 0.f, ++counter, true, prm.octave);
            else v.gate = false;
            return;
        }
        for (auto& v : voices) if (v.note == note && v.gate) v.gate = false;
    }

    void setAuto (bool on)
    {
        if (on == autoMode) return;
        autoMode = on;
        for (auto& v : voices) v.gate = false;
        stack.clear();
    }
    void setAutoTargets (const float* notes, int count, bool gate)
    {
        for (int i = 0; i < 5; ++i)
        {
            auto& v = voices[(size_t) i];
            if (i < count && gate)
            {
                if (! v.gate)
                {
                    v.order = ++counter;
                    if (v.env < 0.02f) v.logF = std::log (mtof (notes[i] + prm.octave));
                }
                v.gate = true;
                v.vel = i == 0 ? 1.f : prm.harmLevel;
                static constexpr float harmPan[5] = { 0.f, -1.f, 1.f, -0.55f, 0.55f };
                v.pan = harmPan[i] * prm.harmWidth;
                v.note = std::clamp ((int) std::round (notes[i]), 0, 127);
                v.targetNote = notes[i];
            }
            else v.gate = false;
        }
    }
    float autoGlideMs = 30.f;

    float level() const { return levelNow; }
    void activeNotes (std::array<bool, 128>& out) const
    {
        out.fill (false);
        for (auto& v : voices) if (v.gate && v.note >= 0 && v.note < 128) out[(size_t) v.note] = true;
    }
    bool anyGate() const { for (auto& v : voices) if (v.gate) return true; return false; }

    void control (int samples)
    {
        const float dtc = (float) samples / fs;
        vibPhase += dtc * prm.vibRate; if (vibPhase > 1.f) vibPhase -= 1.f;
        const float vib = std::sin (2.f * pi * vibPhase);
        const int u = std::clamp (prm.wave == 3 ? std::max (prm.unison, 5) : prm.unison, 1, maxUnison);
        const float spreadSemi = (prm.wave == 3 ? 0.35f + 0.5f * prm.detune : 0.5f * prm.detune);
        const float glide = autoMode ? autoGlideMs : prm.glideMs;
        const float gc = glide <= 0.5f ? 1.f : 1.f - std::exp (-dtc / (glide * 0.001f));
        const float aInc = dtc / std::max (0.001f, prm.attackMs * 0.001f);
        const float rMul = std::exp (-dtc / std::max (0.003f, prm.releaseMs * 0.001f * 0.35f));
        const float lnSemi = std::log (2.f) / 12.f;
        float lvl = 0.f;
        for (auto& v : voices)
        {
            if (! v.active()) continue;
            const float target = std::log (mtof ((autoMode ? v.targetNote : (float) v.note) + prm.octave));
            v.logF += (target - v.logF) * gc;
            if (v.gate) v.envTarget = std::min (1.f, v.envTarget + aInc);
            else v.envTarget *= rMul;
            v.envStep = (v.envTarget - v.env) / (float) samples;
            v.vibAge = v.gate ? std::min (1.f, v.vibAge + dtc / 0.45f) : v.vibAge;
            const float vibSemi = prm.vibrato * 0.55f * v.vibAge * vib + (autoMode ? 0.f : prm.bend);
            for (int k = 0; k < u; ++k)
            {
                const float off = u == 1 ? 0.f : spread (k, u) * spreadSemi + 0.03f * v.rnd[(size_t) k];
                const float f = std::exp (v.logF + (vibSemi + off) * lnSemi);
                v.dt[(size_t) k] = std::min (0.45f, f / fs);
            }
            v.subDt = std::min (0.45f, std::exp (v.logF + vibSemi * lnSemi) * 0.5f / fs);
            v.u = u;
            {
                const float a = (std::clamp (v.pan, -1.f, 1.f) + 1.f) * 0.25f * pi;
                v.gl = std::cos (a) * 1.41421356f; v.gr = std::sin (a) * 1.41421356f;
            }
            lvl += v.envTarget * v.vel;
            if (! v.gate && v.envTarget < 1.0e-4f && v.env < 1.0e-4f) { v.note = -1; v.env = v.envTarget = 0.f; }
        }
        levelNow = std::min (1.f, lvl);
    }

    inline void render (float& L, float& R)
    {
        L = R = 0.f;
        const float pw = prm.wave == 2 ? 0.25f : prm.wave == 4 ? 0.07f : 0.5f;
        for (auto& v : voices)
        {
            if (v.note < 0) continue;
            v.env += v.envStep;
            const float amp = v.env * v.vel;
            if (amp <= 0.f) continue;
            if (prm.wave == 5)
            {
                L += noise.next() * amp * 0.35f * v.gl;
                R += noise.next() * amp * 0.35f * v.gr;
                continue;
            }
            const float norm = 0.32f / std::sqrt ((float) v.u);
            for (int k = 0; k < v.u; ++k)
            {
                float& ph = v.ph[(size_t) k];
                const float dt = v.dt[(size_t) k];
                float s;
                if (prm.wave == 1 || prm.wave == 2 || prm.wave == 4)
                {
                    s = (ph < pw ? 1.f : -1.f) + blep (ph, dt) - blep (frac (ph + 1.f - pw), dt);
                    s -= (2.f * pw - 1.f);
                    if (prm.wave == 4) s *= 1.6f;
                }
                else s = 2.f * ph - 1.f - blep (ph, dt);
                ph += dt; if (ph >= 1.f) ph -= 1.f;
                const float p = v.u == 1 ? 0.f : spread (k, v.u) * prm.width;
                L += s * amp * norm * (1.f - p) * v.gl;
                R += s * amp * norm * (1.f + p) * v.gr;
            }
            if (prm.sub > 0.f)
            {
                float s = (v.subPh < 0.5f ? 1.f : -1.f) + blep (v.subPh, v.subDt) - blep (frac (v.subPh + 0.5f), v.subDt);
                v.subPh += v.subDt; if (v.subPh >= 1.f) v.subPh -= 1.f;
                s *= amp * 0.3f * prm.sub;
                L += s * v.gl; R += s * v.gr;
            }
        }
        if (prm.noise > 0.f)
        {
            const float n = noise.next() * 0.25f * prm.noise * levelNow;
            L += n; R += n;
        }
    }

private:
    static inline float frac (float x) { return x - std::floor (x); }
    static inline float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    static inline float spread (int k, int u) { return u <= 1 ? 0.f : -1.f + 2.f * (float) k / (float) (u - 1); }

    struct Voice
    {
        int note = -1; bool gate = false; float vel = 0.f, env = 0.f, envTarget = 0.f, envStep = 0.f;
        float logF = std::log (440.f), targetNote = 69.f, vibAge = 0.f, subPh = 0.f, subDt = 0.f, pan = 0.f, gl = 1.f, gr = 1.f;
        uint32_t order = 0; int u = 1;
        std::array<float, maxUnison> ph {}, dt {}, rnd {};
        bool active() const { return note >= 0; }
        void seed (int i)
        {
            Rng r ((uint32_t) (i * 7 + 3));
            for (int k = 0; k < maxUnison; ++k) { ph[(size_t) k] = r.uni(); rnd[(size_t) k] = r.next(); }
        }
        void start (int n, float v, bool slide, uint32_t ord, bool legato, float octave)
        {
            if (! slide) logF = std::log (mtof ((float) n + octave));
            note = n; targetNote = (float) n; vel = v; gate = true; order = ord; pan = 0.f;
            if (! legato) vibAge = 0.f;
        }
    };
    void removeFromStack (int n) { stack.erase (std::remove (stack.begin(), stack.end(), n), stack.end()); }

    std::array<Voice, maxVoices> voices;
    std::vector<int> stack;
    SynthParams prm;
    Rng noise { 77 };
    float fs = 48000.f, vibPhase = 0.f, levelNow = 0.f;
    uint32_t counter = 0;
    int lastNote = -1;
    bool autoMode = false;
};

//==============================================================================
// Banc de filtres en tableaux : chaque etage traite toutes les bandes d'un coup
constexpr int MB = 64;

struct BankStage
{
    alignas (32) float a1[MB] {}, a2[MB] {}, a3[MB] {}, k[MB] {}, s1[MB] {}, s2[MB] {};
    void set (int i, float fc, float Q, float fs)
    {
        const float g = std::tan (pi * std::clamp (fc, 5.f, 0.47f * fs) / fs);
        k[i] = 1.f / Q;
        a1[i] = 1.f / (1.f + g * (g + k[i])); a2[i] = g * a1[i]; a3[i] = g * a2[i];
    }
    void reset() { std::fill (std::begin (s1), std::end (s1), 0.f); std::fill (std::begin (s2), std::end (s2), 0.f); }
    inline void runScalar (float x, float* out, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v3 = x - s2[i];
            const float v1 = a1[i] * s1[i] + a2[i] * v3;
            const float v2 = s2[i] + a2[i] * s1[i] + a3[i] * v3;
            s1[i] = 2.f * v1 - s1[i]; s2[i] = 2.f * v2 - s2[i];
            out[i] = k[i] * v1;
        }
    }
    inline void run (float* io, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v3 = io[i] - s2[i];
            const float v1 = a1[i] * s1[i] + a2[i] * v3;
            const float v2 = s2[i] + a2[i] * s1[i] + a3[i] * v3;
            s1[i] = 2.f * v1 - s1[i]; s2[i] = 2.f * v2 - s2[i];
            io[i] = k[i] * v1;
        }
    }
    // dernier etage : sortie + composante en quadrature (pour l'enveloppe instantanee)
    inline void runQuad (float* io, float* quad, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v3 = io[i] - s2[i];
            const float v1 = a1[i] * s1[i] + a2[i] * v3;
            const float v2 = s2[i] + a2[i] * s1[i] + a3[i] * v3;
            s1[i] = 2.f * v1 - s1[i]; s2[i] = 2.f * v2 - s2[i];
            io[i] = k[i] * v1; quad[i] = k[i] * v2;
        }
    }
};

// Conception exacte d'un passe-bande de Butterworth d'ordre 2*M en M cellules SVF
inline float butterworthBandpass (float centre, float bwOct, int M, float fs, float* fcs, float* qs)
{
    using C = std::complex<double>;
    constexpr double dpi = 3.141592653589793;
    const double fl = centre * std::exp2 (-0.5 * bwOct), fh = std::min (0.46 * fs, centre * std::exp2 (0.5 * bwOct));
    const double wl = std::tan (dpi * fl / fs), wh = std::tan (dpi * fh / fs);
    const double w0 = std::sqrt (wl * wh), B = std::max (1e-6, wh - wl);
    int s = 0;
    C total (1.0, 0.0);
    auto add = [&] (C pole)
    {
        if (pole.imag() < 0) pole = std::conj (pole);
        const double wi = std::abs (pole), qi = wi / std::max (1e-9, -2.0 * pole.real());
        fcs[s] = (float) (std::atan (wi) * fs / dpi);
        qs[s] = (float) std::clamp (qi, 0.3, 200.0);
        const C jw (0.0, w0);
        total *= (jw * wi / qi) / (jw * jw + jw * wi / qi + wi * wi);
        ++s;
    };
    for (int k = 0; k < M; ++k)
    {
        const C p = std::polar (1.0, dpi * (2.0 * k + M + 1.0) / (2.0 * M));
        const bool real = std::abs (p.imag()) < 1e-6;
        if (! real && p.imag() < 0.0) continue;
        const C pb = C (real ? -1.0 : p.real(), real ? 0.0 : p.imag()) * B;
        const C d = std::sqrt (pb * pb - 4.0 * w0 * w0);
        if (real) add ((pb + d) * 0.5);
        else { add ((pb + d) * 0.5); add ((pb - d) * 0.5); }
    }
    return (float) (1.0 / std::max (1e-12, std::abs (total)));
}

struct Bank
{
    int n = 0;
    BankStage m[4], cl[3], cr[3];
    alignas (32) float gA[MB] {}, gS[MB] {}, e1[MB] {}, e2[MB] {}, ea[MB] {}, er[MB] {}, modEnv[MB] {},
                       ce[MB] {}, ceA[MB] {}, ceR[MB] {}, g[MB] {}, gStep[MB] {}, pL[MB] {}, pR[MB] {}, fc[MB] {}, tilt[MB] {};
    float spacing = 1.f;

    void design (int bands, float lo, float hi, float sharp, float fs, bool resetState)
    {
        n = std::clamp (bands, 4, MB);
        hi = std::min (hi, 0.42f * fs);
        lo = std::clamp (lo, 30.f, hi * 0.25f);
        const float r = std::pow (hi / lo, 1.f / (float) (n - 1));
        const float eff = std::log2 (r) * (1.3f - 0.5f * sharp);
        spacing = std::log2 (r) * 12.f;
        float f4[4], q4[4], f3[3], q3[3];
        for (int i = 0; i < n; ++i)
        {
            fc[i] = lo * std::pow (r, (float) i);
            gA[i] = butterworthBandpass (fc[i], eff, 4, fs, f4, q4);
            gS[i] = butterworthBandpass (fc[i], eff, 3, fs, f3, q3);
            for (int s = 0; s < 4; ++s) m[s].set (i, f4[s], q4[s], fs);
            for (int s = 0; s < 3; ++s) { cl[s].set (i, f3[s], q3[s], fs); cr[s].set (i, f3[s], q3[s], fs); }
            // l'enveloppe instantanee de la porteuse relache sur quelques periodes
            ceA[i] = std::exp (-1.f / (0.0005f * fs));
            ceR[i] = std::exp (-1.f / (std::max (0.006f, 3.f / fc[i]) * fs));
        }
        if (resetState)
        {
            for (auto* st : { &m[0], &m[1], &m[2], &m[3], &cl[0], &cl[1], &cl[2], &cr[0], &cr[1], &cr[2] }) st->reset();
            for (int i = 0; i < MB; ++i) { e1[i] = e2[i] = modEnv[i] = ce[i] = g[i] = gStep[i] = 0.f; }
        }
    }
    void setEnv (float attMs, float relMs, float fs)
    {
        for (int i = 0; i < n; ++i)
        {
            ea[i] = std::exp (-1.f / (std::max (attMs, 600.f / fc[i]) * 0.001f * fs));
            er[i] = std::exp (-1.f / (std::max (relMs, 2200.f / fc[i]) * 0.001f * fs));
        }
    }
    void setSpread (float s)
    {
        for (int i = 0; i < n; ++i)
        {
            const float p = s * ((i & 1) ? 1.f : -1.f);
            const float a = (p + 1.f) * 0.25f * pi;
            pL[i] = std::cos (a) * 1.41421356f; pR[i] = std::sin (a) * 1.41421356f;
        }
    }
    void setTilt (float db)
    {
        for (int i = 0; i < n; ++i) tilt[i] = dbToGain (db * std::log2 (fc[i] / 1000.f) / std::log2 (10.f));
    }

    inline void tick (float voice, float carL, float carR, bool freeze, float floor, float& yL, float& yR)
    {
        alignas (32) float b[MB], bl[MB], br[MB], ql[MB], qr[MB];
        m[0].runScalar (voice, b, n); m[1].run (b, n); m[2].run (b, n); m[3].run (b, n);
        if (! freeze)
            for (int i = 0; i < n; ++i)
            {
                const float x = std::abs (b[i] * gA[i]);
                e1[i] = x + (x > e1[i] ? ea[i] : er[i]) * (e1[i] - x);
                e2[i] = e1[i] + (e1[i] > e2[i] ? ea[i] : er[i]) * (e2[i] - e1[i]);
                modEnv[i] = e2[i];
            }
        cl[0].runScalar (carL, bl, n); cl[1].run (bl, n); cl[2].runQuad (bl, ql, n);
        cr[0].runScalar (carR, br, n); cr[1].run (br, n); cr[2].runQuad (br, qr, n);
        float sl = 0.f, sr = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float ampL = std::sqrt (bl[i] * bl[i] + ql[i] * ql[i]);
            const float ampR = std::sqrt (br[i] * br[i] + qr[i] * qr[i]);
            const float amp = gS[i] * std::max (ampL, ampR);
            ce[i] = amp + (amp > ce[i] ? ceA[i] : ceR[i]) * (ce[i] - amp);
            // diviseur >= amplitude instantanee : la sortie de la bande ne peut jamais depasser son gain
            const float den = std::max (std::max (ce[i], amp), floor);
            g[i] += gStep[i];
            const float f = g[i] * gS[i] / den;
            sl += bl[i] * f * pL[i];
            sr += br[i] * f * pR[i];
        }
        yL = sl; yR = sr;
    }

    // gains cibles (cadence de controle)
    void control (float shiftBands, float whiten, float dyn, float floor, int ctrl)
    {
        float mean = 0.f;
        for (int i = 0; i < n; ++i) mean += ce[i];
        mean = mean / (float) n + 1e-9f;
        const float presence = mean / (mean + floor);
        const float ex = 1.f - whiten;
        for (int i = 0; i < n; ++i)
        {
            const float src = (float) i - shiftBands;
            float me = 0.f;
            if (src >= 0.f && src <= (float) (n - 1))
            {
                const int i0 = (int) src; const float t = src - (float) i0;
                const int i1 = std::min (n - 1, i0 + 1);
                me = modEnv[i0] * (1.f - t) + modEnv[i1] * t;
            }
            else if (src > -1.f && src < 0.f) me = modEnv[0] * (1.f + src);
            else if (src > (float) (n - 1) && src < (float) n) me = modEnv[n - 1] * ((float) n - src);
            float gain = me * tilt[i] * presence * dyn;
            if (ex > 0.001f) gain *= std::pow (std::max (ce[i], floor) / mean, ex);
            gStep[i] = (gain - g[i]) / (float) ctrl;
        }
    }
    float meanCarrier() const { float s = 0.f; for (int i = 0; i < n; ++i) s += ce[i]; return n > 0 ? s / (float) n : 0.f; }
};

struct VocoderParams
{
    int bands = 24;
    float formant = 0.f, attackMs = 4.f, releaseMs = 40.f, sharp = 0.5f;
    float sibilance = 0.5f, breath = 0.4f, bright = 3.f, gateDb = -60.f;
    float character = 0.1f, spread = 0.f, lo = 80.f, hi = 12000.f;
    bool freeze = false;
};

class Vocoder
{
public:
    static constexpr int maxBands = MB, ctrl = 16;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        hpA.set (3800.f, 0.54f, fs); hpB.set (3800.f, 1.31f, fs);
        detHp.set (4500.f, 0.707f, fs);
        hfEnv.set (2.f, 30.f, fs); totEnv.set (2.f, 30.f, fs);
        gateEnv.set (1.f, 40.f, fs);
        uvSmooth.setMs (8.f, fs);
        gateSmooth.setMs (12.f, fs);
        rmsIn.setMs (400.f, fs); rmsOut.setMs (400.f, fs);
        carPeak = 1e-4f; autoGain = autoGainTarget = 1.f;
        built = false; cur = 0; xf = 1.f;
        holdLeft = 0;
    }

    void setParams (const VocoderParams& p)
    {
        const bool first = ! built;
        const bool countChanged = first || p.bands != prm.bands;
        const bool shape = first || p.sharp != prm.sharp || p.lo != prm.lo || p.hi != prm.hi;
        const bool env = shape || countChanged || p.attackMs != prm.attackMs || p.releaseMs != prm.releaseMs;
        const bool spr = env || p.spread != prm.spread;
        const bool tl = shape || countChanged || p.bright != prm.bright;
        prm = p;
        if (countChanged)
        {
            if (first) { banks[0].design (p.bands, p.lo, p.hi, p.sharp, fs, true); cur = 0; xf = 1.f; }
            else
            {
                // nouveau nombre de bandes : on construit l'autre banc et on passe en fondu
                prev = cur; cur = 1 - cur;
                banks[cur].design (p.bands, p.lo, p.hi, p.sharp, fs, true);
                xf = 0.f;
            }
            built = true;
        }
        else if (shape) banks[cur].design (p.bands, p.lo, p.hi, p.sharp, fs, false);   // coefficients seulement : pas de clic
        auto& b = banks[cur];
        if (env) b.setEnv (p.attackMs, p.releaseMs, fs);
        if (spr) b.setSpread (p.spread);
        if (tl) b.setTilt (p.bright);
    }

    inline void process (float voice, float carL, float carR, float carrierLevel, float& outL, float& outR)
    {
        // consonnes / gate
        const float tot = totEnv.process (voice);
        const float hf = hfEnv.process (detHp.hp (voice));
        float uv = std::clamp ((hf / (tot + 1e-6f) - 0.22f) / 0.35f, 0.f, 1.f);
        uv = uvSmooth.process (uv * uv * (3.f - 2.f * uv));
        unvoiced = uv;
        // gate avec hysteresis (6 dB) et maintien (80 ms) : il ne "bat" jamais
        const float levDb = gainToDb (gateEnv.process (voice) * 1.4142f);
        if (levDb > prm.gateDb) { gateOpen = true; holdLeft = (int) (0.08f * fs); }
        else if (levDb < prm.gateDb - 6.f && --holdLeft <= 0) gateOpen = false;
        const float gate = gateSmooth.process (gateOpen ? 1.f : 0.f);

        const float carMid = 0.5f * (carL + carR);
        carRms = carRms * 0.9995f + 0.0005f * carMid * carMid;
        const float nz = noise.next() * std::sqrt (carRms) * 2.5f * prm.breath * uv;
        carL += nz; carR += nz;

        const float floor = 1.0e-7f + 0.002f * carPeak;
        float yL, yR;
        banks[cur].tick (voice, carL, carR, prm.freeze, floor, yL, yR);
        if (xf < 1.f)
        {
            float pL, pR;
            banks[prev].tick (voice, carL, carR, prm.freeze, floor, pL, pR);
            const float w = 0.5f - 0.5f * std::cos (pi * xf);
            yL = yL * w + pL * (1.f - w); yR = yR * w + pR * (1.f - w);
            xf = std::min (1.f, xf + 1.f / (0.04f * fs));
        }

        if (++ctrlCount >= ctrl)
        {
            ctrlCount = 0;
            auto& b = banks[cur];
            carPeak = std::max (b.meanCarrier(), carPeak * 0.995f);
            const float whiten = 1.f - 0.45f * prm.character;
            const float dyn = std::clamp (carrierLevel, 0.f, 1.f);
            b.control (prm.formant / std::max (0.1f, b.spacing), whiten, dyn, floor, ctrl);
            if (xf < 1.f) banks[prev].control (prm.formant / std::max (0.1f, banks[prev].spacing), whiten, dyn, floor, ctrl);
            carrierOn = dyn * b.meanCarrier() / (b.meanCarrier() + floor);
        }

        const float hiss = hpB.hp (hpA.hp (voice)) * prm.sibilance * uv * 0.9f;
        yL = (yL + hiss) * gate; yR = (yR + hiss) * gate;

        // niveau automatique : le robot sort au niveau de la voix
        if (gate > 0.9f && carrierOn > 0.6f)
        {
            rmsIn.process (voice * voice);
            rmsOut.process (0.5f * (yL * yL + yR * yR));
            if (++agCount >= 64)
            {
                agCount = 0;
                autoGainTarget = std::clamp (std::sqrt ((rmsIn.y + 1e-10f) / (rmsOut.y + 1e-10f)), 0.1f, 12.f);
            }
        }
        autoGain += (autoGainTarget - autoGain) * 0.0001f;
        outL = yL * autoGain; outR = yR * autoGain;
    }

    int numBands() const { return banks[cur].n; }
    float bandFreq (int i) const { return banks[cur].fc[i]; }
    float modLevel (int i) const { return banks[cur].modEnv[i]; }
    float carLevel (int i) const { return banks[cur].ce[i]; }
    float outGain (int i) const { return banks[cur].g[i]; }
    float unvoiced = 0.f;
    bool gateIsOpen() const { return gateOpen; }

private:
    VocoderParams prm;
    Bank banks[2];
    int cur = 0, prev = 1, ctrlCount = 0, agCount = 0, holdLeft = 0;
    float xf = 1.f;
    bool built = false, gateOpen = false;
    Svf hpA, hpB, detHp;
    EnvFollower hfEnv, totEnv, gateEnv;
    OnePole uvSmooth, gateSmooth, rmsIn, rmsOut;
    Rng noise { 4242 };
    float fs = 48000.f, carRms = 0.f, carPeak = 1e-4f, autoGain = 1.f, autoGainTarget = 1.f, carrierOn = 0.f;
};

//==============================================================================
// Doubleur facon Waves Doubler. Chaque voix lit la ligne a retard a une vitesse
// legerement differente (desaccord). Quand la tete de lecture sort de sa fenetre,
// elle saute d'une distance choisie par correlation (synchrone avec la periode de la
// voix) et se fond en 8 ms : une seule tete active la plupart du temps, donc pas de
// filtrage en peigne ni de "flanger", desaccord net au cent pres.
struct DoublerVoiceParams { float delayMs = 12.f, cents = 8.f, pan = -1.f, gainDb = 0.f; };
struct DoublerParams
{
    int voices = 0;                  // 0 OFF, 1 = 2 voix, 2 = 4 voix
    float mix = 0.5f, direct = 1.f, width = 1.f, detuneScale = 1.f, delayScale = 1.f;
    float drift = 0.5f, rate = 0.8f, hpf = 120.f, lpf = 16000.f;
    std::array<DoublerVoiceParams, 4> v;
};

class Doubler
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        line.prepare ((int) (0.4f * fs));
        for (int i = 0; i < 4; ++i)
        {
            auto& v = vs[(size_t) i];
            v = V();
            v.rng = Rng ((uint32_t) (101 + i * 13));
            v.gain.init (0.f, 40.f, fs); v.pan.init (0.f, 60.f, fs);
        }
        mixS.init (0.f, 30.f, fs); dirS.init (1.f, 30.f, fs); widthS.init (1.f, 60.f, fs);
        fadeLen = (int) (0.008f * fs);
        corrLen = (int) (0.006f * fs);
    }
    void setParams (const DoublerParams& p) { prm = p; }
    void reset() { line.reset(); for (auto& v : vs) v.d = -1.f; }

    inline void process (float& L, float& R)
    {
        // filtres des voix doublees (passe-haut / passe-bas 12 dB/oct)
        if (prm.hpf != hpfSet) { hpfSet = prm.hpf; hp.set (prm.hpf, 0.707f, fs); }
        if (prm.lpf != lpfSet) { lpfSet = prm.lpf; lp.set (std::min (prm.lpf, 0.45f * fs), 0.707f, fs); }
        const float in = lp.lp (hp.hp (0.5f * (L + R)));
        line.push (in);
        const int nv = prm.voices <= 0 ? 0 : (prm.voices == 1 ? 2 : 4);
        mixS.t = prm.mix; dirS.t = prm.direct; widthS.t = prm.width;
        const float mix = mixS.next(), dir = dirS.next(), width = widthS.next();
        const float norm = nv == 0 ? 0.f : 1.f / std::sqrt ((float) nv) * 1.25f;
        float oL = 0.f, oR = 0.f;
        const float rate = std::max (0.05f, prm.rate);
        for (int i = 0; i < 4; ++i)
        {
            auto& v = vs[(size_t) i];
            const auto& vp = prm.v[(size_t) i];
            v.gain.t = i < nv ? dbToGain (vp.gainDb) : 0.f;
            const float g = v.gain.next();
            if (g < 1e-5f && v.gain.t == 0.f) { v.d = -1.f; continue; }
            // derive aleatoire lissee (hauteur + retard)
            if (++v.cnt >= v.next)
            {
                v.cnt = 0; v.next = (int) (fs / rate * (0.6f + 0.8f * v.rng.uni()));
                v.dT = v.rng.next(); v.pT = v.rng.next();
            }
            const float sm = std::exp (-rate / (0.7f * fs));
            v.dDr = v.dT + sm * (v.dDr - v.dT);
            v.pDr = v.pT + sm * (v.pDr - v.pT);
            const float centre = std::max (0.004f * fs, (vp.delayMs * prm.delayScale + v.dDr * prm.drift * 3.f) * 0.001f * fs);
            if (v.d < 0.f) { v.d = centre; v.c = centre; v.fading = false; }
            const float dc = centre - v.c;
            v.c = centre;
            const float cents = vp.cents * prm.detuneScale + v.pDr * prm.drift * 6.f;
            const float step = 1.f - std::exp2 (cents / 1200.f) + dc;
            v.d += step;
            if (v.fading) v.dOld += step;
            const float W = std::clamp (0.4f * centre, 0.003f * fs, 0.012f * fs);
            if (! v.fading)
            {
                if (v.d < centre - W) splice (v, +1, W);
                else if (v.d > centre + W) splice (v, -1, W);
            }
            float s = line.read (v.d);
            if (v.fading)
            {
                const float w = 0.5f - 0.5f * std::cos (pi * (float) v.fadePos / (float) fadeLen);
                s = line.read (v.dOld) * (1.f - w) + s * w;
                if (++v.fadePos >= fadeLen) v.fading = false;
            }
            v.pan.t = std::clamp (vp.pan * std::min (1.f, width), -1.f, 1.f);
            const float a = (v.pan.next() + 1.f) * 0.25f * pi;
            oL += s * std::cos (a) * g * norm;
            oR += s * std::sin (a) * g * norm;
        }
        // largeur > 100 % : on elargit encore l'image des doublures (M/S)
        if (width > 1.f)
        {
            const float m = 0.5f * (oL + oR), sd = 0.5f * (oL - oR) * width;
            oL = m + sd; oR = m - sd;
        }
        L = L * dir + oL * mix * 1.4142f;
        R = R * dir + oR * mix * 1.4142f;
    }

private:
    struct V
    {
        float d = -1.f, dOld = 0.f, c = 0.f, dT = 0.f, pT = 0.f, dDr = 0.f, pDr = 0.f;
        int cnt = 0, next = 1, fadePos = 0;
        bool fading = false;
        Rng rng;
        Smooth gain, pan;
    };
    void splice (V& v, int dir, float W)
    {
        // cherche le saut J qui prolonge le mieux la forme d'onde actuelle
        const int jMin = (int) (1.4f * W), jMax = (int) (2.4f * W);
        const int base = (int) v.d;
        const int maxD = line.size() - corrLen - 8;
        float bestC = -2.f; int bestJ = (jMin + jMax) / 2;
        float ea = 0.f;
        for (int k = 0; k < corrLen; k += 2) { const float a = line.readInt (base + k); ea += a * a; }
        for (int J = jMin; J <= jMax; J += 2)
        {
            const int nd = base + dir * J;
            if (nd < 2 || nd > maxD) continue;
            float ab = 0.f, eb = 0.f;
            for (int k = 0; k < corrLen; k += 2)
            {
                const float a = line.readInt (base + k), b = line.readInt (nd + k);
                ab += a * b; eb += b * b;
            }
            const float c = ab / std::sqrt (ea * eb + 1e-12f);
            if (c > bestC) { bestC = c; bestJ = J; }
        }
        v.dOld = v.d;
        v.d = std::clamp (v.d + (float) (dir * bestJ), 2.f, (float) maxD);
        v.fading = true; v.fadePos = 0;
    }

    DelayLine line;
    Svf hp, lp;
    std::array<V, 4> vs;
    DoublerParams prm;
    Smooth mixS, dirS, widthS;
    float fs = 48000.f, hpfSet = -1.f, lpfSet = -1.f;
    int fadeLen = 384, corrLen = 288;
};

//==============================================================================
// Saturation tanh a anti-repliement (ADAA du 1er ordre)
struct AdaaTanh
{
    float x1 = 0.f, F1 = 0.f;
    static inline float logcosh (float x) { const float a = std::abs (x); return a + std::log1p (std::exp (-2.f * a)) - 0.69314718f; }
    inline float process (float x)
    {
        const float F = logcosh (x);
        const float d = x - x1;
        const float y = std::abs (d) < 1e-4f ? std::tanh (0.5f * (x + x1)) : (F - F1) / d;
        x1 = x; F1 = F;
        return y;
    }
};

class Ensemble
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; l.prepare ((int) (0.05f * fs)); r.prepare ((int) (0.05f * fs)); amt.init (0.f, 40.f, fs); }
    void reset() { l.reset(); r.reset(); }
    inline void process (float& L, float& R, float amount, float rate)
    {
        l.push (L); r.push (R);
        amt.t = amount;
        const float a = amt.next();
        if (a <= 0.0001f) return;
        slow += 0.55f * rate / fs; if (slow > 1.f) slow -= 1.f;
        fast += 6.1f / fs;  if (fast > 1.f) fast -= 1.f;
        float wl = 0.f, wr = 0.f;
        for (int k = 0; k < 3; ++k)
        {
            const float o = (float) k / 3.f;
            const float ml = std::sin (2.f * pi * (slow + o)) * 2.2f + std::sin (2.f * pi * (fast + o)) * 0.22f;
            const float mr = std::sin (2.f * pi * (slow + o + 0.5f)) * 2.2f + std::sin (2.f * pi * (fast + o + 0.5f)) * 0.22f;
            wl += l.read ((6.5f + ml) * 0.001f * fs);
            wr += r.read ((6.5f + mr) * 0.001f * fs);
        }
        wl *= 0.4f; wr *= 0.4f;
        L = L * (1.f - 0.45f * a) + wl * a;
        R = R * (1.f - 0.45f * a) + wr * a;
    }
private:
    DelayLine l, r;
    Smooth amt;
    float fs = 48000.f, slow = 0.f, fast = 0.f;
};

// Phaser 6 etages, stereo (LFO decales)
class Phaser
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; mixS.init (0.f, 40.f, fs); reset(); }
    void reset() { for (auto& s : st) s.fill (0.f); fbL = fbR = 0.f; }
    inline void process (float& L, float& R, float mix, float rate, float depth)
    {
        mixS.t = mix;
        const float m = mixS.next();
        if (m <= 0.0001f) return;
        if (++cnt >= 16)
        {
            cnt = 0;
            ph += 16.f * rate / fs; if (ph > 1.f) ph -= 1.f;
            for (int c = 0; c < 2; ++c)
            {
                const float l = 0.5f + 0.5f * std::sin (2.f * pi * (ph + 0.25f * (float) c));
                const float f = 200.f * std::pow (20.f * std::max (0.05f, depth) + 1.f, l);
                const float t = std::tan (pi * std::min (f, 0.45f * fs) / fs);
                coef[c] = (t - 1.f) / (t + 1.f);
            }
        }
        auto run = [&] (float x, int c, float& fb)
        {
            float y = x + 0.45f * fb;
            for (int k = 0; k < 6; ++k)
            {
                const float o = coef[c] * y + st[(size_t) c][(size_t) k];
                st[(size_t) c][(size_t) k] = y - coef[c] * o;
                y = o;
            }
            fb = y;
            return y;
        };
        const float yl = run (L, 0, fbL), yr = run (R, 1, fbR);
        L = L * (1.f - 0.5f * m) + yl * 0.5f * m;
        R = R * (1.f - 0.5f * m) + yr * 0.5f * m;
    }
private:
    std::array<std::array<float, 6>, 2> st {};
    float coef[2] {}, fbL = 0.f, fbR = 0.f, fs = 48000.f, ph = 0.f;
    Smooth mixS;
    int cnt = 0;
};

class TempoDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        l.prepare ((int) (2.6f * fs)); r.prepare ((int) (2.6f * fs));
        for (auto* f : { &hpL, &hpR }) f->set (150.f, 0.6f, fs);
        tSm.setMs (150.f, fs); tSm.y = 0.25f * fs;
        mixS.init (0.f, 40.f, fs);
    }
    void reset() { l.reset(); r.reset(); }
    inline void process (float& L, float& R, float timeSec, float fb, float mix, float toneHz, bool pingPong)
    {
        mixS.t = mix;
        const float m = mixS.next();
        if (toneHz != toneSet) { toneSet = toneHz; lpL.set (toneHz, 0.6f, fs); lpR.set (toneHz, 0.6f, fs); }
        const float d = tSm.process (std::clamp (timeSec, 0.01f, 2.5f) * fs);
        const float dl = l.read (d), dr = r.read (d);
        if (pingPong)
        {
            const float in = 0.5f * (L + R);
            l.push (hpL.hp (lpL.lp (in + dr * fb)));
            r.push (hpR.hp (lpR.lp (dl * fb)));
        }
        else
        {
            l.push (hpL.hp (lpL.lp (L + dl * fb)));
            r.push (hpR.hp (lpR.lp (R + dr * fb)));
        }
        L += dl * m; R += dr * m;
    }
private:
    DelayLine l, r;
    Svf lpL, lpR, hpL, hpR;
    OnePole tSm;
    Smooth mixS;
    float fs = 48000.f, toneSet = -1.f;
};

// Reverbe FDN 8 lignes : pre-delai, taille, amortissement, largeur
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& d : lines) d.prepare ((int) (0.25f * fs));
        for (auto& d : ap) d.prepare ((int) (0.03f * fs));
        pre.prepare ((int) (0.25f * fs));
        mixS.init (0.f, 40.f, fs);
        set (0.5f, 0.5f, true);
    }
    void reset() { for (auto& d : lines) d.reset(); for (auto& d : ap) d.reset(); pre.reset(); damp.fill (0.f); }
    void set (float s, float dampAmt, bool force = false)
    {
        if (! force && std::abs (s - size) < 1e-4f && std::abs (dampAmt - dampSet) < 1e-4f) return;
        size = s; dampSet = dampAmt;
        static constexpr float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 61.3f, 67.1f, 79.3f };
        const float scale = 0.55f + 1.1f * s;
        const float t60 = 0.6f + 5.f * s * s;
        for (int i = 0; i < 8; ++i)
        {
            len[(size_t) i] = base[i] * scale * 0.001f * fs;
            fbk[(size_t) i] = std::pow (10.f, -3.f * len[(size_t) i] / (t60 * fs));
        }
        dampA = std::exp (-2.f * pi * (1500.f + 9000.f * (1.f - dampAmt)) / fs);
    }
    inline void process (float& L, float& R, float mix, float preMs, float width)
    {
        mixS.t = mix;
        const float m = mixS.next();
        if (m <= 0.0001f) return;
        static constexpr float apd[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        pre.push (0.5f * (L + R));
        float x = pre.read (std::max (1.f, preMs * 0.001f * fs));
        for (int i = 0; i < 4; ++i)
        {
            auto& d = ap[(size_t) i];
            const float z = d.read (apd[i] * 0.001f * fs);
            const float v = x + 0.6f * z;
            d.push (v);
            x = z - 0.6f * v;
        }
        std::array<float, 8> s {};
        lfo += 0.31f / fs; if (lfo > 1.f) lfo -= 1.f;
        for (int i = 0; i < 8; ++i)
        {
            const float md = (i & 1) ? std::sin (2.f * pi * (lfo + 0.13f * (float) i)) * 0.0008f * fs : 0.f;
            const float v = lines[(size_t) i].read (len[(size_t) i] + md);
            damp[(size_t) i] = v + dampA * (damp[(size_t) i] - v);
            s[(size_t) i] = damp[(size_t) i] * fbk[(size_t) i];
        }
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = s[(size_t) j], b = s[(size_t) j + h]; s[(size_t) j] = a + b; s[(size_t) j + h] = a - b; }
        for (int i = 0; i < 8; ++i) lines[(size_t) i].push (s[(size_t) i] * 0.35355339f + x * ((i & 2) ? -0.5f : 0.5f));
        float wl = (damp[0] - damp[2] + damp[4] - damp[6]) * 0.6f;
        float wr = (damp[1] - damp[3] + damp[5] - damp[7]) * 0.6f;
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        wl = mid + side; wr = mid - side;
        L = L * (1.f - 0.35f * m) + wl * m;
        R = R * (1.f - 0.35f * m) + wr * m;
    }
private:
    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 4> ap;
    DelayLine pre;
    std::array<float, 8> len {}, fbk {}, damp {};
    Smooth mixS;
    float fs = 48000.f, size = -1.f, dampSet = -1.f, dampA = 0.f, lfo = 0.f;
};

// Compresseur "un bouton" (RMS, stereo lie, rattrapage automatique)
class Compressor
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; det.setMs (8.f, fs); att = std::exp (-1.f / (0.005f * fs)); rel = std::exp (-1.f / (0.12f * fs)); grDb = 0.f; }
    inline void process (float& L, float& R, float amount)
    {
        if (amount <= 0.001f) { grDb = 0.f; return; }
        const float thr = -10.f - 24.f * amount, ratio = 1.5f + 4.f * amount;
        const float lv = det.process (0.5f * (L * L + R * R));
        const float db = 10.f * std::log10 (lv + 1e-12f);
        const float over = db - thr;
        const float target = over > 0.f ? -over * (1.f - 1.f / ratio) : 0.f;
        grDb = target + (target < grDb ? att : rel) * (grDb - target);
        const float makeup = -thr * (1.f - 1.f / ratio) * 0.45f;
        const float g = dbToGain (grDb + makeup);
        L *= g; R *= g;
    }
    float grDb = 0.f;
private:
    OnePole det;
    float fs = 48000.f, att = 0.f, rel = 0.f;
};

// Limiteur de sortie : anticipation optionnelle (mode HQ), gain lisse, plafond -0.3 dBFS
class Limiter
{
public:
    void prepare (double sampleRate, int lookahead)
    {
        fs = (float) sampleRate; la = lookahead;
        l.prepare (std::max (8, la + 4)); r.prepare (std::max (8, la + 4));
        peakHold.assign ((size_t) std::max (1, la + 1), 0.f);
        hp = 0; g = 1.f;
        rel = std::exp (-1.f / (0.08f * fs));
        att = std::exp (-1.f / std::max ((float) la * 0.5f, 0.0002f * fs));
    }
    inline void process (float& L, float& R)
    {
        const float pk = std::max (std::abs (L), std::abs (R));
        float need = pk > ceil ? ceil / pk : 1.f;
        if (la > 0)
        {
            // minimum glissant du gain requis sur la fenetre d'anticipation
            peakHold[(size_t) hp] = need; hp = (hp + 1) % (int) peakHold.size();
            for (auto v : peakHold) need = std::min (need, v);
            l.push (L); r.push (R);
            L = l.readInt (la); R = r.readInt (la);
        }
        g = need < g ? need + att * (g - need) : need + rel * (g - need);
        L *= g; R *= g;
        // filet de securite (ne sert qu'en cas extreme)
        L = std::clamp (L, -1.f, 1.f); R = std::clamp (R, -1.f, 1.f);
        grDb = gainToDb (g);
    }
    float grDb = 0.f;
private:
    DelayLine l, r;
    std::vector<float> peakHold;
    int la = 0, hp = 0;
    float fs = 48000.f, g = 1.f, att = 0.f, rel = 0.f, ceil = 0.966f;
};

//==============================================================================
namespace scales
{
    inline int mask (int scale)
    {
        switch (scale)
        {
            case 1: return 0xFFF;
            case 2: return 0b101010110101;
            case 3: return 0b010110101101;
            case 4: return 0b100110101101;
            case 5: return 0b001010010101;
            case 6: return 0b010010101001;
            case 7: return 0b011010101101;
            default: return 0xFFF;
        }
    }
    inline float quantize (float midi, int key, int scale)
    {
        if (scale == 0) return midi;
        if (scale == 8) { const float rel = midi - (float) key; return (float) key + 12.f * std::round (rel / 12.f); }
        const int m = mask (scale);
        const int base = (int) std::round (midi);
        int best = base; float bd = 99.f;
        for (int o = -6; o <= 6; ++o)
        {
            const int c = base + o;
            if (m & (1 << (((c - key) % 12 + 12) % 12)))
            {
                const float dd = std::abs ((float) c - midi);
                if (dd < bd) { bd = dd; best = c; }
            }
        }
        return (float) best;
    }
    inline float degreeUp (float note, int steps, int key, int scale)
    {
        if (scale == 0 || scale == 1 || scale == 8)
        {
            static constexpr int chrom[5] = { 0, 4, 7, 12, 16 };
            return note + (float) chrom[std::clamp (steps / 2, 0, 4)];
        }
        const int m = mask (scale);
        int c = (int) std::round (note);
        for (int s = 0; s < steps;)
        {
            ++c;
            if (m & (1 << (((c - key) % 12 + 12) % 12))) ++s;
        }
        return (float) c;
    }
}
} // namespace vck

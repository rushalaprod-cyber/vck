#pragma once
// vck : moteur audio
//  - Vocoder a banc de filtres (jusqu'a 64 bandes, filtres 4e ordre, porteuse blanchie)
//  - Porteuse : synthe polyphonique anti-aliase (PolyBLEP), unisson, glide, vibrato
//  - AUTO : suivi de hauteur de la voix (YIN) -> notes quantifiees dans une gamme + harmonies
//  - Doubleur 2 / 4 voix (desaccord + retard + derive naturelle, facon Waves Doubler)
//  - Ensemble, delai synchronise au tempo, reverbe FDN
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <complex>

namespace vck
{
constexpr float pi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.f * std::log10 (std::max (g, 1.0e-9f)); }
inline float mtof (float m)      { return 440.f * std::exp2 ((m - 69.f) / 12.f); }
inline float ftom (float f)      { return 69.f + 12.f * std::log2 (std::max (f, 1.f) / 440.f); }

// Generateur pseudo-aleatoire rapide (xorshift) : pas d'allocation, pas de verrou
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 1u) : s (seed * 2654435761u + 1u) {}
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.f / 8388608.f) - 1.f; } // [-1, 1)
    inline float uni()  { return 0.5f * (next() + 1.f); }
};

//==============================================================================
// Filtre a variables d'etat TPT (Zavalishin) : stable en modulation, tres propre
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    void set (float fc, float Q, float fs)
    {
        g = std::tan (pi * std::min (fc, 0.47f * fs) / fs);
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
    inline float bp (float x) { float v1, v2; tick (x, v1, v2); return k * v1; } // gain unitaire au centre
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

// Suiveur d'enveloppe 2 poles a attaque / relachement separes (peu d'ondulation)
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

//==============================================================================
// Ligne a retard avec lecture interpolee (Hermite 4 points)
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
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = retard en echantillons (>= 1)
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

//==============================================================================
// Detecteur de hauteur YIN (sur un signal decime a ~12 kHz)
class PitchTracker
{
public:
    void prepare (double fs)
    {
        decim = std::max (1, (int) std::round (fs / 12000.0));
        fsd = (float) fs / (float) decim;
        tauMax = std::min (W - 2, (int) (fsd / 60.f));
        tauMin = std::max (2, (int) (fsd / 1100.f));
        aa1.set (0.38f * fsd, 0.54f, (float) fs);
        aa2.set (0.38f * fsd, 1.31f, (float) fs);
        ring.assign (RING, 0.f);
        frame.assign ((size_t) (W + tauMax + 2), 0.f);
        d.assign ((size_t) tauMax + 2, 0.f);
        reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.f);
        aa1.reset(); aa2.reset(); wpos = 0; cnt = 0; hopCnt = 0;
        freq = 0.f; voiced = false; hist[0] = hist[1] = hist[2] = 0.f;
    }
    // renvoie true quand une nouvelle analyse vient d'etre faite
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
    float freq = 0.f;          // Hz (0 = pas de hauteur)
    bool voiced = false;
    float confidence = 0.f;

private:
    static constexpr int RING = 2048, W = 400, HOP = 96;
    void analyse()
    {
        const int n = W + tauMax;
        float energy = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float v = ring[(size_t) ((wpos - n + i) & (RING - 1))];
            frame[(size_t) i] = v; if (i < W) energy += v * v;
        }
        const float rms = std::sqrt (energy / (float) W);
        if (rms < 0.0015f) { setResult (0.f, 0.f); return; }

        // fonction de difference + normalisation cumulee
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
        {
            if (d[(size_t) tau] < 0.14f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0)
        {
            float m = 1.f;
            for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < m) { m = d[(size_t) tau]; best = tau; }
            if (m > 0.3f) { setResult (0.f, 0.f); return; }
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
        // mediane de 3 : supprime les sauts d'octave isoles
        float a = hist[0], b = hist[1], c = hist[2];
        float med = std::max (std::min (a, b), std::min (std::max (a, b), c));
        if (f <= 0.f) { voiced = false; confidence = 0.f; return; }
        freq = med > 0.f ? med : f; voiced = true; confidence = conf;
    }
    Svf aa1, aa2;
    std::vector<float> ring, frame, d;
    float hist[3] {};
    int decim = 4, wpos = 0, cnt = 0, hopCnt = 0, tauMax = 200, tauMin = 11;
    float fsd = 12000.f;
};

//==============================================================================
// Synthe porteur
struct SynthParams
{
    int wave = 0;            // 0 SAW, 1 SQUARE, 2 PULSE, 3 SUPER, 4 BUZZ
    int unison = 1;          // 1..7
    float detune = 0.2f;     // 0..1
    float octave = 0.f;      // demi-tons (multiples de 12)
    float glideMs = 0.f;
    int mode = 0;            // 0 POLY, 1 MONO, 2 LEGATO
    float vibrato = 0.f;     // 0..1
    float attackMs = 5.f, releaseMs = 120.f;
    float noise = 0.f;
    float width = 0.6f;      // ouverture stereo de l'unisson
};

class CarrierSynth
{
public:
    static constexpr int maxVoices = 16, maxUnison = 7;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& v : voices) v = Voice();
        stack.clear(); stack.reserve (128);
        for (int i = 0; i < maxVoices; ++i) voices[(size_t) i].seed (i);
        vibPhase = 0;
    }
    void setParams (const SynthParams& p) { prm = p; }
    void allNotesOff() { for (auto& v : voices) v.gate = false; stack.clear(); }

    void noteOn (int note, float vel)
    {
        if (autoMode) return;
        if (prm.mode != 0)
        {
            removeFromStack (note);
            stack.push_back (note);
            auto& v = voices[0];
            const bool legatoHeld = v.gate && v.note >= 0;
            const bool slide = prm.mode == 1 ? (v.note >= 0) : legatoHeld;
            v.start (note, vel, slide && prm.glideMs > 0.f, ++counter, prm.mode == 2 && legatoHeld, prm.octave);
            return;
        }
        // POLY : meme note deja tenue -> on la relance
        int idx = -1;
        for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].note == note && voices[(size_t) i].env > 0.f) { idx = i; break; }
        if (idx < 0) for (int i = 0; i < maxVoices; ++i) if (! voices[(size_t) i].active()) { idx = i; break; }
        if (idx < 0)
        {   // vol de voix : la plus faible des relachees, sinon la plus ancienne
            float best = 2.f; uint32_t oldest = UINT32_MAX;
            for (int i = 0; i < maxVoices; ++i) if (! voices[(size_t) i].gate && voices[(size_t) i].env < best) { best = voices[(size_t) i].env; idx = i; }
            if (idx < 0) for (int i = 0; i < maxVoices; ++i) if (voices[(size_t) i].order < oldest) { oldest = voices[(size_t) i].order; idx = i; }
        }
        auto& v = voices[(size_t) idx];
        if (prm.glideMs > 0.f && lastNote >= 0) v.logF = std::log (mtof ((float) lastNote + prm.octave));
        v.start (note, vel, prm.glideMs > 0.f && lastNote >= 0, ++counter, false, prm.octave);
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

    // Mode AUTO : la porteuse suit la voix (jusqu'a 5 voix d'harmonie)
    void setAuto (bool on) { if (on != autoMode) { autoMode = on; for (auto& v : voices) v.gate = false; stack.clear(); } }
    void setAutoTargets (const float* notes, int count, bool gate)
    {
        for (int i = 0; i < 5; ++i)
        {
            auto& v = voices[(size_t) i];
            if (i < count && gate)
            {
                if (! v.gate) { v.order = ++counter; if (v.env < 0.01f) v.logF = std::log (mtof (notes[i] + prm.octave)); }
                v.gate = true; v.vel = 1.f; v.note = (int) std::round (notes[i]);
                v.targetNote = notes[i];
            }
            else v.gate = false;
        }
    }
    float autoGlideMs = 30.f;

    // niveau courant de la porteuse (somme des enveloppes, pour respecter velocite et relachement)
    float level() const { return levelNow; }
    int activeNotes (std::array<bool, 128>& out) const
    {
        out.fill (false); int n = 0;
        for (auto& v : voices) if (v.gate && v.note >= 0 && v.note < 128) { out[(size_t) v.note] = true; ++n; }
        return n;
    }

    // met a jour les frequences (a appeler toutes les ~16 echantillons)
    void control (int samples)
    {
        const float dtc = (float) samples / fs;
        vibPhase += dtc * 5.6f; if (vibPhase > 1.f) vibPhase -= 1.f;
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
            // enveloppe d'amplitude (attaque lineaire douce, relachement exponentiel)
            if (v.gate) v.envTarget = std::min (1.f, v.envTarget + aInc);
            else v.envTarget *= rMul;
            v.envStep = (v.envTarget - v.env) / (float) samples;
            v.vibAge = v.gate ? std::min (1.f, v.vibAge + dtc / 0.45f) : v.vibAge;
            const float vibSemi = prm.vibrato * 0.55f * v.vibAge * vib;
            for (int k = 0; k < u; ++k)
            {
                const float off = u == 1 ? 0.f : spread (k, u) * spreadSemi + 0.03f * v.rnd[(size_t) k];
                float f = std::exp (v.logF + (vibSemi + off) * lnSemi);
                v.dt[(size_t) k] = std::min (0.45f, f / fs);
            }
            v.u = u;
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
            if (prm.wave == 5)   // NOISE : souffle (voix chuchotee)
            {
                L += noise.next() * amp * 0.35f;
                R += noise.next() * amp * 0.35f;
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
                L += s * amp * norm * (1.f - p);
                R += s * amp * norm * (1.f + p);
            }
        }
        if (prm.noise > 0.f)
        {
            const float n = noise.next() * 0.25f * prm.noise * std::max (levelNow, 0.f);
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
        float logF = std::log (440.f), targetNote = 69.f, vibAge = 0.f;
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
            note = n; targetNote = (float) n; vel = v; gate = true; order = ord;
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
// Vocoder a banc de filtres
struct VocoderParams
{
    int bands = 24;
    float formant = 0.f;     // demi-tons
    float attackMs = 4.f, releaseMs = 40.f;
    float sharp = 0.5f;      // 0 = bandes larges et douces, 1 = bandes fines et precises
    float sibilance = 0.5f;  // consonnes (s, ch, t) : passage direct + bruit dans la porteuse
    float bright = 3.f;      // inclinaison des aigus (dB)
    float gateDb = -60.f;
    bool freeze = false;
};

class Vocoder
{
public:
    static constexpr int maxBands = 64, ctrl = 16;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        builtBands = -1;
        hpA.set (3800.f, 0.54f, fs); hpB.set (3800.f, 1.31f, fs);
        detHp.set (4500.f, 0.707f, fs);
        hfEnv.set (2.f, 30.f, fs); totEnv.set (2.f, 30.f, fs);
        gateEnv.set (1.f, 60.f, fs);
        uvSmooth.setMs (8.f, fs);
        gateSmooth.setMs (25.f, fs);
        rmsIn.setMs (350.f, fs); rmsOut.setMs (350.f, fs);
        carPeak = 1e-3f;
        autoGain = 1.f;
    }

    void setParams (const VocoderParams& p)
    {
        const bool rebuild = p.bands != prm.bands || p.sharp != prm.sharp || builtBands < 0;
        const bool env = rebuild || p.attackMs != prm.attackMs || p.releaseMs != prm.releaseMs;
        prm = p;
        if (rebuild) build();
        else if (env) setEnvelopes();
        if (rebuild || tiltBuilt != p.bright)
        {
            tiltBuilt = p.bright;
            for (int i = 0; i < n; ++i)
                tilt[(size_t) i] = dbToGain (p.bright * std::log2 (fc[(size_t) i] / 1000.f) / std::log2 (10.f));
        }
    }

    // traite un echantillon : voix (mono) + porteuse stereo -> sortie stereo.
    // carNoiseLevel : niveau de bruit a injecter pour les consonnes (gere ici)
    inline void process (float voice, float carL, float carR, float carrierLevel, float& outL, float& outR)
    {
        // ---- detection des consonnes / du gate sur la voix
        const float tot = totEnv.process (voice);
        const float hf = hfEnv.process (detHp.hp (voice));
        const float ratio = hf / (tot + 1e-6f);
        float uv = std::clamp ((ratio - 0.22f) / 0.35f, 0.f, 1.f);
        uv = uvSmooth.process (uv * uv * (3.f - 2.f * uv));
        const float lev = gateEnv.process (voice);
        const float gTarget = gainToDb (lev * 1.4142f) > prm.gateDb ? 1.f : 0.f;
        const float gate = gateSmooth.process (gTarget);
        unvoiced = uv;

        // bruit dans la porteuse pendant les consonnes (rend les "s" et "f" naturels)
        const float carMid = 0.5f * (carL + carR);
        carRms = carRms * 0.9995f + 0.0005f * carMid * carMid;
        const float nz = noise.next() * std::sqrt (carRms) * 2.2f * prm.sibilance * uv;
        carL += nz; carR += nz;

        // ---- banc de filtres
        float yL = 0.f, yR = 0.f;
        for (int i = 0; i < n; ++i)
        {
            auto& b = band[(size_t) i];
            const float m = b.gA * b.m[3].bp (b.m[2].bp (b.m[1].bp (b.m[0].bp (voice))));   // analyse : Butterworth 8e ordre
            if (! prm.freeze) b.modEnv = b.mEnv.process (m);
            const float cl = b.gS * b.cl[2].bp (b.cl[1].bp (b.cl[0].bp (carL)));            // synthese : Butterworth 6e ordre
            const float cr = b.gS * b.cr[2].bp (b.cr[1].bp (b.cr[0].bp (carR)));
            b.carEnv = b.cEnv.process (0.5f * (cl + cr));
            b.g += b.gStep;
            yL += cl * b.g; yR += cr * b.g;
        }

        // ---- gains (a cadence de controle)
        if (++ctrlCount >= ctrl) { ctrlCount = 0; updateGains (carrierLevel); }

        // ---- consonnes : passage direct des aigus de la voix, seulement quand c'est une consonne
        const float hiss = hpB.hp (hpA.hp (voice)) * prm.sibilance * uv * 0.9f;
        yL += hiss; yR += hiss;

        // ---- niveau automatique : la sortie suit le niveau de la voix
        yL *= gate; yR *= gate;
        if (gate > 0.5f && carrierOn > 0.6f)
        {
            rmsIn.process (voice * voice);
            rmsOut.process (0.5f * (yL * yL + yR * yR));
            if (++agCount >= 64)
            {
                agCount = 0;
                autoGainTarget = std::clamp (std::sqrt ((rmsIn.y + 1e-10f) / (rmsOut.y + 1e-10f)), 0.1f, 12.f);
            }
        }
        autoGain += (autoGainTarget - autoGain) * 0.00015f;
        outL = yL * autoGain; outR = yR * autoGain;
        levelOut = std::max (std::abs (outL), levelOut * 0.9995f);
    }

    int numBands() const { return n; }
    float bandFreq (int i) const { return fc[(size_t) i]; }
    float modLevel (int i) const { return band[(size_t) i].modEnv; }
    float carLevel (int i) const { return band[(size_t) i].carEnv; }
    float outGain (int i) const { return band[(size_t) i].g * band[(size_t) i].carEnv; }
    float unvoiced = 0.f, levelOut = 0.f;

private:
    struct Band
    {
        std::array<Svf, 4> m;
        std::array<Svf, 3> cl, cr;
        float gA = 1.f, gS = 1.f;
        EnvFollower mEnv, cEnv;
        float modEnv = 0, carEnv = 0, g = 0, gStep = 0;
    };

    void build()
    {
        n = std::clamp (prm.bands, 4, maxBands);
        builtBands = n;
        const float lo = 80.f, hi = std::min (12000.f, 0.42f * fs);
        const float r = std::pow (hi / lo, 1.f / (float) (n - 1));
        // Passe-bandes de Butterworth (sommet plat, flancs raides) : largeur a -3 dB un peu
        // plus grande que l'ecart entre bandes (doux) -> un peu plus petite (precis)
        const float eff = std::log2 (r) * (1.3f - 0.5f * prm.sharp);
        spacing = std::log2 (r) * 12.f;
        for (int i = 0; i < n; ++i)
        {
            fc[(size_t) i] = lo * std::pow (r, (float) i);
            auto& b = band[(size_t) i];
            b = Band();
            b.gA = butterworthBandpass (fc[(size_t) i], eff, 4, b.m.data());
            b.gS = butterworthBandpass (fc[(size_t) i], eff, 3, b.cl.data());
            b.cr = b.cl;
        }
        setEnvelopes();
    }
    // Conception exacte d'un passe-bande de Butterworth d'ordre 2*M (transformation passe-bas ->
    // passe-bande dans le domaine pre-deforme de la transformee bilineaire), decompose en M
    // cellules SVF. Renvoie le gain qui ramene la cascade a 1 au centre.
    float butterworthBandpass (float centre, float bwOct, int M, Svf* out) const
    {
        using C = std::complex<double>;
        const double nyq = 0.49 * fs;
        const double fl = centre * std::exp2 (-0.5 * bwOct), fh = std::min (nyq, centre * std::exp2 (0.5 * bwOct));
        const double wl = std::tan (pi * fl / fs), wh = std::tan (pi * fh / fs);
        const double w0 = std::sqrt (wl * wh), B = wh - wl;
        int s = 0;
        C total (1.0, 0.0);
        auto addSection = [&] (C pole)
        {
            if (pole.imag() < 0) pole = std::conj (pole);
            const double wi = std::abs (pole), qi = wi / std::max (1e-9, -2.0 * pole.real());
            const float fi = (float) (std::atan (wi) * fs / pi);
            out[s].set (std::min (fi, 0.47f * fs), (float) qi, fs);
            const C jw (0.0, w0);
            total *= (jw * wi / qi) / (jw * jw + jw * wi / qi + wi * wi);
            ++s;
        };
        constexpr double dpi = 3.141592653589793;
        for (int k = 0; k < M; ++k)
        {
            const C p = std::polar (1.0, dpi * (2.0 * k + M + 1.0) / (2.0 * M));  // pole du passe-bas
            const bool real = std::abs (p.imag()) < 1e-6;
            if (! real && p.imag() < 0.0) continue;                                 // son conjugue suffit
            const C pb = C (real ? -1.0 : p.real(), real ? 0.0 : p.imag()) * B;
            const C d = std::sqrt (pb * pb - 4.0 * w0 * w0);
            if (real) addSection ((pb + d) * 0.5);                                  // pole reel : une cellule
            else { addSection ((pb + d) * 0.5); addSection ((pb - d) * 0.5); }
        }
        jassert (s == M);
        return (float) (1.0 / std::max (1e-12, std::abs (total)));
    }

    void setEnvelopes()
    {
        for (int i = 0; i < n; ++i)
        {
            const float f = fc[(size_t) i];
            // une periode minimale par bande : moins d'ondulation dans les graves
            const float a = std::max (prm.attackMs, 600.f / f);
            const float r = std::max (prm.releaseMs, 2200.f / f);
            band[(size_t) i].mEnv.set (a, r, fs);
            band[(size_t) i].cEnv.set (std::max (1.f, 500.f / f), std::max (12.f, 2500.f / f), fs);
        }
    }

    void updateGains (float carrierLevel)
    {
        // porteuse : niveau moyen des bandes, pour un blanchiment independant du volume
        float mean = 0.f;
        for (int i = 0; i < n; ++i) mean += band[(size_t) i].carEnv;
        mean /= (float) n;
        carPeak = std::max (mean, carPeak * 0.995f);
        const float floor = 1.0e-5f + 0.002f * carPeak;
        const float shift = prm.formant / std::max (0.1f, spacing);   // en bandes
        const float w = 0.95f;                                          // blanchiment (garde 5 % de la pente de la porteuse)
        const float presence = mean / (mean + floor);
        const float dyn = std::clamp (carrierLevel, 0.f, 1.f);
        carrierOn = presence * dyn;
        for (int i = 0; i < n; ++i)
        {
            const float src = (float) i - shift;
            float me = 0.f;
            if (src >= 0.f && src <= (float) (n - 1))
            {
                const int i0 = (int) src; const float t = src - (float) i0;
                const int i1 = std::min (n - 1, i0 + 1);
                me = band[(size_t) i0].modEnv * (1.f - t) + band[(size_t) i1].modEnv * t;
            }
            else if (src > -1.f && src < 0.f) me = band[0].modEnv * (1.f + src);
            else if (src > (float) (n - 1) && src < (float) n) me = band[(size_t) n - 1].modEnv * ((float) n - src);
            me *= tilt[(size_t) i];
            const float ce = std::max (band[(size_t) i].carEnv, floor);
            float gain = me * std::pow (ce / (mean + 1e-9f), 1.f - w) / ce;
            // la sortie de la bande vaut ~ porteuse * gain ~ enveloppe de la voix : le plancher
            // de blanchiment suffit a la borner. Cette limite ne sert que de filet de securite.
            gain = std::min (gain * presence * dyn, 20000.f);
            auto& b = band[(size_t) i];
            b.gStep = (gain - b.g) / (float) ctrl;
        }
    }

    VocoderParams prm;
    std::array<Band, maxBands> band;
    std::array<float, maxBands> fc {}, tilt {};
    Svf hpA, hpB, detHp;
    EnvFollower hfEnv, totEnv, gateEnv;
    OnePole uvSmooth, gateSmooth, rmsIn, rmsOut;
    Rng noise { 4242 };
    float fs = 48000.f, spacing = 1.f, carRms = 0.f, carPeak = 1e-3f, tiltBuilt = -999.f;
    float autoGain = 1.f, autoGainTarget = 1.f, carrierOn = 0.f;
    int n = 24, builtBands = -1, ctrlCount = 0, agCount = 0;
};

//==============================================================================
// Doubleur facon Waves Doubler : 2 ou 4 voix, chacune desaccordee, retardee,
// placee dans l'espace, avec une derive lente (comme un vrai chanteur qui double)
struct DoublerParams { int voices = 0; float detuneCents = 12.f, delayMs = 18.f, width = 0.8f, drift = 0.5f, mix = 0.5f; };

class Doubler
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        line.prepare ((int) (0.25f * fs));
        hp.set (140.f, 0.707f, fs);
        win = 0.042f * fs;
        for (int i = 0; i < 4; ++i)
        {
            auto& v = vs[(size_t) i];
            v.phase = 0.25f * (float) i; v.rng = Rng ((uint32_t) (101 + i * 13));
            v.dSm.setMs (900.f, fs); v.pSm.setMs (700.f, fs);
            v.gainSm.setMs (30.f, fs);
        }
    }
    void setParams (const DoublerParams& p) { prm = p; }
    void reset() { line.reset(); }

    inline void process (float& L, float& R)
    {
        const float in = hp.hp (0.5f * (L + R));
        line.push (in);
        const int nv = prm.voices <= 0 ? 0 : (prm.voices == 1 ? 2 : 4);
        static constexpr float detMul[4] = { 1.f, -1.f, 0.55f, -0.62f };
        static constexpr float dlyMul[4] = { 1.f, 0.7f, 1.4f, 0.45f };
        static constexpr float panPos[4] = { -1.f, 1.f, -0.42f, 0.42f };
        const float norm = nv == 0 ? 0.f : 1.f / std::sqrt ((float) nv) * 1.15f;
        float oL = 0.f, oR = 0.f;
        for (int i = 0; i < 4; ++i)
        {
            auto& v = vs[(size_t) i];
            const float g = v.gainSm.process (i < nv ? 1.f : 0.f);
            if (g < 1e-4f) continue;
            if (++v.cnt >= v.next)   // nouvelle cible de derive
            {
                v.cnt = 0; v.next = (int) (fs * (0.4f + 0.9f * v.rng.uni()));
                v.dT = v.rng.next(); v.pT = v.rng.next();
            }
            const float dDrift = v.dSm.process (v.dT) * prm.drift * 3.5f;      // ms
            const float pDrift = v.pSm.process (v.pT) * prm.drift * 5.f;       // cents
            const float cents = prm.detuneCents * detMul[i] + pDrift;
            const float ratio = std::exp2 (cents / 1200.f);
            v.phase += (1.f - ratio) / win;
            v.phase -= std::floor (v.phase);
            const float base = std::max (1.f, (std::max (0.5f, prm.delayMs * dlyMul[i] + dDrift) * 0.001f * fs) - 0.5f * win);
            const float p2 = v.phase + 0.5f - std::floor (v.phase + 0.5f);
            const float w1 = std::sin (pi * v.phase), w2 = std::sin (pi * p2);
            const float s = line.read (base + v.phase * win) * w1 * w1 + line.read (base + p2 * win) * w2 * w2;
            const float pan = panPos[i] * prm.width;
            const float a = (pan + 1.f) * 0.25f * pi;
            oL += s * std::cos (a) * g * norm;
            oR += s * std::sin (a) * g * norm;
        }
        L += oL * prm.mix * 1.4142f;
        R += oR * prm.mix * 1.4142f;
    }

private:
    struct V { float phase = 0, dT = 0, pT = 0; int cnt = 0, next = 1; Rng rng; OnePole dSm, pSm, gainSm; };
    DelayLine line;
    Svf hp;
    std::array<V, 4> vs;
    DoublerParams prm;
    float fs = 48000.f, win = 2000.f;
};

//==============================================================================
// Ensemble (chorus a 3 phases, facon string machine) : le son "vocoder 80s"
class Ensemble
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; l.prepare ((int) (0.05f * fs)); r.prepare ((int) (0.05f * fs)); }
    void reset() { l.reset(); r.reset(); }
    inline void process (float& L, float& R, float amt)
    {
        l.push (L); r.push (R);
        if (amt <= 0.0001f) return;
        slow += 0.55f / fs; if (slow > 1.f) slow -= 1.f;
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
        L = L * (1.f - 0.45f * amt) + wl * amt;
        R = R * (1.f - 0.45f * amt) + wr * amt;
    }
private:
    DelayLine l, r;
    float fs = 48000.f, slow = 0.f, fast = 0.f;
};

//==============================================================================
// Delai ping-pong synchronise au tempo
class TempoDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        l.prepare ((int) (2.6f * fs)); r.prepare ((int) (2.6f * fs));
        for (auto* f : { &lpL, &lpR }) f->set (5200.f, 0.6f, fs);
        for (auto* f : { &hpL, &hpR }) f->set (180.f, 0.6f, fs);
        tSm.setMs (120.f, fs);
        tSm.y = 0.25f * fs;
    }
    void reset() { l.reset(); r.reset(); }
    inline void process (float& L, float& R, float timeSec, float fb, float mix)
    {
        const float d = tSm.process (std::clamp (timeSec, 0.01f, 2.5f) * fs);
        const float dl = l.read (d), dr = r.read (d);
        const float in = 0.5f * (L + R);
        l.push (hpL.hp (lpL.lp (in + dr * fb)));
        r.push (hpR.hp (lpR.lp (dl * fb)));
        L += dl * mix; R += dr * mix;
    }
private:
    DelayLine l, r;
    Svf lpL, lpR, hpL, hpR;
    OnePole tSm;
    float fs = 48000.f;
};

//==============================================================================
// Reverbe FDN 8 lignes (matrice de Hadamard), diffusion d'entree, amortissement
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (int i = 0; i < 8; ++i) lines[(size_t) i].prepare ((int) (0.2f * fs));
        for (int i = 0; i < 4; ++i) ap[(size_t) i].prepare ((int) (0.03f * fs));
        pre.prepare ((int) (0.05f * fs));
        setSize (0.5f, true);
    }
    void reset()
    {
        for (auto& d : lines) d.reset();
        for (auto& d : ap) d.reset();
        pre.reset(); damp.fill (0.f);
    }
    void setSize (float s, bool force = false)
    {
        if (! force && std::abs (s - size) < 1e-4f) return;
        size = s;
        static constexpr float base[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 61.3f, 67.1f, 79.3f };
        const float scale = 0.55f + 1.1f * s;
        const float t60 = 0.7f + 4.3f * s * s;
        for (int i = 0; i < 8; ++i)
        {
            len[(size_t) i] = base[i] * scale * 0.001f * fs;
            fbk[(size_t) i] = std::pow (10.f, -3.f * len[(size_t) i] / (t60 * fs));
        }
        dampA = std::exp (-2.f * pi * (4200.f + 3000.f * (1.f - s)) / fs);
    }
    inline void process (float& L, float& R, float mix)
    {
        if (mix <= 0.0001f) { tail = 0; return; }
        static constexpr float apd[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        pre.push (0.5f * (L + R));
        float x = pre.read (0.012f * fs);
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
            const float m = (i & 1) ? std::sin (2.f * pi * (lfo + 0.13f * (float) i)) * 0.0008f * fs : 0.f;
            float v = lines[(size_t) i].read (len[(size_t) i] + m);
            damp[(size_t) i] = v + dampA * (damp[(size_t) i] - v);
            s[(size_t) i] = damp[(size_t) i] * fbk[(size_t) i];
        }
        // Hadamard 8 (transformee rapide)
        for (int h = 1; h < 8; h <<= 1)
            for (int i = 0; i < 8; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = s[(size_t) j], b = s[(size_t) j + h]; s[(size_t) j] = a + b; s[(size_t) j + h] = a - b; }
        const float nrm = 0.35355339f;
        for (int i = 0; i < 8; ++i) lines[(size_t) i].push (s[(size_t) i] * nrm + x * ((i & 2) ? -0.5f : 0.5f));
        const float wl = (damp[0] - damp[2] + damp[4] - damp[6]) * 0.6f;
        const float wr = (damp[1] - damp[3] + damp[5] - damp[7]) * 0.6f;
        L = L * (1.f - 0.35f * mix) + wl * mix;
        R = R * (1.f - 0.35f * mix) + wr * mix;
    }
private:
    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 4> ap;
    DelayLine pre;
    std::array<float, 8> len {}, fbk {}, damp {};
    float fs = 48000.f, size = -1.f, dampA = 0.f, lfo = 0.f;
    int tail = 0;
};

//==============================================================================
// Quantification de hauteur (mode AUTO)
namespace scales
{
    // masques de 12 bits (bit 0 = tonique)
    inline int mask (int scale)
    {
        switch (scale)
        {
            case 1: return 0xFFF;                                   // CHROMATIQUE
            case 2: return 0b101010110101;                          // MAJEUR
            case 3: return 0b010110101101;                          // MINEUR
            case 4: return 0b100110101101;                          // MINEUR HARMONIQUE
            case 5: return 0b001010010101;                          // PENTA MAJEURE
            case 6: return 0b010010101001;                          // PENTA MINEURE
            case 7: return 0b011010101101;                          // DORIEN
            default: return 0xFFF;
        }
    }
    inline float quantize (float midi, int key, int scale)
    {
        if (scale == 0) return midi;                                // LIBRE : la voix telle quelle
        if (scale == 8)                                             // DRONE : toujours la tonique
        {
            const float rel = midi - (float) key;
            return (float) key + 12.f * std::round (rel / 12.f);
        }
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
    // n-ieme degre au-dessus dans la gamme (pour les harmonies)
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

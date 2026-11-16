#pragma once

// RETALHO — 54 presets de fábrica em 5 bancos (nomes e conteúdo nossos). Cada um parte da cena padrão.
#include "Dsp.h"

namespace retalho
{
constexpr int numBancos = 5;
inline const char* nomeBanco (int b)
{
    static const char* n[numBancos] = { "RISERS", "DROPS & TAPE", "GLITCH", "GROOVE VOX", "TREMOLO & GATE" };
    return n[lim (b, 0, numBancos - 1)];
}

struct Preset { const char* nome; int banco; void (*aplica) (Cena&); };

namespace pr
{
inline void edit (Cena& c, int shape, int len, int rate, int rampTo = 4, int mod = 0)
{
    c.v[kModo] = mEdit; c.v[kShape] = (float) shape; c.v[kLength] = (float) len; c.v[kRate] = (float) rate;
    c.v[kRampTo] = (float) rampTo; c.v[kRateMod] = (float) mod;
}
inline void pitch (Cena& c, float a, float b, float curva = 50, bool glide = false)
{
    c.v[kPIni] = a; c.v[kPFim] = b; c.v[kPCurva] = curva; c.v[kGlide] = glide ? 1.0f : 0.0f;
}
inline void filtro (Cena& c, int t, float de, float ate, float reso = 30) { c.v[kFiltro] = (float) t; c.v[kFDe] = de; c.v[kFAte] = ate; c.v[kReso] = reso; }
// grade: 'x' liga, '.' desliga, 'r' liga com REVERSE; repete o desenho até 64 passos
inline void grade (Cena& c, const char* g)
{
    const int n = (int) std::strlen (g);
    for (int i = 0; i < maxGrade; ++i)
    {
        const char ch = g[i % n];
        c.grade[i] = retalho::grade (ch != '.', ch == 'r', 255);
    }
}
// passos: tipos (R repeat, P pitch, V reverse, T tape, S speed, G gate, - thru, . mute) e valores (0..f → 0..255)
inline void passos (Cena& c, int nPassos, int read, const char* tipos, const char* vals)
{
    c.v[kModo] = mComplex; c.v[kPassos] = (float) nPassos; c.v[kRead] = (float) read;
    const int nt = (int) std::strlen (tipos), nv = (int) std::strlen (vals);
    for (int i = 0; i < maxPassos; ++i)
    {
        int t = tThru;
        switch (tipos[i % nt])
        {
            case 'R': t = tRepeat; break;  case 'P': t = tPitch; break;  case 'V': t = tReverse; break;
            case 'T': t = tTape; break;    case 'S': t = tSpeed; break;  case 'G': t = tGate; break;
            case '.': t = tMute; break;    default: t = tThru; break;
        }
        const char d = vals[i % nv];
        const int v = (d >= '0' && d <= '9') ? d - '0' : ((d >= 'a' && d <= 'f') ? d - 'a' + 10 : 0);
        c.passo[i] = retalho::passo (t, v * 17, false);
    }
}
inline void groove (Cena& c, int estilo, int bars, float dens, int mix, float duck = -9)
{
    c.v[kModo] = mGroove; c.v[kEstilo] = (float) estilo; c.v[kGBars] = (float) bars; c.v[kDensity] = dens;
    c.v[kMixVoz] = (float) mix; c.v[kDuck] = duck;
    geraEstilo (estilo, bars + 1, c.i (kGGrid) == 1, c.cel);
}
inline void livre (Cena& c, float a, float b)
{
    for (int i = 0; i < numLivre; ++i) c.livre[i] = a + (b - a) * (float) i / (numLivre - 1);
}
} // namespace pr

inline const std::vector<Preset>& presets()
{
    using namespace pr;
    static const std::vector<Preset> v = {
        // ---- RISERS ----
        { "PSY RISER 1/32", 0, [] (Cena& c) { edit (c, shRamp, 1, 3, 4); pitch (c, 0, 12, 50); filtro (c, flLpf, 18000, 900, 35); c.v[kDecay] = 0; c.v[kGate] = 80; } },
        { "HPF RISER", 0, [] (Cena& c) { edit (c, shRamp, 2, 2, 4); pitch (c, 0, 7, 40); filtro (c, flHpf, 100, 3000, 25); c.v[kDecay] = 0; } },
        { "OCTAVE CLIMB", 0, [] (Cena& c) { edit (c, shGrid, 1, 2); pitch (c, 0, 12, 0); c.v[kDecay] = 0; } },
        { "TWO OCTAVES", 0, [] (Cena& c) { edit (c, shRamp, 2, 1, 4); pitch (c, -12, 12, 30); c.v[kDecay] = 0; } },
        { "GLIDE UP", 0, [] (Cena& c) { edit (c, shCurve, 1, 1, 4); c.v[kCurva] = 60; pitch (c, 0, 12, 60, true); c.v[kDecay] = 0; } },
        { "SNARE ROLL VOX", 0, [] (Cena& c) { edit (c, shRamp, 1, 1, 3); c.v[kDecay] = 0.5f; filtro (c, flHpf, 200, 900, 20); c.v[kGate] = 70; } },
        { "SLOW BUILD 2 BARS", 0, [] (Cena& c) { edit (c, shCurve, 2, 0, 4); c.v[kCurva] = 80; pitch (c, 0, 5, 70); c.v[kDecay] = 0; } },
        { "HALF BAR RUSH", 0, [] (Cena& c) { edit (c, shRamp, 0, 2, 4); pitch (c, 0, 12, 20); c.v[kDecay] = 0; } },
        { "TRIPLET RISE", 0, [] (Cena& c) { edit (c, shGrid, 1, 2, 4, 1); pitch (c, 0, 7, 0); c.v[kDecay] = 0; } },
        { "FREE SWELL", 0, [] (Cena& c) { edit (c, shFree, 1, 2); livre (c, 0.1f, 1.0f); pitch (c, 0, 12, 50); c.v[kDecay] = 0; } },
        { "DARK RISER", 0, [] (Cena& c) { edit (c, shRamp, 1, 1, 3); pitch (c, -12, 0, 30); filtro (c, flLpf, 400, 12000, 50); c.v[kDecay] = 0; } },
        { "PING-PONG RISE", 0, [] (Cena& c) { edit (c, shRamp, 1, 2, 4); pitch (c, 0, 12, 50); c.v[kWidth] = 100; c.v[kDecay] = 0; } },
        // ---- DROPS & TAPE ----
        { "TAPE DROP", 1, [] (Cena& c) { edit (c, shGrid, 0, 2); c.v[kTapeStop] = 1; pitch (c, 0, -5, 50); c.v[kDecay] = -1; } },
        { "TAPE STOP 1 BAR", 1, [] (Cena& c) { edit (c, shGrid, 1, 0); c.v[kTapeStop] = 1; c.v[kDecay] = 0; } },
        { "PITCH DIVE", 1, [] (Cena& c) { edit (c, shRamp, 1, 2, 3); pitch (c, 0, -24, -40, true); filtro (c, flLpf, 18000, 500, 30); c.v[kDecay] = 0; } },
        { "REVERSE DROP", 1, [] (Cena& c) { edit (c, shGrid, 1, 1); c.v[kReverse] = 1; c.v[kDecay] = 0; } },
        { "DOWNLIFTER", 1, [] (Cena& c) { edit (c, shCurve, 1, 4, 1); c.v[kCurva] = -50; pitch (c, 0, -12, 50); filtro (c, flLpf, 16000, 700, 25); c.v[kDecay] = -0.5f; } },
        { "STOP & GO", 1, [] (Cena& c) { edit (c, shGrid, 1, 1); grade (c, "xxxx...."); c.v[kDecay] = 0; } },
        { "DROP CUT", 1, [] (Cena& c) { edit (c, shGrid, 0, 0); grade (c, "x..."); c.v[kTapeStop] = 1; } },
        { "BACKSPIN", 1, [] (Cena& c) { edit (c, shGrid, 0, 2); c.v[kReverse] = 1; pitch (c, 0, 12, 50, true); filtro (c, flHpf, 150, 2500, 20); c.v[kDecay] = 0; } },
        { "LAST BEAT STOP", 1, [] (Cena& c) { edit (c, shGrid, 1, 1); c.v[kTapeStop] = 1; c.v[kDecay] = 0; } },
        { "SLOWDOWN", 1, [] (Cena& c) { edit (c, shRamp, 1, 3, 1); pitch (c, 0, -7, 50); c.v[kDecay] = 0; } },
        // ---- GLITCH (COMPLEX) ----
        { "GLITCH 16", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-R-PR-VG-RS-RPT.", "0405800707a0f940"); c.v[kDEvery] = 2; } },
        { "HOLD GLITCH", 2, [] (Cena& c) { passos (c, 1, rdHold, "-RRPRVRGPRRSRPTR", "08cb4058a8d4cbe8"); } },
        { "HITS CHOP", 2, [] (Cena& c) { passos (c, 1, rdHits, "-R-R-RVR-R-RPRT.", "0404040808080b40"); } },
        { "STUTTER X4", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-RRR-RRR-RRR-RRR", "0888088808880888"); } },
        { "REVERSE BITS", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-V-V-V-VV-V-VVVV", "0"); } },
        { "PITCH STAIRS", 2, [] (Cena& c) { passos (c, 1, rdHold, "-PPPPPPPPPPPPPPP", "0899aabbccddeeff"); } },
        { "SPEED WARP", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-S-S-S-S-S-S-SSS", "0f0f0f0f0f0f0f0f"); } },
        { "GATE TRIP", 2, [] (Cena& c) { passos (c, 1, rdFollow, "GGGGGGGGGGGGGGGG", "f4a2f4a2f4a2f824"); } },
        { "TAPE GLITCH", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-RT-RT-R-T-RRTTT", "08f08f0802080af2"); } },
        { "32 STEP MADNESS", 2, [] (Cena& c) { passos (c, 2, rdFollow, "-RPR-VRG-RSRPRT.-RRP-VRG-SRRPRTT", "0485800707a0f94008858007a7a0f942"); c.v[kDEvery] = 1; c.v[kLength] = 2; } },
        { "BROKEN 8", 2, [] (Cena& c) { passos (c, 0, rdHits, "-RVRPRT.", "0c08a8f0"); } },
        { "MUTE HOLES", 2, [] (Cena& c) { passos (c, 1, rdFollow, "-.-..-.--.-.R..", "0"); } },
        // ---- GROOVE VOX ----
        { "OFFBEAT VOX", 3, [] (Cena& c) { groove (c, eOffbeat, 0, 70, mvDuck, -9); } },
        { "GALLOP CHOP", 3, [] (Cena& c) { groove (c, eGallop, 0, 75, mvLayer); } },
        { "SKIP HOUSE", 3, [] (Cena& c) { groove (c, eSkip, 0, 70, mvDuck, -6); } },
        { "2-STEP CUT", 3, [] (Cena& c) { groove (c, e2Step, 0, 80, mvLayer); c.v[kGSwing] = 25; } },
        { "FUNK BR", 3, [] (Cena& c) { groove (c, eFunk, 0, 75, mvDuck, -8); } },
        { "TRAP ROLLS", 3, [] (Cena& c) { groove (c, eTrap, 0, 80, mvLayer); } },
        { "CALL & RESPONSE", 3, [] (Cena& c) { groove (c, eCallResp, 0, 80, mvLayer); } },
        { "OFFBEAT REPLACE", 3, [] (Cena& c) { groove (c, eOffbeat, 0, 90, mvReplace); } },
        { "GALLOP 2 BARS", 3, [] (Cena& c) { groove (c, eGallop, 1, 85, mvDuck, -10); } },
        { "OFFBEAT +12", 3, [] (Cena& c) { groove (c, eOffbeat, 0, 70, mvLayer); c.v[kGPitch] = 12; c.v[kChopHpf] = 400; c.v[kChopVol] = -3; } },
        { "OFFBEAT -12", 3, [] (Cena& c) { groove (c, eOffbeat, 0, 70, mvLayer); c.v[kGPitch] = -12; c.v[kChopLpf] = 6000; } },
        { "SWING SKIP", 3, [] (Cena& c) { groove (c, eSkip, 0, 75, mvDuck, -7); c.v[kGSwing] = 40; } },
        { "HUMAN FUNK", 3, [] (Cena& c) { groove (c, eFunk, 1, 80, mvLayer); c.v[kHumanize] = 60; } },
        { "TRIPLET VOX", 3, [] (Cena& c) { c.v[kGGrid] = 1; groove (c, eOffbeat, 0, 75, mvDuck, -8); } },
        // ---- TREMOLO & GATE ----
        { "TREMOLO 1/16 SQUARE", 4, [] (Cena& c) { edit (c, shTremolo, 1, 2); c.v[kTrem] = trSquare; c.v[kGate] = 55; } },
        { "TREMOLO 1/8 SINE", 4, [] (Cena& c) { edit (c, shTremolo, 1, 1); c.v[kTrem] = trSine; } },
        { "SAW CHOP 1/16", 4, [] (Cena& c) { edit (c, shTremolo, 1, 2); c.v[kTrem] = trSaw; } },
        { "TRANCE GATE 1/32", 4, [] (Cena& c) { edit (c, shTremolo, 1, 3); c.v[kTrem] = trSquare; c.v[kGate] = 50; } },
        { "GATE 1/8 DOTTED", 4, [] (Cena& c) { edit (c, shTremolo, 2, 1, 4, 2); c.v[kTrem] = trSquare; c.v[kGate] = 60; } },
        { "SIDECHAIN SAW 1/4", 4, [] (Cena& c) { edit (c, shTremolo, 1, 0); c.v[kTrem] = trSaw; c.v[kWidth] = 50; } },
    };
    return v;
}

// cena completa de um preset (parte da padrão)
inline Cena cenaPreset (int i)
{
    Cena c;
    const auto& v = presets();
    if (i >= 0 && i < (int) v.size()) v[(size_t) i].aplica (c);
    return c;
}
inline int presetPorNome (const char* nome)
{
    const auto& v = presets();
    for (int i = 0; i < (int) v.size(); ++i) if (std::strcmp (v[(size_t) i].nome, nome) == 0) return i;
    return -1;
}

} // namespace retalho

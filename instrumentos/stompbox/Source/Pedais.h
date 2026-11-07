#pragma once

// STOMPBOX — o que a tela e o processador sabem de cada pedal (nome, knobs, chave, cores). Sem JUCE.

#include <cstdint>

namespace stomp
{
struct InfoPedal
{
    const char* nome;        // nome no pedal
    const char* tipo;        // o que é (inglês, como na serigrafia)
    const char* categoria;   // filtro do seletor (PT)
    int nk; const char* knob[4]; float def[4];   // knobs (0..10)
    int nsw; const char* sw[3];                  // posições da chave (0 = sem chave)
    const char* rotuloSw;                        // texto acima da chave ("" = nenhum)
    uint32_t cor, texto, faixa;                  // carcaça, serigrafia, faixa (0 = sem)
    int fonte;      // 0 Arial Black, 1 Arial Black itálico, 2 Georgia, 3 Impact, 4 Georgia itálico, 5 Segoe UI Light
    int knobEst;    // 0 preto, 1 metal, 2 creme, 3 bico (chicken head)
    int forma;      // 0 caixa, 1 redondo, 2 wah
};

inline const InfoPedal& infoPedal (int t)
{
    static const InfoPedal v[] = {
        { "", "", "", 0, {}, {}, 0, {}, "", 0xff303030, 0xffffffff, 0, 0, 0, 0 },
        { "SQUASH", "COMPRESSOR", "DIN\xc3\x82MICA", 4, { "LEVEL", "SUSTAIN", "ATTACK", "BLEND" }, { 5, 5, 4, 7 }, 0, {}, "",
          0xff2f6fb5, 0xfff2f6fb, 0, 0, 1, 0 },
        { "HUMP", "OVERDRIVE", "DRIVE", 3, { "DRIVE", "LEVEL", "TONE" }, { 4, 6, 5 }, 0, {}, "",
          0xff3c8a45, 0xfff4f7ef, 0, 1, 0, 0 },
        { "CLEARCUT", "TRANSPARENT OD", "DRIVE", 4, { "GAIN", "TREBLE", "VOLUME", "BASS" }, { 3, 5, 6, 5 }, 2, { "LOW", "HIGH" }, "HEADROOM",
          0xffcfae62, 0xff2b200f, 0, 2, 2, 0 },
        { "GRIT", "DISTORTION", "DRIVE", 3, { "DIST", "FILTER", "VOLUME" }, { 6, 4, 5 }, 2, { "CLASSIC", "MODERN" }, "",
          0xff26272a, 0xfff1f1f1, 0xffe2431f, 3, 0, 0 },
        { "WOOLLY", "FUZZ", "DRIVE", 2, { "VOLUME", "FUZZ" }, { 5, 7 }, 2, { "GE", "SI" }, "",
          0xffb3302a, 0xfffbeee0, 0, 4, 3, 1 },
        { "QUACK", "WAH", "FILTRO", 3, { "PEDAL", "Q", "RANGE" }, { 5, 5, 5 }, 2, { "PEDAL", "AUTO" }, "",
          0xff2a2a2c, 0xffe9e9e9, 0, 0, 1, 2 },
        { "SWIRL", "PHASER", "MODULA\xc3\x87\xc3\x83O", 2, { "SPEED", "FEEDBACK" }, { 3, 3 }, 2, { "VINTAGE", "MODERN" }, "",
          0xffee7f1e, 0xff2a1305, 0, 0, 0, 0 },
        { "TIDE", "CHORUS / VIBRATO", "MODULA\xc3\x87\xc3\x83O", 3, { "RATE", "DEPTH", "MIX" }, { 3, 5, 5 }, 2, { "CHORUS", "VIBRATO" }, "",
          0xff58b9c4, 0xff0f2a30, 0, 5, 0, 0 },
        { "ECHO", "ANALOG / TAPE DELAY", "TEMPO", 4, { "TIME", "FEEDBACK", "MIX", "MOD" }, { 5, 3, 3, 2 }, 2, { "BBD", "TAPE" }, "",
          0xffd8cda8, 0xff3b2a17, 0, 2, 2, 0 },
        { "HALO", "REVERB", "AMBI\xc3\x8ANCIA", 4, { "DECAY", "TONE", "MIX", "PRE-DLY" }, { 5, 5, 3, 1 }, 3, { "SPRING", "PLATE", "HALL" }, "",
          0xff4a2e78, 0xffefe7fb, 0, 5, 1, 0 },
        { "BENDER", "PITCH SHIFTER", "PITCH", 3, { "PEDAL", "SHIFT", "MIX" }, { 10, 7.9f, 10 }, 2, { "WHAMMY", "DETUNE" }, "",
          0xffc81e2e, 0xfffbe9e9, 0, 0, 1, 2 },
    };
    return v[(t >= 0 && t <= 11) ? t : 0];
}

struct InfoAmp { const char* nome; const char* sub; uint32_t tolex, painel, texto, grade; };
inline const InfoAmp& infoAmp (int a)
{
    static const InfoAmp v[] = {
        { "BLONDE 65", "AMERICAN CLEAN", 0xffe7d6a8, 0xfff3ecdc, 0xff5a3a1a, 0xff6e2a24 },
        { "CHIME 30",  "CLASS A \xc2\xb7 TOP BOOST", 0xff2a2624, 0xffc8a46a, 0xff2a1a0e, 0xff7a5a3c },
        { "PLEX 68",   "BRITISH CRUNCH", 0xff1e1c1b, 0xffd6b45a, 0xff1a140a, 0xffcfc2a2 },
        { "CITRUS",    "BRITISH THICK", 0xffd4621c, 0xfff3ecdc, 0xff7a4a25, 0xffd9cdb0 },
        { "IRON HG",   "MODERN HIGH GAIN", 0xff1a1a1c, 0xffb9bcc0, 0xff111111, 0xff2c2c2e },
    };
    return v[(a >= 0 && a <= 4) ? a : 0];
}
} // namespace stomp

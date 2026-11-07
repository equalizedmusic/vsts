#pragma once

// STOMPBOX — presets de fábrica (sem JUCE: o teste de bancada usa os mesmos).
// Knobs na escala do pedal (0 a 10). Os nomes não citam artista nem marca; a referência de cada um fica no
// comentário e no README.

#include <vector>

namespace stomp
{
struct PresetPedal { int tipo; bool on; float k[4]; int sw; };

struct Preset
{
    const char* nome;
    std::vector<PresetPedal> pedais;
    int amp; int canal; float ampK[6];   // GAIN BASS MID TREBLE PRESENCE MASTER (0..10)
    bool ampOn; int cab; int mic; float micPos, dist;   // micPos/dist 0..10
    float gateThr;                       // dB (-90 = desligado)
    float inDb, outDb;
};

// tipos: 1 SQUASH, 2 HUMP, 3 CLEARCUT, 4 GRIT, 5 WOOLLY, 6 QUACK, 7 SWIRL, 8 TIDE, 9 ECHO, 10 HALO
// amps: 0 BLONDE 65, 1 CHIME 30, 2 PLEX 68, 3 CITRUS, 4 IRON HG; canal 0 CLEAN, 1 DIRTY; cab 0 1x12 OPEN, 1 2x12, 2 4x12; mic 0 DYN, 1 RIBBON
inline const std::vector<Preset>& presetsFabrica()
{
    static const std::vector<Preset> p {
        // 0: board vazio, amp limpo
        { "INIT", {},
          0, 0, { 3.0f, 5.0f, 5.0f, 6.0f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, 5.0f },

        // o board do esboço: compressor, drive verde e eco no amp laranja
        { "BLUES CRUNCH", {
              { 1, true,  { 5.0f, 5.0f, 3.0f, 6.0f }, 0 },
              { 2, true,  { 4.0f, 6.0f, 5.0f, 0.0f }, 0 },
              { 4, false, { 6.0f, 4.0f, 5.0f, 0.0f }, 0 },
              { 8, false, { 3.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 5.5f, 3.0f, 2.5f, 2.0f }, 0 },
              { 10, true, { 5.0f, 5.0f, 2.0f, 2.0f }, 1 } },
          3, 1, { 4.5f, 5.5f, 6.0f, 5.5f, 5.0f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -70.0f, 0.0f, -1.0f },

        // Tom Morello: Marshall de cabeçote (JCM) sem pedal de drive, médio na cara, slap curto;
        // wah e phaser à mão (desligados). Não tem pitch shifter (o Whammy dele) no STOMPBOX.
        { "KILL SWITCH", {
              { 6, false, { 6.0f, 5.0f, 5.0f, 0.0f }, 0 },
              { 7, false, { 3.0f, 2.0f, 0.0f, 0.0f }, 0 },
              { 9, true,  { 2.5f, 1.0f, 1.5f, 0.0f }, 0 } },
          2, 1, { 6.5f, 5.0f, 7.0f, 6.0f, 5.5f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -60.0f, 0.0f, -1.0f },

        // Slash: Marshall com médio gordo, sem pedal de drive; wah para o solo, eco curto e plate
        { "TOP HAT", {
              { 6, false, { 7.0f, 6.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 4.5f, 2.5f, 2.0f, 2.0f }, 0 },
              { 10, true, { 4.0f, 5.0f, 1.5f, 2.0f }, 1 } },
          2, 1, { 7.5f, 6.0f, 7.0f, 5.5f, 5.0f, 6.5f }, true, 2, 0, 3.0f, 2.0f, -62.0f, 0.0f, -2.5f },

        // John Frusciante: Strat num Marshall limpo no limite, chorus ligado; distorção de pedal laranja,
        // wah e eco de fita prontos para ligar
        { "FUNK EDGE", {
              { 4, false, { 5.0f, 4.0f, 6.0f, 0.0f }, 0 },
              { 6, false, { 6.0f, 5.0f, 5.0f, 0.0f }, 0 },
              { 8, true,  { 3.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, false, { 5.0f, 3.0f, 2.5f, 3.0f }, 1 } },
          2, 0, { 6.0f, 4.5f, 6.0f, 6.5f, 5.0f, 6.0f }, true, 2, 0, 4.0f, 3.0f, -90.0f, 0.0f, 3.0f },

        // David Gilmour: compressor + fuzz de silício para o solo longo, eco de fita (~440 ms) e hall grande,
        // num amp de muito headroom
        { "SKY LEAD", {
              { 1, true,  { 5.0f, 5.0f, 4.0f, 6.0f }, 0 },
              { 5, true,  { 5.5f, 7.5f, 0.0f, 0.0f }, 1 },
              { 8, false, { 2.0f, 4.0f, 3.0f, 0.0f }, 0 },
              { 9, true,  { 7.0f, 4.0f, 3.0f, 3.0f }, 1 },
              { 10, true, { 7.0f, 5.0f, 3.0f, 3.0f }, 2 } },
          0, 0, { 4.0f, 5.0f, 6.0f, 6.0f, 5.0f, 7.0f }, true, 2, 0, 3.0f, 3.0f, -75.0f, 0.0f, -2.5f },

        // Jimi Hendrix: fuzz de germânio num Marshall de 1967 alto, mola curta; wah e o vibrato (Uni-Vibe) à mão
        { "'67 FUZZ", {
              { 6, false, { 6.0f, 6.0f, 4.0f, 0.0f }, 0 },
              { 5, true,  { 6.0f, 8.0f, 0.0f, 0.0f }, 0 },
              { 8, false, { 3.0f, 6.0f, 5.0f, 0.0f }, 1 },
              { 10, true, { 3.0f, 5.0f, 1.5f, 0.0f }, 0 } },
          2, 1, { 5.5f, 5.0f, 6.0f, 6.5f, 6.0f, 8.0f }, true, 2, 0, 4.0f, 3.0f, -70.0f, 0.0f, -6.0f },

        // Linkin Park (Brad Delson): retificado high-gain com o drive verde só como boost (aperta o grave), gate firme
        { "HYBRID WALL", {
              { 2, true,  { 0.0f, 8.0f, 6.0f, 0.0f }, 0 } },
          4, 1, { 6.5f, 6.0f, 4.0f, 6.0f, 6.0f, 6.0f }, true, 2, 0, 3.0f, 1.5f, -50.0f, 0.0f, 1.0f },

        // Tony Iommi: treble booster (o CLEARCUT com agudo no talo) num britânico encorpado, escuro e pesado
        { "DOOM RIFF", {
              { 3, true,  { 6.0f, 9.0f, 7.0f, 2.0f }, 1 },
              { 10, true, { 3.0f, 4.0f, 1.5f, 1.0f }, 1 } },
          3, 1, { 7.0f, 6.5f, 6.0f, 5.0f, 4.0f, 7.0f }, true, 2, 0, 5.0f, 3.0f, -60.0f, 0.0f, -2.5f },

        // Zakk Wylde: drive verde como boost num Marshall no talo, gate; wah, phaser e chorus à mão
        { "BERSERKER", {
              { 6, false, { 7.0f, 7.0f, 3.0f, 0.0f }, 0 },
              { 2, true,  { 1.0f, 8.0f, 6.5f, 0.0f }, 0 },
              { 7, false, { 3.0f, 4.0f, 0.0f, 0.0f }, 1 },
              { 8, false, { 2.0f, 4.0f, 4.0f, 0.0f }, 0 } },
          2, 1, { 8.5f, 6.0f, 6.5f, 6.0f, 6.0f, 6.5f }, true, 2, 0, 2.0f, 2.0f, -55.0f, 0.0f, -4.0f },

        // limpo atmosférico: compressor, chorus, eco de fita longo e hall
        { "AMBIENT SWELL", {
              { 1, true,  { 5.0f, 6.0f, 5.0f, 5.0f }, 0 },
              { 8, true,  { 2.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 7.5f, 5.0f, 3.5f, 4.0f }, 1 },
              { 10, true, { 8.0f, 4.0f, 4.0f, 4.0f }, 2 } },
          0, 0, { 3.0f, 5.0f, 5.0f, 6.0f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 4.0f, -90.0f, 0.0f, 7.0f },

        // classe A com chime e mola, para indie/britpop
        { "CHIME JANGLE", {
              { 1, true,  { 5.0f, 4.0f, 6.0f, 5.0f }, 0 },
              { 10, true, { 4.0f, 6.0f, 2.0f, 0.0f }, 0 } },
          1, 0, { 5.5f, 4.5f, 5.0f, 6.5f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, 2.0f },
    };
    return p;
}
} // namespace stomp

#pragma once

// STOMPBOX — presets de fábrica (sem JUCE: o teste de bancada usa os mesmos).
// Knobs na escala do pedal (0 a 10). Os nomes brincam com o do guitarrista (nunca o nome exato) e não citam marca;
// a referência de cada um fica no comentário e no README.

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

// tipos: 1 SQUASH, 2 HUMP, 3 CLEARCUT, 4 GRIT, 5 WOOLLY, 6 QUACK, 7 SWIRL, 8 TIDE, 9 ECHO, 10 HALO, 11 BENDER
// amps: 0 BLONDE 65, 1 CHIME 30, 2 PLEX 68, 3 CITRUS, 4 IRON HG; canal 0 CLEAN, 1 DIRTY; cab 0 1x12 OPEN, 1 2x12, 2 4x12; mic 0 DYN, 1 RIBBON
inline const std::vector<Preset>& presetsFabrica()
{
    static const std::vector<Preset> p {
        // 0: board vazio, amp limpo
        { "INIT", {},
          0, 0, { 3.0f, 5.0f, 5.0f, 6.0f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, -1.6f },

        // o board do esboço: compressor, drive verde e eco no amp laranja
        { "BLUES CRUNCH", {
              { 1, true,  { 5.0f, 5.0f, 3.0f, 6.0f }, 0 },
              { 2, true,  { 4.0f, 6.0f, 5.0f, 0.0f }, 0 },
              { 4, false, { 6.0f, 4.0f, 5.0f, 0.0f }, 0 },
              { 8, false, { 3.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 5.5f, 3.0f, 2.5f, 2.0f }, 0 },
              { 10, true, { 5.0f, 5.0f, 2.0f, 2.0f }, 1 } },
          3, 1, { 4.5f, 5.5f, 6.0f, 5.5f, 5.0f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -70.0f, 0.0f, -1.9f },

        // Tom Morello: Marshall de cabeçote (JCM) sem pedal de drive, médio na cara, slap curto;
        // BENDER ligado com o pedal no calcanhar (som normal): é só pisar até o bico para o +2 OCT do solo; wah e phaser à mão.
        { "JHON MORELO", {
              { 6, false, { 6.0f, 5.0f, 5.0f, 0.0f }, 0 },
              { 11, true, { 0.0f, 9.5f, 10.0f, 0.0f }, 0 },
              { 7, false, { 3.0f, 2.0f, 0.0f, 0.0f }, 0 },
              { 9, true,  { 2.5f, 1.0f, 1.5f, 0.0f }, 0 } },
          2, 1, { 6.5f, 5.0f, 7.0f, 6.0f, 5.5f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -60.0f, 0.0f, -1.1f },

        // Slash: Marshall com médio gordo, sem pedal de drive; wah para o solo, eco curto e plate
        { "SDASH", {
              { 6, false, { 7.0f, 6.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 4.5f, 2.5f, 2.0f, 2.0f }, 0 },
              { 10, true, { 4.0f, 5.0f, 1.5f, 2.0f }, 1 } },
          2, 1, { 7.5f, 6.0f, 7.0f, 5.5f, 5.0f, 6.5f }, true, 2, 0, 3.0f, 2.0f, -62.0f, 0.0f, -2.4f },

        // John Frusciante: Strat num Marshall limpo no limite, chorus ligado; distorção de pedal laranja,
        // wah e eco de fita prontos para ligar
        { "JHON FRUSHANTE", {
              { 4, false, { 5.0f, 4.0f, 6.0f, 0.0f }, 0 },
              { 6, false, { 6.0f, 5.0f, 5.0f, 0.0f }, 0 },
              { 8, true,  { 3.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, false, { 5.0f, 3.0f, 2.5f, 3.0f }, 1 } },
          2, 0, { 6.0f, 4.5f, 6.0f, 6.5f, 5.0f, 6.0f }, true, 2, 0, 4.0f, 3.0f, -90.0f, 0.0f, 3.5f },

        // David Gilmour: compressor + fuzz de silício para o solo longo, eco de fita (~440 ms) e hall grande,
        // num amp de muito headroom
        { "DAVID GILMOR", {
              { 1, true,  { 5.0f, 5.0f, 4.0f, 6.0f }, 0 },
              { 5, true,  { 5.5f, 7.5f, 0.0f, 0.0f }, 1 },
              { 8, false, { 2.0f, 4.0f, 3.0f, 0.0f }, 0 },
              { 9, true,  { 7.0f, 4.0f, 3.0f, 3.0f }, 1 },
              { 10, true, { 7.0f, 5.0f, 3.0f, 3.0f }, 2 } },
          0, 0, { 4.0f, 5.0f, 6.0f, 6.0f, 5.0f, 7.0f }, true, 2, 0, 3.0f, 3.0f, -75.0f, 0.0f, -9.4f },

        // Jimi Hendrix: fuzz de germânio num Marshall de 1967 alto, mola curta; wah e o vibrato (Uni-Vibe) à mão
        { "JIMMY HENDRIQUES", {
              { 6, false, { 6.0f, 6.0f, 4.0f, 0.0f }, 0 },
              { 5, true,  { 6.0f, 8.0f, 0.0f, 0.0f }, 0 },
              { 8, false, { 3.0f, 6.0f, 5.0f, 0.0f }, 1 },
              { 10, true, { 3.0f, 5.0f, 1.5f, 0.0f }, 0 } },
          2, 1, { 5.5f, 5.0f, 6.0f, 6.5f, 6.0f, 8.0f }, true, 2, 0, 4.0f, 3.0f, -70.0f, 0.0f, -5.1f },

        // Linkin Park (Brad Delson): retificado high-gain com o drive verde só como boost (aperta o grave), gate firme
        { "LINKIN PARQUE", {
              { 2, true,  { 0.0f, 8.0f, 6.0f, 0.0f }, 0 } },
          4, 1, { 6.5f, 6.0f, 4.0f, 6.0f, 6.0f, 6.0f }, true, 2, 0, 3.0f, 1.5f, -50.0f, 0.0f, -0.5f },

        // Tony Iommi: treble booster (o CLEARCUT com agudo no talo) num britânico encorpado, escuro e pesado
        { "TONY IOMI", {
              { 3, true,  { 6.0f, 9.0f, 7.0f, 2.0f }, 1 },
              { 10, true, { 3.0f, 4.0f, 1.5f, 1.0f }, 1 } },
          3, 1, { 7.0f, 6.5f, 6.0f, 5.0f, 4.0f, 7.0f }, true, 2, 0, 5.0f, 3.0f, -60.0f, 0.0f, -2.7f },

        // Zakk Wylde: drive verde como boost num Marshall no talo, gate; wah, phaser e chorus à mão
        { "ZAKK WAILD", {
              { 6, false, { 7.0f, 7.0f, 3.0f, 0.0f }, 0 },
              { 2, true,  { 1.0f, 8.0f, 6.5f, 0.0f }, 0 },
              { 7, false, { 3.0f, 4.0f, 0.0f, 0.0f }, 1 },
              { 8, false, { 2.0f, 4.0f, 4.0f, 0.0f }, 0 } },
          2, 1, { 8.5f, 6.0f, 6.5f, 6.0f, 6.0f, 6.5f }, true, 2, 0, 2.0f, 2.0f, -55.0f, 0.0f, -3.5f },

        // Kurt Cobain (Nirvana): distorção laranja de pedal num amp limpo, chorus de BBD fundo (o do riff
        // arpejado), gate firme; desliga a distorção e fica o limpo com chorus
        { "KURT COBAINHO", {
              { 4, true,  { 7.5f, 3.5f, 5.5f, 0.0f }, 0 },
              { 8, true,  { 3.5f, 7.0f, 5.0f, 0.0f }, 0 },
              { 10, true, { 3.0f, 5.0f, 1.5f, 0.0f }, 1 } },
          0, 0, { 5.0f, 6.0f, 4.0f, 6.5f, 5.5f, 6.5f }, true, 2, 0, 3.0f, 2.0f, -55.0f, 0.0f, -9.5f },

        // The Edge (U2): eco de colcheia pontuada (375 ms = 120 BPM; ligue o SYNC e ponha 1/8.), classe A
        // com chime, compressor e reverb grande
        { "THE EJ", {
              { 1, true,  { 5.0f, 5.0f, 5.0f, 6.0f }, 0 },
              { 9, true,  { 6.6f, 4.0f, 4.5f, 1.0f }, 0 },
              { 10, true, { 6.0f, 6.0f, 2.5f, 2.0f }, 2 } },
          1, 0, { 6.5f, 4.5f, 5.5f, 7.0f, 6.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, -6.2f },

        // Stevie Ray Vaughan: drive verde com pouco ganho e muito nível num clean americano alto, mola;
        // o vibrato rotativo à mão
        { "STEVIE RAY VAUGHAM", {
              { 2, true,  { 2.0f, 6.0f, 6.0f, 0.0f }, 0 },
              { 8, false, { 2.5f, 6.0f, 10.0f, 0.0f }, 1 },
              { 10, true, { 3.5f, 5.0f, 2.0f, 0.0f }, 0 } },
          0, 0, { 6.5f, 6.0f, 5.5f, 6.0f, 6.0f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -90.0f, 0.0f, -9.7f },

        // Eddie Van Halen: britânico no talo (o "brown sound"), phaser lento de 4 estágios ligado, plate
        { "EDDIE VAN HALLEN", {
              { 7, true,  { 2.0f, 1.5f, 0.0f, 0.0f }, 0 },
              { 9, false, { 3.5f, 2.0f, 2.0f, 1.0f }, 0 },
              { 10, true, { 4.0f, 5.0f, 2.0f, 2.0f }, 1 } },
          2, 1, { 8.0f, 5.5f, 6.5f, 6.0f, 5.0f, 7.5f }, true, 2, 0, 3.0f, 2.0f, -60.0f, 0.0f, -4.1f },

        // Jack White (White Stripes): fuzz de silício estourado e o pitch uma oitava abaixo no bico
        // (o riff de baixo que não é baixo); pedal já no bico
        { "JACK WHITTE", {
              { 5, true,  { 6.0f, 9.0f, 0.0f, 0.0f }, 1 },
              { 11, true, { 10.0f, 2.0f, 10.0f, 0.0f }, 0 } },
          2, 1, { 5.0f, 6.5f, 5.0f, 5.5f, 5.0f, 6.5f }, true, 2, 0, 3.0f, 2.0f, -60.0f, 0.0f, -1.2f },

        // Kirk Hammett (Metallica): high-gain com o drive verde de boost, médio cavado, gate firme; wah do solo à mão
        { "KIRK HAMETT", {
              { 6, false, { 6.0f, 6.0f, 5.0f, 0.0f }, 0 },
              { 2, true,  { 0.5f, 8.0f, 6.0f, 0.0f }, 0 },
              { 9, false, { 4.0f, 2.5f, 2.0f, 1.0f }, 0 } },
          4, 1, { 6.5f, 6.5f, 3.0f, 6.5f, 6.0f, 6.0f }, true, 2, 0, 3.0f, 1.5f, -48.0f, 0.0f, -0.3f },

        // Mark Knopfler (Dire Straits): limpo no limite, compressor, médio na frente, plate curto
        { "MARK KNOPFLA", {
              { 1, true,  { 5.0f, 5.5f, 3.0f, 6.0f }, 0 },
              { 3, true,  { 2.5f, 5.0f, 6.0f, 5.0f }, 1 },
              { 9, true,  { 2.5f, 1.5f, 1.5f, 1.0f }, 1 },
              { 10, true, { 3.0f, 6.0f, 2.0f, 1.0f }, 1 } },
          0, 0, { 5.5f, 4.5f, 6.5f, 6.0f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, -8.1f },

        // Billy Corgan (Smashing Pumpkins): parede de fuzz (fuzz de silício empilhado na distorção moderna)
        { "BILLY CORGANN", {
              { 5, true,  { 5.0f, 8.0f, 0.0f, 0.0f }, 1 },
              { 4, true,  { 5.0f, 5.0f, 5.0f, 0.0f }, 1 } },
          3, 1, { 5.0f, 6.5f, 4.5f, 6.0f, 5.0f, 6.0f }, true, 2, 0, 3.0f, 2.0f, -55.0f, 0.0f, -1.6f },

        // Brian May (Queen): treble booster no talo num classe A sujo, eco e plate para as camadas do solo
        { "BRIAN MAIO", {
              { 3, true,  { 7.0f, 8.5f, 7.0f, 3.0f }, 1 },
              { 9, true,  { 6.0f, 3.5f, 3.0f, 1.0f }, 0 },
              { 10, true, { 4.5f, 5.0f, 2.0f, 2.0f }, 1 } },
          1, 1, { 6.0f, 5.0f, 6.0f, 6.0f, 5.5f, 6.5f }, true, 1, 0, 3.0f, 2.0f, -65.0f, 0.0f, -3.3f },

        // Angus Young (AC/DC): sem pedal, britânico de ganho médio no volume, mais ataque que distorção
        { "ANGUS YOUNGUE", {
              { 10, true, { 2.5f, 5.0f, 1.0f, 0.0f }, 1 } },
          2, 1, { 4.5f, 5.0f, 6.5f, 6.5f, 6.0f, 7.5f }, true, 2, 0, 3.0f, 2.0f, -65.0f, 0.0f, -2.8f },

        // Carlos Santana: lead cantado de muito sustain (drive verde num amp sujo), médio gordo, eco e hall
        { "CARLOS SANTANNA", {
              { 2, true,  { 4.0f, 6.5f, 4.5f, 0.0f }, 0 },
              { 9, true,  { 5.0f, 3.0f, 2.0f, 2.0f }, 1 },
              { 10, true, { 5.5f, 4.5f, 2.5f, 2.0f }, 2 } },
          3, 1, { 6.5f, 5.5f, 7.5f, 4.5f, 4.0f, 6.0f }, true, 2, 1, 4.0f, 3.0f, -65.0f, 0.0f, -0.1f },

        // Jonny Greenwood (Radiohead): distorção de op-amp agressiva que entra e sai seca, num limpo;
        // eco e o pitch de oitava acima à mão para os ruídos
        { "JONNY GREENWOODY", {
              { 4, true,  { 8.0f, 6.0f, 5.0f, 0.0f }, 1 },
              { 11, false, { 10.0f, 7.9f, 6.0f, 0.0f }, 0 },
              { 9, false, { 4.0f, 6.0f, 4.0f, 3.0f }, 1 } },
          0, 0, { 4.5f, 5.0f, 5.5f, 6.5f, 6.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -55.0f, 0.0f, -7.9f },

        // limpo atmosférico: compressor, chorus, eco de fita longo e hall
        { "AMBIENT SWELL", {
              { 1, true,  { 5.0f, 6.0f, 5.0f, 5.0f }, 0 },
              { 8, true,  { 2.0f, 5.0f, 4.0f, 0.0f }, 0 },
              { 9, true,  { 7.5f, 5.0f, 3.5f, 4.0f }, 1 },
              { 10, true, { 8.0f, 4.0f, 4.0f, 4.0f }, 2 } },
          0, 0, { 3.0f, 5.0f, 5.0f, 6.0f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 4.0f, -90.0f, 0.0f, -0.5f },

        // classe A com chime e mola, para indie/britpop
        { "CHIME JANGLE", {
              { 1, true,  { 5.0f, 4.0f, 6.0f, 5.0f }, 0 },
              { 10, true, { 4.0f, 6.0f, 2.0f, 0.0f }, 0 } },
          1, 0, { 5.5f, 4.5f, 5.0f, 6.5f, 5.0f, 6.0f }, true, 1, 0, 3.0f, 2.0f, -90.0f, 0.0f, -4.6f },
    };
    return p;
}
} // namespace stomp

// QUIMERA — banco de presets de fábrica.
// O Init primeiro e depois 20 por categoria, na ordem de categoryNames (BASS, LEAD, PLUCK, PAD, ATMOS, PERC,
// DRUMS, KEYS, FX). Valores REAIS (%, Hz, ms, st, dB); listas e liga/desliga = índice / 0 ou 1.
// O que não aparece volta ao PADRÃO do parâmetro (ver createLayout em Params.h).
// mpe, mpeBend e hold NUNCA entram aqui (são de performance).

#include "Params.h"

namespace quimera
{
namespace
{
    using KV = std::pair<juce::String, float>;

    // um slot da matriz: fonte, destino, quantidade (−100..100)
    struct Mod
    {
        Src   src;
        Dst   dst;
        float amt;
    };

    // atalhos de id
    juce::String O (int osc, const char* f) { return oscId (osc, f); }
    juce::String F (int flt, const char* f) { return filterId (flt, f); }
    juce::String E (int env, const char* f) { return envId (env, f); }
    juce::String L (int lfo, const char* f) { return lfoId (lfo, f); }

    // os slots da matriz são preenchidos em ordem (01, 02, ...)
    FactoryPreset mk (int cat, const juce::String& name, std::initializer_list<KV> base,
                        std::initializer_list<Mod> mods = {})
    {
        FactoryPreset p;
        p.category = cat;
        p.name = name;
        p.values.assign (base.begin(), base.end());
        int s = 0;
        for (const auto& m : mods)
        {
            if (s >= numSlots) break;
            p.values.emplace_back (slotId (s, "src"), (float) m.src);
            p.values.emplace_back (slotId (s, "dst"), (float) m.dst);
            p.values.emplace_back (slotId (s, "amt"), m.amt);
            ++s;
        }
        return p;
    }

    // categorias
    constexpr int cBaixo = 0, cLead = 1, cPluck = 2, cPad = 3, cAtmos = 4, cPerc = 5, cBateria = 6, cTeclas = 7, cFx = 8;

    // tabelas (índice em tableNames)
    namespace tb
    {
        constexpr float classica = 0, pulso = 1, serraDupla = 2, triDobrado = 3, metal = 4, formante = 5, sincronizado = 6,
                        dobra = 7, eletrico = 8, quadSerra = 9, senoQuad = 10, escova = 11, cristal = 12, digital = 13,
                        vocal = 14, harmonicos = 15, granito = 16, sinos = 17, fmLinear = 18, fmMetalico = 19,
                        espectral = 20, orgao = 21, corda = 22, quimeraTab = 23;
    }
    // posições úteis da Clássica
    namespace cl { constexpr float seno = 0, tri = 33.3f, serra = 66.7f, quadrada = 100; }
    // warps
    namespace wp { constexpr float nenhum = 0, sync = 1, dobraMais = 2, dobraMenos = 3, pwm = 4, espelho = 5, dobrar = 6,
                                   quantizar = 7, fm = 8, rm = 9, assimetria = 10; }
    // filtros
    namespace ft { constexpr float ladder24 = 0, ladder12 = 1, pb12 = 2, pa12 = 3, pf12 = 4, rejeita = 5, pente = 6, formante = 7; }
    // destino do sinal (osc, sub, ruído)
    namespace rt { constexpr float filtroA = 0, filtroB = 1, ambos = 2, direto = 3; }
    // LFO: formas e modos
    namespace sh { constexpr float seno = 0, tri = 1, serraDesce = 2, serraSobe = 3, quadrada = 4, sah = 5, suave = 6; }
    namespace lm { constexpr float livre = 0, porNota = 1, envelope = 2; }
    // divisões (índice em divNames)
    namespace dv { constexpr float d32 = 0, d16t = 1, d16 = 2, d8t = 3, d332 = 4, d8 = 5, d4t = 6, d316 = 7, d4 = 8,
                                   d38 = 9, d2 = 10, c1 = 11, c2 = 12, c4 = 13; }
    // voz
    namespace pm { constexpr float poli = 0, mono = 1, legato = 2; }
    // efeitos
    namespace ds { constexpr float valvula = 0, duro = 1, dobra = 2, bitcrush = 3, downsample = 4, assimetrico = 5; }
    namespace rv { constexpr float sala = 0, placa = 1, hall = 2; }
    namespace cp { constexpr float simples = 0, multibanda = 1; }
} // namespace

std::vector<FactoryPreset> factoryPresets()
{
    std::vector<FactoryPreset> p;
    p.reserve (1 + numCategories * 20);

    p.push_back ({ -1, u8 ("Init · Quimera"), {} });

    // =====================================================================================================
    // BAIXO
    // =====================================================================================================
    p.push_back (mk (cBaixo, u8 ("Grave Rolante"),
        { { O (0, "pos"), cl::serra }, { id::subOn, 1 }, { id::subLvl, 60 },
          { F (0, "cut"), 220 }, { F (0, "res"), 20 }, { F (0, "env"), 45 },
          { E (0, "a"), 1 }, { E (0, "r"), 80 }, { E (1, "d"), 280 }, { E (1, "s"), 20 },
          { id::polyMode, pm::mono }, { id::volume, -7 } },
        { { srcMacro1, dstFACut, 40 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cBaixo, u8 ("Chão de Terra"),
        { { O (0, "pos"), cl::seno }, { O (0, "lvl"), 70 }, { id::subOn, 1 }, { id::subWave, 1 }, { id::subLvl, 55 },
          { F (0, "cut"), 400 }, { F (0, "res"), 0 }, { E (0, "r"), 90 },
          { id::polyMode, pm::legato }, { id::glide, 60 }, { id::drift, 10 }, { id::volume, -6 } },
        { { srcMacro1, dstO1Pos, 35 }, { srcVel, dstO1Pos, 15 } }));

    p.push_back (mk (cBaixo, u8 ("Ácido Paulista"),
        { { O (0, "pos"), cl::serra }, { F (0, "cut"), 300 }, { F (0, "res"), 82 }, { F (0, "drv"), 30 }, { F (0, "env"), 65 },
          { E (0, "a"), 1 }, { E (0, "r"), 60 }, { E (1, "d"), 220 }, { E (1, "r"), 120 },
          { id::polyMode, pm::legato }, { id::glide, 80 },
          { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 25 }, { id::dstMix, 60 }, { id::volume, -10 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFARes, 15 } }));

    p.push_back (mk (cBaixo, u8 ("Ácido Quadrado"),
        { { O (0, "pos"), cl::quadrada }, { F (0, "cut"), 250 }, { F (0, "res"), 86 }, { F (0, "drv"), 20 }, { F (0, "env"), 70 },
          { E (0, "a"), 1 }, { E (0, "r"), 60 }, { E (1, "d"), 180 },
          { id::polyMode, pm::legato }, { id::glide, 50 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 25 }, { id::dlyMix, 15 }, { id::volume, -10 } },
        { { srcVel, dstFACut, 20 }, { srcMacro1, dstFACut, 35 }, { srcMacro2, dstFADrive, 40 } }));

    p.push_back (mk (cBaixo, u8 ("Reese da Madrugada"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 60 }, { O (0, "uni"), 4 }, { O (0, "det"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), -12 },
          { O (1, "uni"), 4 }, { O (1, "det"), 35 },
          { id::subOn, 1 }, { id::subLvl, 50 },
          { F (0, "cut"), 900 }, { F (0, "res"), 25 }, { F (0, "drv"), 25 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.3f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.17f }, { L (1, "mode"), lm::livre },
          { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 45 }, { id::dstTone, 45 }, { id::dstMix, 70 },
          { id::polyMode, pm::mono }, { id::volume, -10 } },
        { { srcLfo1, dstDetune, 15 }, { srcLfo2, dstFACut, 20 }, { srcMacro1, dstFACut, 45 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cBaixo, u8 ("Reese Corroído"),
        { { O (0, "tab"), tb::escova }, { O (0, "pos"), 40 }, { O (0, "uni"), 4 }, { O (0, "det"), 35 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 14 },
          { O (1, "uni"), 3 }, { O (1, "det"), 30 },
          { id::subOn, 1 }, { id::subLvl, 45 },
          { F (0, "cut"), 1100 }, { F (0, "res"), 30 }, { F (0, "drv"), 20 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.25f }, { L (0, "mode"), lm::livre },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 55 }, { id::dstTone, 40 }, { id::dstMix, 60 },
          { id::phsOn, 1 }, { id::phsRate, 0.15f }, { id::phsDepth, 50 }, { id::phsMix, 35 },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 40 },
          { id::polyMode, pm::mono }, { id::volume, -11 } },
        { { srcLfo1, dstO1Pos, 25 }, { srcMacro1, dstFACut, 45 }, { srcMacro2, dstDetune, 30 } }));

    p.push_back (mk (cBaixo, u8 ("Balanço Sintético"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 3 }, { O (0, "det"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::pulso }, { O (1, "pos"), 30 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 55 },
          { id::subOn, 1 }, { id::subLvl, 50 },
          { F (0, "cut"), 200 }, { F (0, "res"), 45 }, { F (0, "drv"), 30 },
          { L (0, "div"), dv::d8 }, { L (0, "shape"), sh::seno },
          { id::dstOn, 1 }, { id::dstDrive, 35 }, { id::dstMix, 60 },
          { id::polyMode, pm::mono }, { id::volume, -10 } },
        { { srcLfo1, dstFACut, 55 }, { srcMacro1, dstLfo1Rate, 50 }, { srcMacro2, dstFARes, 30 } }));

    p.push_back (mk (cBaixo, u8 ("Gangorra Tercinada"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 60 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 300 }, { F (0, "res"), 55 },
          { L (0, "div"), dv::d8t }, { L (0, "shape"), sh::tri },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 50 },
          { id::polyMode, pm::mono }, { id::volume, -10 } },
        { { srcLfo1, dstFACut, 50 }, { srcLfo1, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 50 }, { srcMacro2, dstLfo1Rate, 25 } }));

    p.push_back (mk (cBaixo, u8 ("Baixo de Lata"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 35 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 60 },
          { F (0, "cut"), 1800 }, { F (0, "res"), 10 }, { F (0, "env"), 35 }, { E (1, "d"), 250 },
          { E (2, "d"), 300 }, { E (0, "r"), 90 },
          { id::polyMode, pm::mono }, { id::volume, -8 } },
        { { srcEnv3, dstO1Pos, 45 }, { srcMacro1, dstO1Pos, 40 }, { srcVel, dstO1Pos, 20 } }));

    p.push_back (mk (cBaixo, u8 ("Ferro Fundido"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 50 }, { O (0, "warp"), wp::dobraMais }, { O (0, "wamt"), 30 },
          { id::subOn, 1 }, { id::subLvl, 55 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 10 }, { F (0, "env"), 30 }, { E (1, "d"), 300 },
          { E (2, "d"), 400 }, { E (0, "r"), 90 },
          { id::polyMode, pm::mono }, { id::volume, -8 } },
        { { srcEnv3, dstO1Warp, 40 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstO1Warp, 50 } }));

    p.push_back (mk (cBaixo, u8 ("Mono de Setenta"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 8 },
          { O (2, "on"), 1 }, { O (2, "tab"), tb::classica }, { O (2, "pos"), cl::quadrada }, { O (2, "oct"), -1 }, { O (2, "lvl"), 50 },
          { F (0, "cut"), 500 }, { F (0, "res"), 30 }, { F (0, "drv"), 20 }, { F (0, "env"), 50 }, { F (0, "key"), 30 },
          { E (1, "d"), 400 }, { E (1, "s"), 30 },
          { id::polyMode, pm::legato }, { id::glide, 40 }, { id::drift, 30 }, { id::volume, -10 } },
        { { srcMacro1, dstFACut, 40 }, { srcVel, dstFACut, 20 }, { srcPressure, dstFACut, 20 } }));

    p.push_back (mk (cBaixo, u8 ("Pé no Porão"),
        { { O (0, "pos"), cl::tri }, { O (0, "lvl"), 40 }, { id::subOn, 1 }, { id::subLvl, 80 },
          { F (0, "cut"), 600 }, { F (0, "res"), 0 }, { E (0, "r"), 60 },
          { id::polyMode, pm::mono }, { id::volume, -6 } },
        { { srcMacro1, dstFACut, 35 }, { srcMacro2, dstO1Level, 40 } }));

    p.push_back (mk (cBaixo, u8 ("Pulso Elástico"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 40 },
          { id::subOn, 1 }, { id::subWave, 2 }, { id::subLvl, 40 },
          { F (0, "cut"), 700 }, { F (0, "res"), 35 }, { F (0, "env"), 40 }, { E (1, "d"), 300 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.5f }, { L (0, "mode"), lm::livre },
          { id::polyMode, pm::legato }, { id::glide, 30 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcMacro1, dstFACut, 40 } }));

    p.push_back (mk (cBaixo, u8 ("Borracha"),
        { { O (0, "tab"), tb::dobra }, { O (0, "pos"), 20 }, { id::subOn, 1 }, { id::subLvl, 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 1200 }, { F (0, "res"), 20 },
          { E (2, "d"), 180 }, { E (0, "r"), 80 },
          { id::polyMode, pm::mono }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pos, 50 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cBaixo, u8 ("Grave Sincronizado"),
        { { O (0, "tab"), tb::sincronizado }, { O (0, "pos"), 10 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 55 },
          { F (0, "cut"), 2000 }, { F (0, "res"), 15 }, { F (0, "env"), 25 },
          { E (2, "d"), 350 }, { E (0, "r"), 80 },
          { id::dstOn, 1 }, { id::dstType, ds::dobra }, { id::dstDrive, 30 }, { id::dstMix, 50 },
          { id::polyMode, pm::mono }, { id::volume, -9 } },
        { { srcEnv3, dstO1Pos, 45 }, { srcMacro1, dstO1Pos, 40 }, { srcVel, dstO1Pos, 15 } }));

    p.push_back (mk (cBaixo, u8 ("Nave Mãe"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 60 }, { O (0, "uni"), 2 }, { O (0, "det"), 15 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::digital }, { O (1, "pos"), 40 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 55 },
          { F (0, "type"), ft::formante }, { F (0, "cut"), 800 }, { F (0, "res"), 50 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::sah },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 50 }, { id::dstMix, 60 },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 55 },
          { id::polyMode, pm::mono }, { id::volume, -11 } },
        { { srcLfo1, dstFACut, 35 }, { srcMacro1, dstO1Pos, 40 }, { srcMacro2, dstFACut, 40 }, { srcRandom, dstO2Pos, 20 } }));

    p.push_back (mk (cBaixo, u8 ("Elétrico Seco"),
        { { O (0, "tab"), tb::eletrico }, { O (0, "pos"), 50 },
          { F (0, "cut"), 450 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 200 }, { E (1, "s"), 10 },
          { E (0, "a"), 1 }, { E (0, "d"), 400 }, { E (0, "s"), 70 }, { E (0, "r"), 40 },
          { id::cmpOn, 1 }, { id::cmpAmt, 35 },
          { id::polyMode, pm::mono }, { id::volume, -7 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cBaixo, u8 ("Dedo na Corda"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 70 }, { id::subOn, 1 }, { id::subLvl, 40 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 5 }, { F (0, "key"), 50 },
          { E (0, "a"), 1 }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "d"), 600 },
          { id::polyMode, pm::mono }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, -60 }, { srcVel, dstFACut, 20 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cBaixo, u8 ("Dobradiça"),
        { { O (0, "tab"), tb::triDobrado }, { O (0, "pos"), 30 }, { id::subOn, 1 }, { id::subLvl, 50 },
          { F (0, "cut"), 900 }, { F (0, "res"), 25 }, { F (0, "env"), 40 }, { E (1, "d"), 350 },
          { E (2, "d"), 500 },
          { id::polyMode, pm::legato }, { id::glide, 120 }, { id::volume, -8 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcMacro1, dstO1Pos, 50 }, { srcPressure, dstO1Pos, 25 } }));

    p.push_back (mk (cBaixo, u8 ("Rugido de Fábrica"),
        { { O (0, "tab"), tb::escova }, { O (0, "pos"), 50 }, { O (0, "uni"), 3 }, { O (0, "det"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::metal }, { O (1, "pos"), 40 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 600 }, { F (0, "res"), 40 }, { F (0, "drv"), 50 }, { F (0, "env"), 40 }, { E (1, "d"), 300 },
          { id::dstOn, 1 }, { id::dstType, ds::assimetrico }, { id::dstDrive, 40 }, { id::dstMix, 60 },
          { id::eqOn, 1 }, { id::eqLow, 3 },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 50 },
          { id::polyMode, pm::mono }, { id::volume, -12 } },
        { { srcRandom, dstO1Pos, 15 }, { srcMacro1, dstFACut, 40 }, { srcMacro2, dstO2Level, 40 } }));

    // =====================================================================================================
    // LEAD
    // =====================================================================================================
    p.push_back (mk (cLead, u8 ("Supersserra Hiper"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 7 }, { O (0, "det"), 35 }, { O (0, "wid"), 90 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "oct"), 1 },
          { O (1, "uni"), 7 }, { O (1, "det"), 30 }, { O (1, "lvl"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 8000 }, { F (0, "res"), 10 },
          { E (0, "a"), 5 }, { E (0, "r"), 300 },
          { id::hypOn, 1 }, { id::hypRate, 45 }, { id::hypDetune, 40 }, { id::hypMix, 50 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyFb, 30 }, { id::dlyMix, 15 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 }, { id::volume, -11 } },
        { { srcMacro1, dstFACut, 30 }, { srcWheel, dstDetune, 30 }, { srcPressure, dstFACut, 25 }, { srcSlide, dstDetune, 20 } }));

    p.push_back (mk (cLead, u8 ("Mono de Ouro"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 7 },
          { O (2, "on"), 1 }, { O (2, "tab"), tb::classica }, { O (2, "pos"), cl::quadrada }, { O (2, "oct"), -1 }, { O (2, "lvl"), 55 },
          { F (0, "cut"), 1200 }, { F (0, "res"), 35 }, { F (0, "drv"), 25 }, { F (0, "env"), 45 }, { F (0, "key"), 50 },
          { E (1, "a"), 5 }, { E (1, "d"), 500 }, { E (1, "s"), 40 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.5f }, { L (0, "mode"), lm::livre }, { L (0, "rise"), 800 },
          { id::polyMode, pm::legato }, { id::glide, 70 }, { id::drift, 35 }, { id::volume, -9 } },
        { { srcLfo1, dstPitch, 1 }, { srcPressure, dstFACut, 25 }, { srcSlide, dstFARes, 25 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cLead, u8 ("Glide Setentista"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -1 },
          { F (0, "cut"), 900 }, { F (0, "res"), 45 }, { F (0, "env"), 50 }, { F (0, "key"), 40 },
          { E (1, "d"), 350 }, { E (1, "s"), 50 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyMix, 18 },
          { id::polyMode, pm::legato }, { id::glide, 180 }, { id::drift, 30 }, { id::volume, -9 } },
        { { srcPressure, dstFACut, 30 }, { srcSlide, dstO2Level, -40 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cLead, u8 ("Lâmina Sincronizada"),
        { { O (0, "tab"), tb::sincronizado }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), -10 }, { O (1, "lvl"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 }, { F (0, "res"), 20 },
          { E (2, "d"), 700 }, { E (2, "s"), 30 },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 25 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 30 }, { id::dlyMix, 20 },
          { id::polyMode, pm::legato }, { id::glide, 30 }, { id::volume, -10 } },
        { { srcEnv3, dstO1Pos, 50 }, { srcMacro1, dstO1Pos, 40 }, { srcSlide, dstO1Pos, 40 }, { srcPressure, dstFACut, 25 } }));

    p.push_back (mk (cLead, u8 ("Aço Escovado"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 40 }, { O (0, "warp"), wp::dobraMais }, { O (0, "wamt"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::metal }, { O (1, "pos"), 50 }, { O (1, "lvl"), 40 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 7000 }, { F (0, "res"), 10 },
          { L (0, "div"), dv::d8 }, { L (0, "shape"), sh::tri },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 },
          { id::polyMode, pm::mono }, { id::volume, -9 } },
        { { srcLfo1, dstO1Warp, 30 }, { srcPressure, dstO1Pos, 35 }, { srcSlide, dstO1Warp, 40 }, { srcMacro1, dstO2Level, 40 } }));

    p.push_back (mk (cLead, u8 ("Ping-Pong Neon"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 12 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3500 }, { F (0, "res"), 25 }, { F (0, "env"), 30 },
          { E (1, "d"), 300 }, { E (1, "s"), 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 45 }, { id::dlyPing, 1 }, { id::dlyMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 15 },
          { id::voices, 6 }, { id::volume, -9 } },
        { { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cLead, u8 ("Eco Cruzado"),
        { { O (0, "tab"), tb::quadSerra }, { O (0, "pos"), 50 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 30 }, { F (0, "key"), 40 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.3f }, { L (0, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4t }, { id::dlyFb, 50 }, { id::dlyPing, 1 }, { id::dlyMix, 32 },
          { id::polyMode, pm::legato }, { id::glide, 40 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 30 } }));

    p.push_back (mk (cLead, u8 ("Quimera Desperta"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "oct"), -1 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 25 },
          { E (2, "a"), 300 }, { E (2, "d"), 1500 }, { E (2, "s"), 60 },
          { id::hypOn, 1 }, { id::hypMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 20 }, { id::volume, -10 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcPressure, dstO1Pos, 30 }, { srcSlide, dstFACut, 30 }, { srcMacro1, dstO1Pos, 50 } }));

    p.push_back (mk (cLead, u8 ("Vocalista Robô"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 20 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 6000 },
          { L (0, "div"), dv::d2 }, { L (0, "shape"), sh::tri },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::polyMode, pm::legato }, { id::glide, 60 }, { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcPressure, dstO1Pos, 40 }, { srcSlide, dstFACut, 25 } }));

    p.push_back (mk (cLead, u8 ("Grito Formante"),
        { { O (0, "tab"), tb::formante }, { O (0, "pos"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "lvl"), 35 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.0f }, { L (0, "mode"), lm::livre }, { L (0, "rise"), 400 },
          { id::dstOn, 1 }, { id::dstDrive, 30 }, { id::dstMix, 60 },
          { id::polyMode, pm::legato }, { id::glide, 50 }, { id::volume, -9 } },
        { { srcSlide, dstO1Pos, 80 }, { srcPressure, dstFACut, 30 }, { srcLfo1, dstPitch, 1 }, { srcMacro1, dstO1Pos, 60 } }));

    p.push_back (mk (cLead, u8 ("Assobio no Vale"),
        { { O (0, "pos"), cl::seno },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::tri }, { O (1, "oct"), 1 }, { O (1, "lvl"), 25 },
          { E (0, "a"), 30 }, { E (0, "r"), 400 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.5f }, { L (0, "mode"), lm::livre }, { L (0, "rise"), 500 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 25 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyMix, 20 },
          { id::polyMode, pm::legato }, { id::glide, 90 }, { id::volume, -5 } },
        { { srcLfo1, dstPitch, 1 }, { srcPressure, dstO2Level, 40 }, { srcWheel, dstPitch, 1 } }));

    p.push_back (mk (cLead, u8 ("Serra Elétrica"),
        { { O (0, "tab"), tb::escova }, { O (0, "pos"), 70 }, { O (0, "uni"), 3 }, { O (0, "det"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::eletrico }, { O (1, "pos"), 60 }, { O (1, "semi"), 7 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 20 }, { F (0, "env"), 30 },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 40 }, { id::dstTone, 55 }, { id::dstMix, 60 },
          { id::polyMode, pm::mono }, { id::volume, -11 } },
        { { srcPressure, dstFACut, 25 }, { srcSlide, dstO1Pos, -40 }, { srcMacro1, dstO2Level, -60 } }));

    p.push_back (mk (cLead, u8 ("Laço de Cobre"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 30 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 6000 },
          { E (2, "d"), 500 }, { E (2, "s"), 30 },
          { id::choOn, 1 }, { id::choMix, 25 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyMix, 20 }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 25 }, { srcPressure, dstO1Pos, 30 }, { srcSlide, dstFACut, -30 } }));

    p.push_back (mk (cLead, u8 ("Dobra Viva"),
        { { O (0, "pos"), cl::serra }, { O (0, "warp"), wp::dobraMais }, { O (0, "wamt"), 10 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 6 },
          { F (0, "cut"), 2200 }, { F (0, "res"), 30 }, { F (0, "env"), 35 },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.2f }, { L (1, "mode"), lm::livre },
          { id::polyMode, pm::legato }, { id::glide, 50 }, { id::volume, -9 } },
        { { srcLfo2, dstO1Warp, 40 }, { srcMacro1, dstO1Warp, 60 }, { srcSlide, dstO1Warp, 40 }, { srcPressure, dstFACut, 25 } }));

    p.push_back (mk (cLead, u8 ("Cristal Cortante"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 20 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 9000 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 25 }, { id::volume, -6 } },
        { { srcPressure, dstO1Pos, 30 }, { srcSlide, dstO2Pos, 40 }, { srcMacro1, dstO2Level, 50 } }));

    p.push_back (mk (cLead, u8 ("Fuzz de Garagem"),
        { { O (0, "tab"), tb::quadSerra }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), -15 },
          { F (0, "cut"), 1800 }, { F (0, "res"), 20 }, { F (0, "env"), 30 },
          { id::dstOn, 1 }, { id::dstType, ds::valvula }, { id::dstDrive, 65 }, { id::dstTone, 50 },
          { id::polyMode, pm::mono }, { id::volume, -12 } },
        { { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 }, { srcMacro1, dstFACut, 40 } }));

    p.push_back (mk (cLead, u8 ("Feixe de Harmônicos"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 55 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::sah },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 20 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 25 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cLead, u8 ("Pulso de Fliperama"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 50 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::pulso }, { O (1, "pos"), 20 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 15 }, { F (0, "env"), 30 }, { E (1, "d"), 400 }, { E (1, "s"), 50 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.7f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 25 },
          { id::polyMode, pm::mono }, { id::glide, 25 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 25 }, { srcPressure, dstFACut, 25 }, { srcSlide, dstO2Pos, 50 } }));

    p.push_back (mk (cLead, u8 ("Seno que Grita"),
        { { O (0, "tab"), tb::senoQuad }, { O (0, "pos"), 20 },
          { F (0, "cut"), 5000 }, { F (0, "res"), 25 },
          { E (2, "d"), 800 }, { E (2, "s"), 30 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 },
          { id::polyMode, pm::legato }, { id::glide, 40 }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcPressure, dstO1Pos, 50 }, { srcSlide, dstFACut, 30 } }));

    p.push_back (mk (cLead, u8 ("Ópera Metálica"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 70 }, { O (0, "uni"), 5 }, { O (0, "det"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmMetalico }, { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 6000 }, { F (0, "res"), 20 },
          { id::hypOn, 1 }, { id::hypMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 4 }, { id::revMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyFb, 30 }, { id::dlyMix, 18 }, { id::volume, -11 } },
        { { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 30 }, { srcMacro1, dstO2Level, 50 } }));

    // =====================================================================================================
    // PLUCK
    // =====================================================================================================
    p.push_back (mk (cPluck, u8 ("Gota de Vidro"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 30 },
          { F (0, "cut"), 800 }, { F (0, "res"), 20 }, { F (0, "env"), 60 }, { E (1, "d"), 220 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 0 }, { E (0, "r"), 400 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -5 } },
        { { srcMacro1, dstFACut, 35 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cPluck, u8 ("Corda Solta"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 70 },
          { F (0, "cut"), 4000 }, { F (0, "key"), 40 }, { F (0, "env"), 30 }, { E (1, "d"), 300 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1200 }, { E (0, "s"), 0 }, { E (0, "r"), 500 },
          { E (2, "d"), 500 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 }, { id::volume, -5 } },
        { { srcEnv3, dstO1Pos, -60 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Leque de Serras"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 5 }, { O (0, "det"), 25 },
          { F (0, "cut"), 400 }, { F (0, "res"), 20 }, { F (0, "env"), 70 }, { E (1, "d"), 180 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 35 }, { id::dlyMix, 22 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -9 } },
        { { srcMacro1, dstFACut, 30 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cPluck, u8 ("Chuva Ácida"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 30 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 600 }, { F (0, "res"), 40 }, { F (0, "env"), 55 }, { E (1, "d"), 150 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 350 }, { E (0, "s"), 0 }, { E (0, "r"), 250 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 45 }, { id::dlyMix, 28 }, { id::volume, -6 } },
        { { srcRandom, dstO1Pos, 20 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cPluck, u8 ("Harpa Elétrica"),
        { { O (0, "tab"), tb::eletrico }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::tri }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 15 }, { F (0, "env"), 50 }, { E (1, "d"), 300 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 600 },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 }, { id::volume, -7 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Pingo Metálico"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { F (0, "env"), 40 }, { E (1, "d"), 150 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 350 }, { E (0, "s"), 0 }, { E (0, "r"), 250 },
          { E (2, "d"), 120 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyMix, 20 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 35 } }));

    p.push_back (mk (cPluck, u8 ("Estalo de Cristal"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 70 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 45 },
          { F (0, "cut"), 2000 }, { F (0, "env"), 40 }, { E (1, "d"), 100 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 0 }, { E (0, "r"), 500 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 70 }, { id::revDecay, 3.5f }, { id::revMix, 30 },
          { id::volume, -6 } },
        { { srcVel, dstO2Level, 40 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cPluck, u8 ("Bolha de Sabão"),
        { { O (0, "pos"), cl::tri },
          { F (0, "cut"), 300 }, { F (0, "res"), 65 }, { F (0, "env"), 65 }, { E (1, "d"), 120 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 200 }, { id::volume, -6 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstFARes, 25 } }));

    p.push_back (mk (cPluck, u8 ("Cavaco de Silício"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::corda }, { O (1, "pos"), 40 }, { O (1, "fine"), 7 },
          { F (0, "cut"), 5000 }, { F (0, "key"), 50 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 700 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::choOn, 1 }, { id::choMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 }, { id::volume, -7 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPluck, u8 ("Koto de Fibra"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 10 }, { O (0, "warp"), wp::dobraMais }, { O (0, "wamt"), 25 },
          { F (0, "cut"), 3500 }, { F (0, "env"), 25 }, { E (1, "d"), 250 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1000 }, { E (0, "s"), 0 }, { E (0, "r"), 500 },
          { E (2, "d"), 300 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -5 } },
        { { srcEnv3, dstO1Warp, 40 }, { srcMacro1, dstO1Warp, 50 } }));

    p.push_back (mk (cPluck, u8 ("Mordida de Quimera"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 50 },
          { F (0, "cut"), 900 }, { F (0, "res"), 25 }, { F (0, "env"), 55 }, { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 450 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "d"), 250 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcMacro1, dstO1Pos, 40 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cPluck, u8 ("Grão de Areia"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 40 },
          { F (0, "cut"), 1000 }, { F (0, "res"), 25 }, { F (0, "env"), 50 }, { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 40 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 25 }, { id::volume, -5 } },
        { { srcRandom, dstO1Pos, 30 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Pulo do Gato"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 60 },
          { F (0, "cut"), 500 }, { F (0, "res"), 35 }, { F (0, "env"), 60 }, { E (1, "d"), 160 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 280 }, { E (0, "s"), 0 }, { E (0, "r"), 150 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 35 }, { id::dlyMix, 20 }, { id::volume, -6 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstO1Pos, 35 } }));

    p.push_back (mk (cPluck, u8 ("Pizzicato Fantasma"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 5 },
          { F (0, "cut"), 700 }, { F (0, "res"), 10 }, { F (0, "env"), 50 }, { F (0, "key"), 40 }, { E (1, "d"), 250 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 250 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 3 }, { id::revMix, 35 }, { id::volume, -7 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Gotejo Digital"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 70 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 1200 }, { F (0, "res"), 30 }, { F (0, "env"), 40 }, { E (1, "d"), 120 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "d"), 200 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 50 }, { id::dlyMix, 22 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, -50 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Sílaba"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 50 },
          { F (0, "cut"), 900 }, { F (0, "env"), 50 }, { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 450 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "d"), 300 },
          { id::choOn, 1 }, { id::choMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcRandom, dstO1Pos, 15 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPluck, u8 ("Dedilhado Noturno"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 2500 }, { F (0, "env"), 30 }, { E (1, "d"), 400 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1500 }, { E (0, "s"), 0 }, { E (0, "r"), 800 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d38 }, { id::dlyMix, 15 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 75 }, { id::revDecay, 5 }, { id::revMix, 35 },
          { id::volume, -6 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Faísca Larga"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 50 }, { O (0, "uni"), 3 },
          { F (0, "cut"), 500 }, { F (0, "env"), 65 }, { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 450 }, { E (0, "s"), 0 }, { E (0, "r"), 350 },
          { id::hypOn, 1 }, { id::hypMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 }, { id::volume, -9 } },
        { { srcVel, dstFACut, 20 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cPluck, u8 ("Mola Analógica"),
        { { O (0, "pos"), cl::quadrada },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 250 }, { F (0, "res"), 55 }, { F (0, "drv"), 20 }, { F (0, "env"), 75 }, { E (1, "d"), 140 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 120 }, { id::volume, -9 } },
        { { srcVel, dstFACut, 25 }, { srcMacro1, dstFARes, 25 } }));

    p.push_back (mk (cPluck, u8 ("Vagalume"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 60 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal }, { O (1, "pos"), 20 }, { O (1, "lvl"), 40 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 6000 }, { F (0, "env"), 30 }, { E (1, "d"), 80 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 220 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8t }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 }, { id::volume, -5 } },
        { { srcRandom, dstO1Pos, 25 }, { srcMacro1, dstO2Level, 50 } }));

    // =====================================================================================================
    // PAD
    // =====================================================================================================
    p.push_back (mk (cPad, u8 ("Neblina Quente"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 40 }, { O (0, "uni"), 4 }, { O (0, "det"), 25 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 8 }, { O (1, "uni"), 3 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 15 }, { F (0, "env"), 20 },
          { E (1, "a"), 1500 }, { E (1, "d"), 3000 }, { E (1, "s"), 50 },
          { E (0, "a"), 1200 }, { E (0, "d"), 1000 }, { E (0, "s"), 90 }, { E (0, "r"), 2500 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.08f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 75 }, { id::revDecay, 5 }, { id::revMix, 35 },
          { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cPad, u8 ("Catedral Digital"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal }, { O (1, "pos"), 30 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 },
          { F (0, "cut"), 9000 }, { F (0, "res"), 5 },
          { E (0, "a"), 1800 }, { E (0, "s"), 100 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.05f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::tri },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.07f }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 90 }, { id::revDecay, 8 }, { id::revMix, 40 },
          { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstO2Pos, 40 }, { srcPressure, dstO2Level, 40 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Vidro Respirando"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 20 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 4000 },
          { E (0, "a"), 1500 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.1f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.13f }, { L (1, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcLfo2, dstFACut, 15 }, { srcPressure, dstO1Pos, 30 }, { srcSlide, dstFACut, 30 } }));

    p.push_back (mk (cPad, u8 ("Tapete Analógico"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 10 },
          { O (2, "on"), 1 }, { O (2, "tab"), tb::classica }, { O (2, "pos"), cl::serra }, { O (2, "fine"), -10 }, { O (2, "oct"), -1 },
          { O (2, "lvl"), 50 },
          { F (0, "cut"), 1200 }, { F (0, "res"), 20 }, { F (0, "env"), 25 },
          { E (1, "a"), 800 }, { E (1, "d"), 2000 }, { E (1, "s"), 60 },
          { E (0, "a"), 700 }, { E (0, "s"), 90 }, { E (0, "r"), 2000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.2f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choDepth, 50 }, { id::choMix, 45 },
          { id::drift, 40 }, { id::volume, -10 } },
        { { srcLfo1, dstFACut, 10 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstFARes, 25 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cPad, u8 ("Coral de Máquinas"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 }, { O (0, "uni"), 3 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::formante }, { O (1, "pos"), 50 }, { O (1, "lvl"), 50 },
          { E (0, "a"), 1000 }, { E (0, "r"), 2500 },
          { L (0, "div"), dv::c4 }, { L (0, "shape"), sh::tri }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcSlide, dstO1Pos, 50 }, { srcPressure, dstFACut, 25 }, { srcMacro1, dstO2Pos, 50 } }));

    p.push_back (mk (cPad, u8 ("Aurora Austral"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 10 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::espectral }, { O (1, "pos"), 30 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 2500 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.04f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.09f }, { L (1, "mode"), lm::livre },
          { id::hypOn, 1 }, { id::hypMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 85 }, { id::revDecay, 7 }, { id::revMix, 40 },
          { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 60 }, { srcLfo2, dstO2Pos, 40 }, { srcPressure, dstFACut, -30 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Pulsação Lenta"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 30 }, { O (0, "uni"), 2 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::pulso }, { O (1, "pos"), 60 }, { O (1, "fine"), 9 },
          { F (0, "cut"), 2000 }, { F (0, "res"), 10 },
          { E (0, "a"), 600 }, { E (0, "r"), 1800 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.3f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 50 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO2Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Mar de Granito"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::granito }, { O (1, "pos"), 70 }, { O (1, "semi"), 7 }, { O (1, "lvl"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 },
          { E (0, "a"), 1500 }, { E (0, "r"), 3500 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.06f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.11f }, { L (1, "mode"), lm::livre }, { L (1, "shape"), sh::suave },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstO2Pos, 40 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Seda"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 6 }, { O (0, "det"), 20 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 10 },
          { E (0, "a"), 400 }, { E (0, "r"), 1500 },
          { id::choOn, 1 }, { id::choMix, 40 },
          { id::hypOn, 1 }, { id::hypMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -11 } },
        { { srcPressure, dstFACut, 25 }, { srcSlide, dstDetune, 25 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cPad, u8 ("Inverno Metálico"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 15 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::tri }, { O (1, "lvl"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 },
          { E (0, "a"), 1200 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.07f }, { L (0, "mode"), lm::livre },
          { id::phsOn, 1 }, { id::phsRate, 0.1f }, { id::phsMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 25 }, { srcPressure, dstO1Pos, 30 }, { srcSlide, dstFACut, 30 }, { srcMacro1, dstO1Level, -50 } }));

    p.push_back (mk (cPad, u8 ("Câmara Lenta"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::serraDupla }, { O (1, "pos"), 70 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 1800 }, { F (0, "env"), 30 },
          { E (1, "a"), 3000 }, { E (1, "d"), 4000 }, { E (1, "s"), 40 },
          { E (0, "a"), 2000 }, { E (0, "s"), 100 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.05f }, { L (0, "mode"), lm::livre },
          { id::phsOn, 1 }, { id::phsRate, 0.08f }, { id::phsDepth, 70 }, { id::phsMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO2Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Nuvem Elétrica"),
        { { O (0, "tab"), tb::eletrico }, { O (0, "pos"), 30 }, { O (0, "uni"), 4 }, { O (0, "det"), 30 },
          { F (0, "cut"), 1600 }, { F (0, "res"), 20 },
          { E (0, "a"), 800 }, { E (0, "r"), 2500 },
          { L (0, "div"), dv::c2 }, { L (0, "shape"), sh::tri }, { L (0, "mode"), lm::livre },
          { L (1, "div"), dv::c1 }, { L (1, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcLfo2, dstFACut, 15 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstDetune, 30 } }));

    p.push_back (mk (cPad, u8 ("Órbita Baixa"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 8000 }, { F (0, "res"), 5 },
          { E (0, "a"), 2000 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.03f }, { L (0, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 40 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -6 } },
        { { srcLfo1, dstO1Pos, 70 }, { srcSlide, dstO1Pos, 50 }, { srcPressure, dstO1Level, 30 } }));

    p.push_back (mk (cPad, u8 ("Veludo Escuro"),
        { { O (0, "pos"), 50 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), 50 }, { O (1, "oct"), -1 },
          { F (0, "cut"), 600 }, { F (0, "res"), 25 }, { F (0, "key"), 30 },
          { E (0, "a"), 900 }, { E (0, "r"), 2500 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.1f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 15 }, { srcPressure, dstFACut, 40 }, { srcSlide, dstFARes, 30 }, { srcMacro1, dstFACut, 40 } }));

    p.push_back (mk (cPad, u8 ("Dobra do Tempo"),
        { { O (0, "tab"), tb::triDobrado }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::triDobrado }, { O (1, "pos"), 50 }, { O (1, "fine"), 12 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 10 },
          { E (0, "a"), 1500 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.07f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.12f }, { L (1, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstO2Pos, 40 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 30 } }));

    p.push_back (mk (cPad, u8 ("Vapor Sincronizado"),
        { { O (0, "tab"), tb::sincronizado }, { O (0, "pos"), 10 }, { O (0, "uni"), 3 },
          { F (0, "cut"), 2500 }, { F (0, "res"), 20 },
          { E (0, "a"), 1000 }, { E (0, "r"), 2500 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.08f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyFb, 40 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 35 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Coração de Quimera"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::quimeraTab }, { O (1, "pos"), 60 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 15 },
          { E (0, "a"), 1200 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.04f }, { L (0, "mode"), lm::livre },
          { id::hypOn, 1 }, { id::hypMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcMacro1, dstO1Pos, 50 }, { srcMacro2, dstFACut, 40 },
          { srcPressure, dstO2Level, 40 }, { srcSlide, dstO2Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Campo Espectral"),
        { { O (0, "tab"), tb::espectral }, { O (0, "pos"), 20 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 },
          { E (0, "a"), 2500 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.05f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 90 }, { id::revDecay, 9 }, { id::revMix, 45 },
          { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 60 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cPad, u8 ("Fita Desgastada"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 50 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::tri }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 1400 },
          { E (0, "a"), 500 }, { E (0, "r"), 2000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.4f }, { L (0, "mode"), lm::livre },
          { id::dstOn, 1 }, { id::dstType, ds::downsample }, { id::dstDrive, 25 }, { id::dstMix, 30 },
          { id::choOn, 1 }, { id::choMix, 50 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 30 },
          { id::drift, 70 }, { id::volume, -9 } },
        { { srcLfo1, dstPitch, 1 }, { srcPressure, dstFACut, 30 }, { srcMacro1, dstFACut, -40 } }));

    p.push_back (mk (cPad, u8 ("Horizonte Largo"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 8 }, { O (0, "det"), 40 }, { O (0, "wid"), 100 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal }, { O (1, "pos"), 40 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 10 },
          { E (0, "a"), 800 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.09f }, { L (0, "mode"), lm::livre },
          { id::hypOn, 1 }, { id::hypMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -12 } },
        { { srcLfo1, dstO2Pos, 40 }, { srcPressure, dstFACut, 25 }, { srcSlide, dstO2Level, 50 } }));

    // =====================================================================================================
    // ATMOS
    // =====================================================================================================
    p.push_back (mk (cAtmos, u8 ("Vento Solar"),
        { { O (0, "tab"), tb::espectral }, { O (0, "pos"), 30 }, { O (0, "lvl"), 30 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 60 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1200 }, { F (0, "res"), 50 },
          { E (0, "a"), 2000 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.07f }, { L (0, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 50 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 95 }, { id::revDecay, 12 }, { id::revMix, 50 },
          { id::volume, -8 } },
        { { srcLfo1, dstFACut, 40 }, { srcPressure, dstFACut, 30 }, { srcMacro1, dstNoiseLevel, -50 } }));

    p.push_back (mk (cAtmos, u8 ("Poeira Cósmica"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 50 },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 15 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 4000 },
          { E (0, "a"), 3000 }, { E (0, "r"), 6000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.05f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 6.0f }, { L (1, "mode"), lm::livre }, { L (1, "shape"), sh::sah },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d38 }, { id::dlyFb, 55 }, { id::dlyMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 10 }, { id::revMix, 50 }, { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 80 }, { srcLfo2, dstO1Pan, 40 }, { srcSlide, dstFACut, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Caverna Úmida"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 20 },
          { id::noiseOn, 1 }, { id::noiseColor, 20 }, { id::noiseLvl, 30 },
          { F (0, "cut"), 400 }, { F (0, "res"), 40 },
          { E (0, "a"), 2500 }, { E (0, "r"), 6000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.03f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 15 }, { id::revDamp, 70 }, { id::revMix, 55 },
          { id::volume, -6 } },
        { { srcLfo1, dstFACut, 30 }, { srcPressure, dstFACut, 30 }, { srcMacro1, dstFARes, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Espectro Distante"),
        { { O (0, "tab"), tb::espectral }, { O (0, "pos"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::espectral }, { O (1, "pos"), 50 }, { O (1, "semi"), 7 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 2500 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.02f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.03f }, { L (1, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 60 }, { id::dlyMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 15 }, { id::revMix, 50 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 90 }, { srcLfo2, dstO2Pos, -60 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Rádio Fantasma"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 30 },
          { id::noiseOn, 1 }, { id::noiseColor, 50 }, { id::noiseLvl, 40 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1800 }, { F (0, "res"), 60 },
          { E (0, "a"), 800 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 3.0f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::sah },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.2f }, { L (1, "mode"), lm::livre }, { L (1, "shape"), sh::suave },
          { id::dstOn, 1 }, { id::dstType, ds::downsample }, { id::dstDrive, 40 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 50 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 40 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcLfo2, dstFACut, 30 }, { srcMacro1, dstNoiseLevel, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Maré de Ferro"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::metal }, { O (1, "pos"), 40 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 30 }, { id::noiseLvl, 15 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 30 },
          { E (0, "a"), 2500 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.04f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.06f }, { L (1, "mode"), lm::livre },
          { id::phsOn, 1 }, { id::phsRate, 0.05f }, { id::phsMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 10 }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstFACut, 25 }, { srcPressure, dstO2Level, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Floresta Digital"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 70 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 20 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { F (0, "res"), 30 },
          { E (0, "a"), 500 }, { E (0, "r"), 3000 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::sah }, { L (0, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 60 }, { id::dlyMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 30 }, { srcLfo1, dstFACut, 25 }, { srcRandom, dstO1Pos, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Respiração da Máquina"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 40 }, { id::noiseLvl, 70 },
          { F (0, "cut"), 600 }, { F (0, "res"), 50 },
          { E (0, "a"), 1500 }, { E (0, "r"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.15f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::tri },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -6 } },
        { { srcLfo1, dstFACut, 50 }, { srcLfo1, dstAmp, 30 }, { srcPressure, dstFARes, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Abismo"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 10 }, { O (0, "oct"), -2 },
          { id::subOn, 1 }, { id::subOct, 1 }, { id::subLvl, 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 10 }, { id::noiseLvl, 20 },
          { F (0, "cut"), 300 }, { F (0, "res"), 30 },
          { E (0, "a"), 3000 }, { E (0, "r"), 6000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.02f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 100 }, { id::revDecay, 20 }, { id::revMix, 50 },
          { id::volume, -7 } },
        { { srcLfo1, dstFACut, 30 }, { srcMacro1, dstFACut, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Satélite Perdido"),
        { { O (0, "pos"), cl::seno },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 },
          { E (0, "a"), 10 }, { E (0, "d"), 400 }, { E (0, "s"), 50 }, { E (0, "r"), 2000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 2.0f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::sah },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d38 }, { id::dlyFb, 70 }, { id::dlyMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 45 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pitch, 25 }, { srcMacro1, dstLfo1Rate, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Neve Estática"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 60 }, { O (0, "lvl"), 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 85 }, { id::noiseLvl, 45 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 6000 },
          { E (0, "a"), 2000 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.08f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.1f }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 9 }, { id::revMix, 50 }, { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcLfo2, dstFACut, 20 }, { srcSlide, dstNoiseLevel, -40 } }));

    p.push_back (mk (cAtmos, u8 ("Vozes no Corredor"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::formante }, { O (1, "pos"), 20 }, { O (1, "semi"), -5 }, { O (1, "lvl"), 40 },
          { F (0, "type"), ft::formante }, { F (0, "cut"), 1200 }, { F (0, "res"), 35 },
          { E (0, "a"), 2000 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.06f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.09f }, { L (1, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 55 }, { id::dlyMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 10 }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 60 }, { srcLfo2, dstO2Pos, 50 }, { srcLfo2, dstFACut, 20 }, { srcSlide, dstO1Pos, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Tempestade Elétrica"),
        { { O (0, "tab"), tb::escova }, { O (0, "pos"), 60 }, { O (0, "lvl"), 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 50 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 45 },
          { E (0, "a"), 1500 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 8.0f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::sah },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 30 }, { id::dstMix, 30 },
          { id::phsOn, 1 }, { id::phsRate, 0.2f }, { id::phsMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -11 } },
        { { srcLfo1, dstFACut, 35 }, { srcMacro1, dstLfo1Rate, 40 }, { srcPressure, dstNoiseLevel, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Deriva Continental"),
        { { O (0, "tab"), tb::serraDupla }, { O (0, "pos"), 50 }, { O (0, "uni"), 4 }, { O (0, "det"), 40 },
          { F (0, "cut"), 800 }, { F (0, "res"), 20 },
          { E (0, "a"), 3000 }, { E (0, "r"), 6000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.03f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.05f }, { L (1, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 14 }, { id::revMix, 45 },
          { id::drift, 100 }, { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcLfo2, dstDetune, 30 }, { srcPressure, dstFACut, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Cristais Submersos"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 60 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 },
          { F (0, "cut"), 2500 },
          { E (0, "a"), 2000 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.07f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.11f }, { L (1, "mode"), lm::livre },
          { id::phsOn, 1 }, { id::phsRate, 0.06f }, { id::phsMix, 40 },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 12 }, { id::revDamp, 60 }, { id::revMix, 50 },
          { id::volume, -7 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcLfo2, dstO2Pos, 40 }, { srcSlide, dstFACut, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Sopro Espectral"),
        { { O (0, "tab"), tb::espectral }, { O (0, "pos"), 60 },
          { id::noiseOn, 1 }, { id::noiseColor, 50 }, { id::noiseLvl, 40 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1000 }, { F (0, "res"), 65 },
          { E (0, "a"), 2000 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.04f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.05f }, { L (1, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 45 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 10 }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstFACut, 45 }, { srcPressure, dstFARes, 25 } }));

    p.push_back (mk (cAtmos, u8 ("Interferência"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 50 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 3000 }, { F (0, "res"), 30 },
          { E (0, "a"), 300 }, { E (0, "r"), 2000 },
          { L (0, "div"), dv::d32 }, { L (0, "shape"), sh::sah }, { L (0, "mode"), lm::livre },
          { L (1, "div"), dv::d16 }, { L (1, "shape"), sh::sah }, { L (1, "mode"), lm::livre },
          { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 40 }, { id::dstMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16t }, { id::dlyFb, 45 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 30 }, { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstFACut, 30 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cAtmos, u8 ("Planeta Oco"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 40 }, { O (0, "oct"), -1 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmLinear }, { O (1, "pos"), 30 }, { O (1, "oct"), -1 }, { O (1, "fine"), 8 },
          { O (1, "lvl"), 40 },
          { F (0, "type"), ft::pente }, { F (0, "cut"), 200 }, { F (0, "res"), 70 },
          { E (0, "a"), 2500 }, { E (0, "r"), 5000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.04f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 16 }, { id::revMix, 50 }, { id::volume, -11 } },
        { { srcLfo1, dstFACut, 40 }, { srcSlide, dstFACut, 30 }, { srcMacro1, dstFARes, 25 } }));

    p.push_back (mk (cAtmos, u8 ("Chuva de Silício"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 80 }, { O (0, "lvl"), 30 },
          { id::noiseOn, 1 }, { id::noiseColor, 90 }, { id::noiseLvl, 30 },
          { E (0, "a"), 1000 }, { E (0, "r"), 4000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.0f }, { L (0, "mode"), lm::livre }, { L (0, "shape"), sh::sah },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 60 }, { id::dlyMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcLfo1, dstO1Pos, 40 }, { srcMacro1, dstLfo1Rate, 40 }, { srcPressure, dstNoiseLevel, 30 } }));

    p.push_back (mk (cAtmos, u8 ("Eclipse"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 50 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::quimeraTab }, { O (1, "pos"), 90 }, { O (1, "semi"), 7 }, { O (1, "lvl"), 30 },
          { id::subOn, 1 }, { id::subLvl, 30 },
          { id::noiseOn, 1 }, { id::noiseColor, 30 }, { id::noiseLvl, 15 },
          { F (0, "cut"), 1800 }, { F (0, "res"), 25 },
          { E (0, "a"), 3000 }, { E (0, "r"), 6000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.02f }, { L (0, "mode"), lm::livre },
          { L (1, "sync"), 0 }, { L (1, "rate"), 0.03f }, { L (1, "mode"), lm::livre },
          { id::hypOn, 1 }, { id::hypMix, 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d2 }, { id::dlyFb, 50 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 18 }, { id::revMix, 50 }, { id::volume, -10 } },
        { { srcLfo1, dstO1Pos, 50 }, { srcLfo2, dstFACut, 35 }, { srcPressure, dstFACut, 30 }, { srcSlide, dstO1Pos, 40 } }));

    // =====================================================================================================
    // PERC
    // =====================================================================================================
    p.push_back (mk (cPerc, u8 ("Sino de Templo"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 8000 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 2500 }, { E (0, "s"), 0 }, { E (0, "r"), 2500 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -6 } },
        { { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPerc, u8 ("Marimba de Neon"),
        { { O (0, "pos"), cl::seno },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 20 }, { O (1, "oct"), 2 }, { O (1, "lvl"), 25 },
          { F (0, "cut"), 3000 }, { F (0, "env"), 40 }, { E (1, "d"), 100 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "d"), 60 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 }, { id::volume, -5 } },
        { { srcEnv3, dstO2Level, 50 }, { srcVel, dstFACut, 25 } }));

    p.push_back (mk (cPerc, u8 ("Vibrafone de Ar"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "lvl"), 50 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1800 }, { E (0, "s"), 0 }, { E (0, "r"), 1500 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.0f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -6 } },
        { { srcLfo1, dstAmp, 25 }, { srcVel, dstO1Pos, 25 }, { srcWheel, dstLfo1Rate, 30 } }));

    p.push_back (mk (cPerc, u8 ("Gongo de Aço"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 60 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::metal }, { O (1, "pos"), 50 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 4000 }, { E (0, "s"), 0 }, { E (0, "r"), 4000 },
          { E (2, "d"), 1500 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -8 } },
        { { srcEnv3, dstO1Pos, 30 }, { srcVel, dstO1Pos, 20 } }));

    p.push_back (mk (cPerc, u8 ("Bigorna"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 70 },
          { id::noiseOn, 1 }, { id::noiseColor, 70 }, { id::noiseLvl, 15 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 }, { F (0, "env"), 50 }, { E (1, "d"), 60 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 }, { id::volume, -7 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstO1Pos, 30 } }));

    p.push_back (mk (cPerc, u8 ("Kalimba de Cobre"),
        { { O (0, "pos"), cl::seno },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::harmonicos }, { O (1, "pos"), 25 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 0 }, { E (0, "r"), 400 },
          { E (2, "d"), 30 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 15 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 }, { id::volume, -5 } },
        { { srcEnv3, dstO2Level, 60 }, { srcVel, dstO2Level, 25 } }));

    p.push_back (mk (cPerc, u8 ("Tubo de Ensaio"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 80 },
          { F (0, "cut"), 6000 }, { F (0, "key"), 50 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 900 }, { E (0, "s"), 0 }, { E (0, "r"), 800 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 }, { id::volume, -5 } },
        { { srcVel, dstFACut, 25 }, { srcRandom, dstO1Pos, 10 } }));

    p.push_back (mk (cPerc, u8 ("Gamelão Elétrico"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 70 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 30 }, { O (1, "fine"), 15 }, { O (1, "lvl"), 50 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1500 }, { E (0, "s"), 0 }, { E (0, "r"), 1500 },
          { id::choOn, 1 }, { id::choMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -8 } },
        { { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO2Pos, 40 } }));

    p.push_back (mk (cPerc, u8 ("Lata de Tinta"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmMetalico }, { O (1, "pos"), 60 }, { O (1, "semi"), 5 }, { O (1, "lvl"), 50 },
          { F (0, "type"), ft::pa12 }, { F (0, "cut"), 400 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 180 }, { E (0, "s"), 0 }, { E (0, "r"), 150 },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 20 }, { id::dstMix, 50 }, { id::volume, -8 } },
        { { srcVel, dstO2Level, 30 }, { srcRandom, dstO1Pos, 15 } }));

    p.push_back (mk (cPerc, u8 ("Xilo de Osso"),
        { { O (0, "pos"), cl::tri },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), 2 }, { O (1, "lvl"), 25 },
          { F (0, "cut"), 2500 }, { F (0, "env"), 40 }, { E (1, "d"), 80 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 250 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 }, { id::volume, -5 } },
        { { srcVel, dstFACut, 30 } }));

    p.push_back (mk (cPerc, u8 ("Brinde"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 50 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 80 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 2000 }, { E (0, "s"), 0 }, { E (0, "r"), 2000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 4.0f }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -6 } },
        { { srcLfo1, dstAmp, 10 }, { srcVel, dstO2Level, 30 } }));

    p.push_back (mk (cPerc, u8 ("Campainha do Futuro"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 60 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 800 }, { E (0, "s"), 0 }, { E (0, "r"), 600 },
          { E (2, "d"), 400 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 20 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 20 } }));

    p.push_back (mk (cPerc, u8 ("Sino ao Contrário"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 50 },
          { E (0, "a"), 600 }, { E (0, "d"), 100 }, { E (0, "s"), 0 }, { E (0, "r"), 300 }, { E (0, "c"), 60 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -6 } },
        { { srcEnv1, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPerc, u8 ("Mola de Relógio"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "d"), 150 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 40 }, { id::dlyMix, 20 }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pitch, 20 }, { srcVel, dstO1Pos, 25 } }));

    p.push_back (mk (cPerc, u8 ("Gota na Panela"),
        { { O (0, "pos"), cl::seno },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmMetalico }, { O (1, "pos"), 30 }, { O (1, "lvl"), 25 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "d"), 40 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 }, { id::volume, -5 } },
        { { srcEnv3, dstPitch, 30 }, { srcVel, dstO2Level, 30 } }));

    p.push_back (mk (cPerc, u8 ("Tambor de Aço"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { F (0, "cut"), 3000 }, { F (0, "env"), 40 }, { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 700 }, { E (0, "s"), 0 }, { E (0, "r"), 500 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 }, { id::volume, -6 } },
        { { srcVel, dstFACut, 25 }, { srcVel, dstO1Pos, 15 } }));

    p.push_back (mk (cPerc, u8 ("Celesta de Gelo"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 40 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::cristal }, { O (1, "pos"), 10 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1200 }, { E (0, "s"), 0 }, { E (0, "r"), 1000 },
          { id::choOn, 1 }, { id::choMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -5 } },
        { { srcVel, dstO2Level, 30 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPerc, u8 ("Prato Quebrado"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 90 }, { O (0, "uni"), 3 }, { O (0, "det"), 50 },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 40 },
          { F (0, "type"), ft::pa12 }, { F (0, "cut"), 3000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1500 }, { E (0, "s"), 0 }, { E (0, "r"), 1500 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 }, { id::volume, -9 } },
        { { srcVel, dstNoiseLevel, 30 } }));

    p.push_back (mk (cPerc, u8 ("Pêndulo"),
        { { O (0, "tab"), tb::sinos }, { O (0, "pos"), 20 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 0 }, { E (0, "r"), 400 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 55 }, { id::dlyMix, 35 }, { id::volume, -6 } },
        { { srcVel, dstO1Pos, 30 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cPerc, u8 ("Ferradura"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 90 },
          { F (0, "cut"), 4000 }, { F (0, "env"), 30 }, { E (1, "d"), 100 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 350 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "d"), 120 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, -50 }, { srcRandom, dstO1Pos, 15 }, { srcVel, dstFACut, 20 } }));

    // =====================================================================================================
    // BATERIA (sustain 0; bumbo e chimbal em mono para cortar a nota anterior)
    // =====================================================================================================
    p.push_back (mk (cBateria, u8 ("Bumbo Profundo"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { F (0, "cut"), 4000 }, { F (0, "res"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 450 }, { E (0, "s"), 0 }, { E (0, "r"), 120 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 70 }, { E (2, "s"), 0 }, { E (2, "r"), 50 },
          { id::dstOn, 1 }, { id::dstDrive, 20 }, { id::dstMix, 50 },
          { id::polyMode, pm::mono }, { id::drift, 0 }, { id::velAmp, 60 }, { id::volume, -4 } },
        { { srcEnv3, dstPitch, 85 }, { srcMacro1, dstPitch, 10 } }));

    p.push_back (mk (cBateria, u8 ("Bumbo Longo de Rua"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { F (0, "cut"), 3000 }, { F (0, "res"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1800 }, { E (0, "s"), 0 }, { E (0, "r"), 400 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 50 }, { E (2, "s"), 0 }, { E (2, "r"), 50 },
          { id::dstOn, 1 }, { id::dstDrive, 25 }, { id::dstMix, 40 },
          { id::polyMode, pm::mono }, { id::glide, 30 }, { id::drift, 0 }, { id::velAmp, 50 }, { id::volume, -5 } },
        { { srcEnv3, dstPitch, 50 }, { srcMacro1, dstFACut, -30 } }));

    p.push_back (mk (cBateria, u8 ("Bumbo de Concreto"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { F (0, "cut"), 5000 }, { F (0, "res"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 80 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 40 }, { E (2, "s"), 0 }, { E (2, "r"), 40 },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 50 }, { id::dstMix, 60 },
          { id::cmpOn, 1 }, { id::cmpAmt, 50 },
          { id::polyMode, pm::mono }, { id::drift, 0 }, { id::velAmp, 60 }, { id::volume, -7 } },
        { { srcEnv3, dstPitch, 95 }, { srcMacro1, dstPitch, 10 } }));

    p.push_back (mk (cBateria, u8 ("Caixa de Feira"),
        { { O (0, "pos"), cl::tri }, { O (0, "lvl"), 60 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 70 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 7000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 220 }, { E (0, "s"), 0 }, { E (0, "r"), 150 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 40 }, { E (2, "s"), 0 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 },
          { id::voices, 4 }, { id::drift, 0 }, { id::velAmp, 70 }, { id::volume, -6 } },
        { { srcEnv3, dstPitch, 25 }, { srcMacro1, dstNoiseLevel, 30 } }));

    p.push_back (mk (cBateria, u8 ("Caixa de Silício"),
        { { O (0, "tab"), tb::senoQuad }, { O (0, "pos"), 40 }, { O (0, "lvl"), 55 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 75 }, { id::noiseLvl, 80 },
          { F (0, "type"), ft::ladder12 }, { F (0, "cut"), 5000 }, { F (0, "env"), 30 }, { E (1, "d"), 80 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 180 }, { E (0, "s"), 0 }, { E (0, "r"), 120 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 30 }, { E (2, "s"), 0 },
          { id::dstOn, 1 }, { id::dstDrive, 30 }, { id::dstMix, 40 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 },
          { id::voices, 4 }, { id::drift, 0 }, { id::velAmp, 70 }, { id::volume, -7 } },
        { { srcEnv3, dstPitch, 30 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cBateria, u8 ("Chimbal Fechadinho"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 90 }, { id::noiseLvl, 80 }, { id::noiseRt, rt::filtroB },
          { F (0, "on"), 0 },
          { F (1, "on"), 1 }, { F (1, "type"), ft::pa12 }, { F (1, "cut"), 8000 }, { F (1, "res"), 20 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 50 }, { E (0, "s"), 0 }, { E (0, "r"), 40 },
          { id::polyMode, pm::mono }, { id::velAmp, 80 }, { id::volume, -6 } },
        { { srcMacro1, dstFBCut, 20 }, { srcVel, dstFBCut, 10 } }));

    p.push_back (mk (cBateria, u8 ("Chimbal Escancarado"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 90 }, { O (0, "lvl"), 20 }, { O (0, "rt"), rt::filtroB },
          { id::noiseOn, 1 }, { id::noiseColor, 85 }, { id::noiseLvl, 80 }, { id::noiseRt, rt::filtroB },
          { F (0, "on"), 0 },
          { F (1, "on"), 1 }, { F (1, "type"), ft::pa12 }, { F (1, "cut"), 7000 }, { F (1, "res"), 15 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::polyMode, pm::mono }, { id::velAmp, 80 }, { id::volume, -7 } },
        { { srcMacro1, dstFBCut, 20 }, { srcVel, dstFBCut, 10 } }));

    p.push_back (mk (cBateria, u8 ("Chimbal de Lata"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 80 }, { O (0, "oct"), 2 }, { O (0, "lvl"), 40 }, { O (0, "rt"), rt::filtroB },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 60 }, { id::noiseRt, rt::filtroB },
          { F (0, "on"), 0 },
          { F (1, "on"), 1 }, { F (1, "type"), ft::pa12 }, { F (1, "cut"), 6500 }, { F (1, "res"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 90 }, { E (0, "s"), 0 }, { E (0, "r"), 60 },
          { id::polyMode, pm::mono }, { id::velAmp, 80 }, { id::volume, -7 } },
        { { srcRandom, dstO1Pos, 10 }, { srcMacro1, dstFBCut, 20 } }));

    p.push_back (mk (cBateria, u8 ("Palmas na Laje"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 90 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1300 }, { F (0, "res"), 35 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 160 }, { E (0, "s"), 0 }, { E (0, "r"), 180 }, { E (0, "c"), -60 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 40.0f }, { L (0, "shape"), sh::quadrada },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revSize, 35 }, { id::revMix, 30 },
          { id::voices, 4 }, { id::velAmp, 70 }, { id::volume, -6 } },
        { { srcLfo1, dstAmp, 40 }, { srcMacro1, dstFACut, 25 } }));

    p.push_back (mk (cBateria, u8 ("Palmas de Estádio"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 55 }, { id::noiseLvl, 90 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1100 }, { F (0, "res"), 25 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 250 }, { E (0, "c"), -50 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 33.0f }, { L (0, "shape"), sh::quadrada },
          { id::hypOn, 1 }, { id::hypMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revSize, 60 }, { id::revMix, 40 },
          { id::voices, 4 }, { id::velAmp, 70 }, { id::volume, -7 } },
        { { srcLfo1, dstAmp, 35 }, { srcRandom, dstFACut, 10 } }));

    p.push_back (mk (cBateria, u8 ("Tom de Barro"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 30 }, { id::noiseLvl, 10 },
          { F (0, "cut"), 2000 }, { F (0, "res"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 120 }, { E (2, "s"), 0 },
          { id::voices, 4 }, { id::drift, 0 }, { id::velAmp, 60 }, { id::volume, -5 } },
        { { srcEnv3, dstPitch, 30 }, { srcVel, dstPitch, 3 } }));

    p.push_back (mk (cBateria, u8 ("Tom de Fliperama"),
        { { O (0, "pos"), cl::tri }, { O (0, "rand"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 150 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 200 }, { E (2, "s"), 0 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 },
          { id::voices, 4 }, { id::drift, 0 }, { id::velAmp, 60 }, { id::volume, -5 } },
        { { srcEnv3, dstPitch, 45 }, { srcMacro1, dstFACut, -30 } }));

    p.push_back (mk (cBateria, u8 ("Aro Estalado"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 50 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 70 }, { id::noiseLvl, 40 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 2500 }, { F (0, "res"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 40 }, { E (0, "s"), 0 }, { E (0, "r"), 30 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 15 }, { E (2, "s"), 0 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 },
          { id::voices, 4 }, { id::velAmp, 70 }, { id::volume, -6 } },
        { { srcEnv3, dstPitch, 30 } }));

    p.push_back (mk (cBateria, u8 ("Sineta de Pasto"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 0 }, { O (0, "rand"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::pulso }, { O (1, "pos"), 0 }, { O (1, "semi"), 7 }, { O (1, "fine"), -20 },
          { O (1, "rand"), 0 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 1500 }, { F (0, "res"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 100 }, { E (0, "c"), -40 },
          { id::voices, 4 }, { id::drift, 0 }, { id::velAmp, 70 }, { id::volume, -8 } },
        { { srcVel, dstFACut, 15 } }));

    p.push_back (mk (cBateria, u8 ("Prato de Ataque Torto"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 90 }, { O (0, "oct"), 2 }, { O (0, "lvl"), 40 },
          { O (0, "uni"), 4 }, { O (0, "det"), 60 },
          { id::noiseOn, 1 }, { id::noiseColor, 85 }, { id::noiseLvl, 70 },
          { F (0, "type"), ft::pa12 }, { F (0, "cut"), 4000 }, { F (0, "res"), 10 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1800 }, { E (0, "s"), 0 }, { E (0, "r"), 1500 }, { E (0, "c"), -40 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 },
          { id::voices, 4 }, { id::velAmp, 70 }, { id::volume, -9 } },
        { { srcMacro1, dstFACut, 25 } }));

    p.push_back (mk (cBateria, u8 ("Ganzá"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 70 },
          { F (0, "type"), ft::pa12 }, { F (0, "cut"), 5000 }, { F (0, "res"), 10 },
          { E (0, "a"), 15 }, { E (0, "d"), 90 }, { E (0, "s"), 0 }, { E (0, "r"), 60 },
          { id::voices, 4 }, { id::velAmp, 90 }, { id::volume, -8 } },
        { { srcVel, dstFACut, 15 }, { srcRandom, dstFACut, 8 } }));

    p.push_back (mk (cBateria, u8 ("Bumbo Rasgado"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 500 }, { E (0, "s"), 0 }, { E (0, "r"), 100 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 60 }, { E (2, "s"), 0 },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 70 }, { id::dstTone, 40 }, { id::dstMix, 80 },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 50 },
          { id::polyMode, pm::mono }, { id::drift, 0 }, { id::velAmp, 50 }, { id::volume, -10 } },
        { { srcEnv3, dstPitch, 90 }, { srcMacro1, dstPitch, 10 } }));

    p.push_back (mk (cBateria, u8 ("Atabaque"),
        { { O (0, "pos"), cl::seno }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 50 }, { id::noiseLvl, 8 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 120 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 25 }, { E (2, "s"), 0 },
          { id::voices, 6 }, { id::drift, 5 }, { id::velAmp, 70 }, { id::volume, -5 } },
        { { srcEnv3, dstPitch, 15 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cBateria, u8 ("Caixa com Palma"),
        { { O (0, "pos"), cl::tri }, { O (0, "lvl"), 50 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 65 }, { id::noiseLvl, 80 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 6000 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 200 }, { E (0, "s"), 0 }, { E (0, "r"), 200 }, { E (0, "c"), -50 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 35 }, { E (2, "s"), 0 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 38.0f }, { L (0, "shape"), sh::quadrada },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 },
          { id::voices, 4 }, { id::velAmp, 70 }, { id::volume, -7 } },
        { { srcEnv3, dstPitch, 25 }, { srcLfo1, dstAmp, 35 } }));

    p.push_back (mk (cBateria, u8 ("Tamborim de Lata"),
        { { O (0, "pos"), cl::tri }, { O (0, "oct"), 1 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 30 },
          { F (0, "type"), ft::pa12 }, { F (0, "cut"), 600 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 90 }, { E (0, "s"), 0 }, { E (0, "r"), 60 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 15 }, { E (2, "s"), 0 },
          { id::voices, 4 }, { id::velAmp, 80 }, { id::volume, -6 } },
        { { srcEnv3, dstPitch, 20 }, { srcVel, dstNoiseLevel, 20 } }));

    // =====================================================================================================
    // TECLAS
    // =====================================================================================================
    p.push_back (mk (cTeclas, u8 ("Elétrico de Palco"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 25 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "lvl"), 50 },
          { E (0, "a"), 1 }, { E (0, "d"), 2500 }, { E (0, "s"), 20 }, { E (0, "r"), 500 },
          { E (2, "d"), 800 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 4.5f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 25 },
          { id::voices, 12 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, 40 }, { srcVel, dstO1Pos, 30 }, { srcLfo1, dstPan, 30 }, { srcMacro1, dstO1Pos, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Teclado de Lata"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 15 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmLinear }, { O (1, "pos"), 30 }, { O (1, "lvl"), 60 },
          { E (0, "a"), 1 }, { E (0, "d"), 2000 }, { E (0, "s"), 15 }, { E (0, "r"), 400 },
          { E (2, "d"), 400 },
          { id::phsOn, 1 }, { id::phsRate, 0.3f }, { id::phsMix, 30 },
          { id::voices, 12 }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pos, 30 }, { srcVel, dstO2Pos, 25 }, { srcMacro1, dstO1Level, -50 } }));

    p.push_back (mk (cTeclas, u8 ("Diapasão Quente"),
        { { O (0, "pos"), cl::seno },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmLinear }, { O (1, "pos"), 40 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 35 },
          { E (0, "a"), 1 }, { E (0, "d"), 3000 }, { E (0, "s"), 10 }, { E (0, "r"), 600 },
          { E (2, "d"), 300 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 4.0f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 20 },
          { id::drift, 20 }, { id::voices, 12 }, { id::volume, -5 } },
        { { srcEnv3, dstO2Level, 40 }, { srcVel, dstO2Pos, 30 }, { srcLfo1, dstAmp, 15 }, { srcMacro1, dstO2Level, 40 } }));

    p.push_back (mk (cTeclas, u8 ("Capela"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 80 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::orgao }, { O (1, "pos"), 40 }, { O (1, "oct"), -1 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 12000 }, { F (0, "res"), 0 },
          { E (0, "a"), 20 }, { E (0, "s"), 100 }, { E (0, "r"), 300 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 4 }, { id::revMix, 30 },
          { id::velAmp, 0 }, { id::voices, 12 }, { id::volume, -9 } },
        { { srcMacro1, dstO1Pos, -40 }, { srcWheel, dstO2Level, 40 } }));

    p.push_back (mk (cTeclas, u8 ("Órgão de Baile"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 50 },
          { F (0, "cut"), 10000 }, { F (0, "res"), 0 },
          { E (0, "a"), 2 }, { E (0, "s"), 100 }, { E (0, "r"), 80 },
          { E (2, "d"), 150 },
          { id::dstOn, 1 }, { id::dstDrive, 30 }, { id::dstMix, 50 },
          { id::choOn, 1 }, { id::choRate, 6.0f }, { id::choDepth, 40 }, { id::choMix, 40 },
          { id::velAmp, 0 }, { id::voices, 12 }, { id::volume, -9 } },
        { { srcEnv3, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cTeclas, u8 ("Caixa Giratória"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 30 },
          { F (0, "cut"), 9000 }, { F (0, "res"), 0 },
          { E (0, "a"), 5 }, { E (0, "s"), 100 }, { E (0, "r"), 150 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.8f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::velAmp, 0 }, { id::voices, 12 }, { id::volume, -8 } },
        { { srcLfo1, dstPan, 25 }, { srcLfo1, dstAmp, 10 }, { srcWheel, dstLfo1Rate, 70 }, { srcMacro1, dstO1Pos, 40 } }));

    p.push_back (mk (cTeclas, u8 ("Clave Funk"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 70 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 1500 }, { F (0, "res"), 30 }, { F (0, "env"), 50 }, { F (0, "key"), 50 },
          { E (1, "d"), 200 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 30 }, { E (0, "r"), 50 },
          { id::phsOn, 1 }, { id::phsRate, 0.5f }, { id::phsMix, 25 },
          { id::velAmp, 70 }, { id::volume, -6 } },
        { { srcVel, dstFACut, 30 }, { srcMacro1, dstO1Pos, -40 } }));

    p.push_back (mk (cTeclas, u8 ("Clave com Pedal"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 60 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 800 }, { F (0, "res"), 55 }, { F (0, "env"), 50 }, { E (1, "d"), 250 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 700 }, { E (0, "s"), 30 }, { E (0, "r"), 60 },
          { id::velAmp, 70 }, { id::volume, -7 } },
        { { srcPressure, dstFACut, 40 }, { srcWheel, dstFACut, 50 }, { srcVel, dstFACut, 20 } }));

    p.push_back (mk (cTeclas, u8 ("Piano de Cordas Finas"),
        { { O (0, "tab"), tb::corda }, { O (0, "pos"), 60 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::corda }, { O (1, "pos"), 40 }, { O (1, "fine"), 4 }, { O (1, "lvl"), 50 },
          { F (0, "cut"), 6000 }, { F (0, "key"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 4000 }, { E (0, "s"), 0 }, { E (0, "r"), 600 }, { E (0, "c"), -50 },
          { E (2, "d"), 1200 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 },
          { id::velAmp, 70 }, { id::voices, 16 }, { id::volume, -6 } },
        { { srcEnv3, dstO1Pos, -50 }, { srcVel, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Cravo de Fibra"),
        { { O (0, "tab"), tb::escova }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::pulso }, { O (1, "pos"), 50 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 40 },
          { F (0, "type"), ft::pb12 }, { F (0, "cut"), 7000 }, { F (0, "key"), 40 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 1500 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 25 },
          { id::velAmp, 30 }, { id::voices, 12 }, { id::volume, -8 } },
        { { srcMacro1, dstO2Level, 40 } }));

    p.push_back (mk (cTeclas, u8 ("Elétrico Rouco"),
        { { O (0, "tab"), tb::senoQuad }, { O (0, "pos"), 30 },
          { E (0, "a"), 1 }, { E (0, "d"), 2000 }, { E (0, "s"), 30 }, { E (0, "r"), 300 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 5.0f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 15 },
          { id::velAmp, 70 }, { id::voices, 12 }, { id::volume, -6 } },
        { { srcVel, dstO1Pos, 40 }, { srcLfo1, dstAmp, 15 }, { srcMacro1, dstO1Pos, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Sopro Analógico"),
        { { O (0, "pos"), cl::serra },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 7 },
          { F (0, "cut"), 900 }, { F (0, "res"), 15 }, { F (0, "env"), 45 },
          { E (1, "a"), 60 }, { E (1, "d"), 500 }, { E (1, "s"), 50 },
          { E (0, "a"), 25 }, { E (0, "d"), 500 }, { E (0, "s"), 85 }, { E (0, "r"), 250 },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::drift, 25 }, { id::volume, -9 } },
        { { srcVel, dstFACut, 25 }, { srcPressure, dstFACut, 25 }, { srcMacro1, dstFACut, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Piano de Brinquedo"),
        { { O (0, "pos"), cl::tri },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::sinos }, { O (1, "pos"), 10 }, { O (1, "oct"), 1 }, { O (1, "lvl"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { id::volume, -5 } },
        { { srcVel, dstO2Level, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Harmônio do Porão"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 20 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::vocal }, { O (1, "pos"), 30 }, { O (1, "lvl"), 40 },
          { E (0, "a"), 80 }, { E (0, "s"), 100 }, { E (0, "r"), 600 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.3f }, { L (0, "mode"), lm::livre },
          { id::choOn, 1 }, { id::choMix, 35 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 },
          { id::velAmp, 20 }, { id::volume, -8 } },
        { { srcLfo1, dstO2Pos, 30 }, { srcPressure, dstO2Level, 30 } }));

    p.push_back (mk (cTeclas, u8 ("Elétrico da Madrugada"),
        { { O (0, "tab"), tb::fmLinear }, { O (0, "pos"), 15 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "oct"), -1 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 1500 }, { F (0, "key"), 30 },
          { E (0, "a"), 1 }, { E (0, "d"), 3000 }, { E (0, "s"), 15 }, { E (0, "r"), 600 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 25 },
          { id::voices, 12 }, { id::volume, -6 } },
        { { srcVel, dstO1Pos, 30 }, { srcVel, dstFACut, 20 }, { srcMacro1, dstFACut, 35 } }));

    p.push_back (mk (cTeclas, u8 ("Teclado de Vidro"),
        { { O (0, "tab"), tb::cristal }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::fmLinear }, { O (1, "pos"), 20 }, { O (1, "lvl"), 50 },
          { E (0, "a"), 1 }, { E (0, "d"), 2500 }, { E (0, "s"), 10 }, { E (0, "r"), 800 },
          { id::choOn, 1 }, { id::choMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 30 },
          { id::voices, 12 }, { id::volume, -6 } },
        { { srcVel, dstO1Pos, 25 }, { srcMacro1, dstO2Level, -50 } }));

    p.push_back (mk (cTeclas, u8 ("Órgão de Boteco"),
        { { O (0, "tab"), tb::orgao }, { O (0, "pos"), 90 },
          { F (0, "cut"), 8000 }, { F (0, "res"), 0 },
          { E (0, "a"), 2 }, { E (0, "s"), 100 }, { E (0, "r"), 100 },
          { id::dstOn, 1 }, { id::dstDrive, 55 }, { id::dstMix, 70 },
          { id::phsOn, 1 }, { id::phsRate, 0.6f }, { id::phsMix, 35 },
          { id::velAmp, 0 }, { id::voices, 12 }, { id::volume, -11 } },
        { { srcMacro1, dstFACut, -40 }, { srcWheel, dstO1Pos, -30 } }));

    p.push_back (mk (cTeclas, u8 ("Polifônico Oitentista"),
        { { O (0, "tab"), tb::pulso }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::serra }, { O (1, "fine"), 6 },
          { F (0, "cut"), 2000 }, { F (0, "res"), 15 }, { F (0, "env"), 30 },
          { E (1, "d"), 800 }, { E (1, "s"), 30 },
          { E (0, "a"), 3 }, { E (0, "d"), 800 }, { E (0, "s"), 70 }, { E (0, "r"), 400 },
          { id::choOn, 1 }, { id::choMix, 40 },
          { id::drift, 25 }, { id::volume, -8 } },
        { { srcVel, dstFACut, 20 }, { srcMacro1, dstO1Pos, 40 }, { srcPressure, dstFACut, 25 } }));

    p.push_back (mk (cTeclas, u8 ("Quimera de Palco"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 80 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::seno }, { O (1, "lvl"), 45 },
          { E (0, "a"), 1 }, { E (0, "d"), 2200 }, { E (0, "s"), 15 }, { E (0, "r"), 500 },
          { E (2, "d"), 600 },
          { id::choOn, 1 }, { id::choMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 20 },
          { id::voices, 12 }, { id::volume, -7 } },
        { { srcEnv3, dstO1Pos, -30 }, { srcVel, dstO1Pos, 20 }, { srcMacro1, dstO1Pos, -50 } }));

    p.push_back (mk (cTeclas, u8 ("Sanfona Digital"),
        { { O (0, "tab"), tb::harmonicos }, { O (0, "pos"), 30 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::orgao }, { O (1, "pos"), 60 }, { O (1, "lvl"), 50 }, { O (1, "fine"), 6 },
          { E (0, "a"), 40 }, { E (0, "s"), 100 }, { E (0, "r"), 400 },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::velAmp, 30 }, { id::volume, -8 } },
        { { srcMacro1, dstO1Pos, 50 }, { srcPressure, dstAmp, 30 } }));

    // =====================================================================================================
    // FX
    // =====================================================================================================
    p.push_back (mk (cFx, u8 ("Subida ao Céu"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 6 }, { O (0, "det"), 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 60 },
          { F (0, "cut"), 300 }, { F (0, "res"), 40 },
          { E (0, "a"), 4000 }, { E (0, "s"), 100 }, { E (0, "r"), 1500 },
          { E (2, "a"), 6000 }, { E (2, "d"), 100 }, { E (2, "s"), 100 }, { E (2, "r"), 500 },
          { id::hypOn, 1 }, { id::hypMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 40 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -10 } },
        { { srcEnv3, dstFACut, 60 }, { srcEnv3, dstPitch, 25 }, { srcMacro1, dstFARes, 30 } }));

    p.push_back (mk (cFx, u8 ("Elevador de Carga"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 50 }, { id::noiseLvl, 70 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 300 }, { F (0, "res"), 60 },
          { E (0, "a"), 3000 }, { E (0, "s"), 100 }, { E (0, "r"), 1000 },
          { E (2, "a"), 8000 }, { E (2, "s"), 100 }, { E (2, "r"), 500 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::quadrada },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -8 } },
        { { srcEnv3, dstFACut, 70 }, { srcLfo1, dstAmp, 30 }, { srcEnv3, dstLfo1Rate, 50 } }));

    p.push_back (mk (cFx, u8 ("Queda Livre"),
        { { O (0, "pos"), cl::serra }, { O (0, "uni"), 4 },
          { id::noiseOn, 1 }, { id::noiseLvl, 40 },
          { F (0, "cut"), 400 }, { F (0, "res"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 3500 }, { E (0, "s"), 0 }, { E (0, "r"), 1000 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 3000 }, { E (2, "s"), 0 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -9 } },
        { { srcEnv3, dstPitch, 40 }, { srcEnv3, dstFACut, 60 } }));

    p.push_back (mk (cFx, u8 ("Pistola de Raio"),
        { { O (0, "tab"), tb::fmMetalico }, { O (0, "pos"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 400 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 200 }, { E (2, "s"), 0 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyFb, 50 }, { id::dlyMix, 30 }, { id::volume, -9 } },
        { { srcEnv3, dstPitch, 100 }, { srcRandom, dstPitch, 5 } }));

    p.push_back (mk (cFx, u8 ("Feixe Sincronizado"),
        { { O (0, "tab"), tb::sincronizado }, { O (0, "pos"), 80 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 600 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 300 }, { E (2, "s"), 0 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8t }, { id::dlyFb, 45 }, { id::dlyMix, 30 }, { id::volume, -10 } },
        { { srcEnv3, dstO1Pos, -70 }, { srcEnv3, dstPitch, 40 } }));

    p.push_back (mk (cFx, u8 ("Impacto Sísmico"),
        { { O (0, "pos"), cl::seno }, { O (0, "oct"), -1 }, { O (0, "rand"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 40 }, { id::noiseLvl, 60 },
          { F (0, "cut"), 3000 }, { F (0, "env"), 30 }, { E (1, "d"), 800 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 3000 }, { E (0, "s"), 0 }, { E (0, "r"), 2000 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 150 }, { E (2, "s"), 0 },
          { id::dstOn, 1 }, { id::dstDrive, 40 }, { id::dstMix, 60 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revSize, 90 }, { id::revDecay, 8 }, { id::revMix, 45 },
          { id::cmpOn, 1 }, { id::cmpMode, cp::multibanda }, { id::cmpAmt, 40 }, { id::volume, -7 } },
        { { srcEnv3, dstPitch, 70 } }));

    p.push_back (mk (cFx, u8 ("Vassoura de Ruído"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 70 }, { id::noiseLvl, 80 },
          { F (0, "type"), ft::pf12 }, { F (0, "cut"), 500 }, { F (0, "res"), 55 },
          { E (0, "a"), 200 }, { E (0, "r"), 1500 },
          { L (0, "div"), dv::c2 }, { L (0, "shape"), sh::tri }, { L (0, "mode"), lm::livre },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d4 }, { id::dlyMix, 20 },
          { id::revOn, 1 }, { id::revType, rv::placa }, { id::revMix, 30 }, { id::volume, -9 } },
        { { srcLfo1, dstFACut, 60 }, { srcMacro1, dstFARes, 30 } }));

    p.push_back (mk (cFx, u8 ("Sirene de Bordo"),
        { { O (0, "pos"), cl::quadrada },
          { F (0, "cut"), 4000 },
          { L (0, "div"), dv::d8 }, { L (0, "shape"), sh::quadrada },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyMix, 20 },
          { id::polyMode, pm::mono }, { id::volume, -11 } },
        { { srcLfo1, dstO1Pitch, 15 } }));

    p.push_back (mk (cFx, u8 ("Sirene Distante"),
        { { O (0, "tab"), tb::senoQuad }, { O (0, "pos"), 50 },
          { F (0, "cut"), 3000 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 0.5f }, { L (0, "shape"), sh::tri }, { L (0, "mode"), lm::livre },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 45 },
          { id::polyMode, pm::mono }, { id::volume, -11 } },
        { { srcLfo1, dstPitch, 25 }, { srcMacro1, dstLfo1Rate, 50 } }));

    p.push_back (mk (cFx, u8 ("Robô Tagarela"),
        { { O (0, "tab"), tb::vocal }, { O (0, "pos"), 40 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::sah },
          { L (1, "div"), dv::d8 }, { L (1, "shape"), sh::sah },
          { id::dstOn, 1 }, { id::dstType, ds::bitcrush }, { id::dstDrive, 40 }, { id::dstMix, 50 },
          { id::volume, -9 } },
        { { srcLfo1, dstO1Pos, 60 }, { srcLfo2, dstPitch, 6 }, { srcMacro1, dstLfo1Rate, 40 } }));

    p.push_back (mk (cFx, u8 ("Fita Ligando"),
        { { O (0, "pos"), cl::serra }, { O (0, "oct"), -2 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::classica }, { O (1, "pos"), cl::quadrada }, { O (1, "oct"), -2 }, { O (1, "fine"), 8 },
          { F (0, "cut"), 2500 },
          { E (2, "a"), 1200 }, { E (2, "s"), 100 }, { E (2, "r"), 800 }, { E (2, "c"), 40 },
          { id::choOn, 1 }, { id::choMix, 30 },
          { id::drift, 50 }, { id::volume, -9 } },
        { { srcEnv3, dstPitch, 50 }, { srcEnv3, dstFACut, 30 } }));

    p.push_back (mk (cFx, u8 ("Teletransporte"),
        { { O (0, "tab"), tb::espectral }, { O (0, "pos"), 0 },
          { E (0, "a"), 500 }, { E (0, "r"), 2000 },
          { E (2, "a"), 2000 }, { E (2, "s"), 100 },
          { id::phsOn, 1 }, { id::phsRate, 2.0f }, { id::phsMix, 50 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d316 }, { id::dlyFb, 60 }, { id::dlyMix, 30 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 45 }, { id::volume, -8 } },
        { { srcEnv3, dstO1Pos, 100 }, { srcEnv3, dstPitch, 30 } }));

    p.push_back (mk (cFx, u8 ("Mergulho de Ruído"),
        { { O (0, "on"), 0 },
          { id::noiseOn, 1 }, { id::noiseColor, 80 }, { id::noiseLvl, 80 },
          { F (0, "cut"), 150 }, { F (0, "res"), 30 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 4500 }, { E (0, "s"), 0 }, { E (0, "r"), 1500 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 4000 }, { E (2, "s"), 0 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -9 } },
        { { srcEnv3, dstFACut, 90 } }));

    p.push_back (mk (cFx, u8 ("Colapso Digital"),
        { { O (0, "tab"), tb::digital }, { O (0, "pos"), 30 },
          { E (0, "a"), 5 }, { E (0, "r"), 1000 },
          { E (2, "a"), 2500 }, { E (2, "s"), 100 },
          { L (0, "div"), dv::d32 }, { L (0, "shape"), sh::sah },
          { id::dstOn, 1 }, { id::dstType, ds::downsample }, { id::dstDrive, 50 }, { id::dstMix, 60 },
          { id::revOn, 1 }, { id::revType, rv::sala }, { id::revMix, 30 }, { id::volume, -10 } },
        { { srcEnv3, dstPitch, -50 }, { srcLfo1, dstO1Pos, 40 } }));

    p.push_back (mk (cFx, u8 ("Moedor de Engrenagem"),
        { { O (0, "tab"), tb::metal }, { O (0, "pos"), 60 }, { O (0, "uni"), 6 }, { O (0, "det"), 80 },
          { F (0, "cut"), 2000 }, { F (0, "res"), 40 },
          { L (0, "div"), dv::d32 }, { L (0, "shape"), sh::sah },
          { id::dstOn, 1 }, { id::dstType, ds::duro }, { id::dstDrive, 50 }, { id::dstMix, 60 },
          { id::volume, -12 } },
        { { srcLfo1, dstFACut, 40 }, { srcMacro1, dstDetune, 20 } }));

    p.push_back (mk (cFx, u8 ("Sonar Submarino"),
        { { O (0, "pos"), cl::seno }, { O (0, "oct"), 1 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 300 }, { E (0, "s"), 0 }, { E (0, "r"), 300 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d38 }, { id::dlyFb, 75 }, { id::dlyMix, 45 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 50 }, { id::volume, -6 } },
        { { srcMacro1, dstFACut, -40 } }));

    p.push_back (mk (cFx, u8 ("Passarinho de Corda"),
        { { O (0, "pos"), cl::seno }, { O (0, "oct"), 2 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 250 }, { E (0, "s"), 0 }, { E (0, "r"), 200 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 200 }, { E (2, "s"), 0 },
          { L (0, "sync"), 0 }, { L (0, "rate"), 9.0f },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d16 }, { id::dlyMix, 25 }, { id::volume, -8 } },
        { { srcLfo1, dstPitch, 8 }, { srcEnv3, dstPitch, 25 }, { srcRandom, dstPitch, 4 } }));

    p.push_back (mk (cFx, u8 ("Aspirador Reverso"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 50 }, { O (0, "lvl"), 40 },
          { id::noiseOn, 1 }, { id::noiseColor, 60 }, { id::noiseLvl, 70 },
          { F (0, "cut"), 200 }, { F (0, "res"), 30 },
          { E (0, "a"), 2000 }, { E (0, "d"), 50 }, { E (0, "s"), 0 }, { E (0, "r"), 50 }, { E (0, "c"), 60 },
          { E (2, "a"), 2000 }, { E (2, "s"), 100 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 35 }, { id::volume, -8 } },
        { { srcEnv3, dstFACut, 70 } }));

    p.push_back (mk (cFx, u8 ("Desmoronamento"),
        { { O (0, "tab"), tb::granito }, { O (0, "pos"), 50 }, { O (0, "oct"), -1 }, { O (0, "uni"), 6 }, { O (0, "det"), 60 },
          { id::noiseOn, 1 }, { id::noiseColor, 40 }, { id::noiseLvl, 60 },
          { F (0, "cut"), 200 }, { F (0, "res"), 20 },
          { E (0, "a"), 0.5f }, { E (0, "d"), 3000 }, { E (0, "s"), 0 }, { E (0, "r"), 2000 },
          { E (2, "a"), 0.5f }, { E (2, "d"), 1500 }, { E (2, "s"), 0 },
          { id::dstOn, 1 }, { id::dstDrive, 50 }, { id::dstMix, 60 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revDecay, 6 }, { id::revMix, 45 }, { id::volume, -10 } },
        { { srcEnv3, dstFACut, 60 }, { srcEnv3, dstO1Pos, 50 } }));

    p.push_back (mk (cFx, u8 ("Portal da Quimera"),
        { { O (0, "tab"), tb::quimeraTab }, { O (0, "pos"), 0 },
          { O (1, "on"), 1 }, { O (1, "tab"), tb::quimeraTab }, { O (1, "pos"), 100 }, { O (1, "semi"), 7 }, { O (1, "lvl"), 40 },
          { F (0, "cut"), 1500 }, { F (0, "res"), 25 },
          { E (0, "a"), 1000 }, { E (0, "r"), 2500 },
          { E (2, "a"), 5000 }, { E (2, "s"), 100 },
          { L (0, "div"), dv::d16 }, { L (0, "shape"), sh::quadrada },
          { id::hypOn, 1 }, { id::hypMix, 40 },
          { id::dlyOn, 1 }, { id::dlyDiv, dv::d8 }, { id::dlyFb, 45 }, { id::dlyMix, 25 },
          { id::revOn, 1 }, { id::revType, rv::hall }, { id::revMix, 40 }, { id::volume, -10 } },
        { { srcEnv3, dstO1Pos, 100 }, { srcEnv3, dstFACut, 40 }, { srcLfo1, dstAmp, 40 }, { srcEnv3, dstLfo1Rate, 40 },
          { srcMacro1, dstPitch, 25 } }));

    return p;
}
} // namespace quimera

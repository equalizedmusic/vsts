// Abre o editor do plugin, toca um loop de psytrance a 145 BPM por alguns segundos (para medidores,
// mandala, rolos e fogo estarem vivos) e grava a tela em PNG a 200%. Instrumento (SHOT_SYNTH): em vez do loop,
// toca um acorde segurado por MIDI com pressão (MPE/aftertouch) subindo.
// Uso: shot_<plugin> <pasta de saída>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#ifndef SHOT_MESA
 #define SHOT_MESA 0
#endif
#ifndef SHOT_ALIGN
 #define SHOT_ALIGN 0
#endif
// MESA: a foto mostra a CONSOLE cheia, com MESA CANAL (canais e grupos) tocando o mesmo loop; e a janela de um canal
static std::vector<std::unique_ptr<juce::AudioProcessor>> extras;
#if SHOT_MESA
juce::AudioProcessor* mesaCriaCanal();
#endif

namespace
{
constexpr double fs = 48000.0, bpm = 145.0;
constexpr int    block = 512;
#ifndef SHOT_SYNTH
 #define SHOT_SYNTH 0
#endif
#ifndef SHOT_W
 #define SHOT_W 2000
 #define SHOT_H 1160
#endif

// host "tocando": régua andando no BPM
class ShotPlayHead : public juce::AudioPlayHead
{
public:
    std::atomic<double> ppq { 0.0 };

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        p.setPpqPosition (ppq.load());
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

// bumbo em todo tempo, baixo rolante nas semicolcheias, chimbal no contratempo e um pad
class AudioThread : public juce::Thread
{
public:
    AudioThread (juce::AudioProcessor& p, ShotPlayHead& h) : juce::Thread ("shot-audio"), proc (p), head (h) {}

    void run() override
    {
        juce::AudioBuffer<float> buf (SHOT_ALIGN ? 4 : 2, block);
        juce::MidiBuffer midi;
        juce::Random rnd (7);
        const double twoPi = juce::MathConstants<double>::twoPi;

        while (! threadShouldExit())
        {
            auto* l = buf.getWritePointer (0);
            auto* r = buf.getWritePointer (1);
            for (int i = 0; i < block; ++i, t += 1.0 / fs)
            {
                const double beat = t * bpm / 60.0;
                const double inBeat = (beat - std::floor (beat)) * 60.0 / bpm;            // s desde o tempo
                const double six = beat * 4.0;
                const int    k16 = (int) std::floor (six) % 4;
                const double in16 = (six - std::floor (six)) * 15.0 / bpm;                // s desde a semicolcheia

                kickPh += twoPi * (45.0 + 110.0 * std::exp (-inBeat * 30.0)) / fs;
                const double kick = std::sin (kickPh) * std::exp (-inBeat * 7.0) * 0.85;

                bassPh += 46.25 / fs;                                                     // F#1
                const double saw  = 2.0 * (bassPh - std::floor (bassPh)) - 1.0;
                const double bass = k16 == 0 ? 0.0 : saw * std::exp (-in16 * 28.0) * 0.32;

                const bool   off  = std::fmod (beat, 1.0) >= 0.5;
                const double hatT = (std::fmod (beat, 0.5)) * 60.0 / bpm;
                const double hat  = off ? (rnd.nextFloat() * 2.0 - 1.0) * std::exp (-hatT * 60.0) * 0.18 : 0.0;

                padPh1 += 369.99 / fs;
                padPh2 += 554.37 / fs;
                const double pad = 0.06 * std::sin (twoPi * padPh1) + 0.05 * std::sin (twoPi * padPh2);

                l[i] = (float) (kick + bass + hat + pad);
                r[i] = (float) (kick + bass - hat * 0.6 + pad * 0.8);
            }
           #if SHOT_ALIGN
            // ALIGN: rolling de psy (K b B B, o b a -3 dB). Entrada = BAIXO (saw com LP de envelope, sustain 0),
            // sidechain = KICK (queda até G1). A nota começa ~155° fora da cauda do kick: cancela no grave.
            {
                auto* bl = buf.getWritePointer (0); auto* br = buf.getWritePointer (1);
                auto* kl = buf.getWritePointer (2); auto* kr = buf.getWritePointer (3);
                const double t0 = t - block / fs;
                for (int i = 0; i < block; ++i)
                {
                    const double tt = t0 + i / fs, beatLen = 60.0 / bpm, s16 = beatLen / 4;
                    const double tb = tt - std::floor (tt / beatLen) * beatLen;
                    const double fk = 49.0 + 91.0 * std::exp (-tb * 90.0);
                    if (tb < 1.0 / fs) aKick = 0;
                    aKick += twoPi * fk / fs;
                    const double kick = std::sin (aKick) * std::min (1.0, tb / 0.001) * std::exp (-tb * 13.0) * 0.9;
                    const int slot = (int) (tb / s16);
                    double bass = 0;
                    if (slot > 0)
                    {
                        const double u = tb - slot * s16, gate = s16 * 0.86;
                        const double ph = 49.0 * u + 155.0 / 360.0;
                        const double saw = 2.0 * (ph - std::floor (ph + 0.5));
                        const double env = std::min (1.0, u / 0.002) * (u < gate ? std::exp (-u * 7) : std::exp (-gate * 7) * std::max (0.0, 1 - (u - gate) / 0.006));
                        const double fc = 120.0 + 260.0 * std::exp (-u * 35.0), gg = std::tan (juce::MathConstants<double>::pi * fc / fs), kq = 1.0 / 0.9;
                        const double a1 = 1.0 / (1.0 + gg * (gg + kq)), a2 = gg * a1, a3 = gg * a2;
                        const double v3 = saw - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
                        bass = v2 * env * (slot == 1 ? 0.62 * 0.708 : 0.62);
                    }
                    bl[i] = br[i] = (float) bass;
                    kl[i] = kr[i] = (float) kick;
                }
            }
           #endif
            head.ppq.store (head.ppq.load() + block * bpm / 60.0 / fs);
           #if SHOT_SYNTH
            // instrumento: silêncio na entrada; acorde de Lá menor com 9ª e um baixo, pressão subindo
            buf.clear();
            midi.clear();
            if (blocks == 0)
                for (int note : { 45, 57, 60, 64, 67, 71 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
            if (blocks % 8 == 0)
                midi.addEvent (juce::MidiMessage::channelPressureChange (1, juce::jmin (127, blocks)), 0);
            if (blocks % 8 == 4)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::jmin (127, blocks / 2)), 0);
            ++blocks;
           #endif
           #if SHOT_MESA
            for (size_t k = 0; k < extras.size(); ++k)
            {
                juce::AudioBuffer<float> copy (buf);
                copy.applyGain (0.35f + 0.08f * (float) (k % 7));
                extras[k]->processBlock (copy, midi);
            }
           #endif
            proc.processBlock (buf, midi);
            juce::Thread::sleep (10);   // ~tempo real (512 amostras = 10,7 ms)
        }
    }

private:
    juce::AudioProcessor& proc;
    ShotPlayHead& head;
    double t = 0.0, kickPh = 0.0, bassPh = 0.0, padPh1 = 0.0, padPh2 = 0.0, aKick = 0.0, ic1 = 0.0, ic2 = 0.0;
    int blocks = 0;
};

void setParam (juce::AudioProcessor& p, const juce::String& pid, float value)
{
    for (auto* prm : p.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() == pid)
                rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

// ajustes para a foto ficar representativa
void applyLook (juce::AudioProcessor& p)
{
    const juce::String name (SHOT_PLUGIN);
    if (name == "CHAMA")   { setParam (p, "chama", 78.0f); setParam (p, "noise", 70.0f); setParam (p, "space", 70.0f); }
    if (name == "NAVALHA") { setParam (p, "drive", 12.0f); setParam (p, "ceiling", -4.0f); setParam (p, "knee", 10.0f); }
    if (name == "FITA")    { setParam (p, "drive", 55.0f); setParam (p, "wow", 45.0f); }
    if (name == "F1")      { setParam (p, "cutoff", 700.0f); setParam (p, "resonance", 70.0f); setParam (p, "envamt", 60.0f); setParam (p, "lfoamt", 30.0f); }
    if (name == "RINGER")  { setParam (p, "freq", 55.0f); setParam (p, "lfoamt", 40.0f); }
    if (name == "REPETECO") { setParam (p, "feedback", 70.0f); setParam (p, "mix", 45.0f); }
    if (name == "MIRAGEM") { setParam (p, "shimmer", 60.0f); setParam (p, "motion", 60.0f); setParam (p, "mix", 45.0f); }
    if (name == "PICOTE")
    {
        setParam (p, "swing", 20.0f); setParam (p, "pan02", -60.0f); setParam (p, "pan04", 60.0f);
        // a foto sai num instante qualquer do compasso: nenhum passo mudo, senão os medidores aparecem zerados
        const char* const fracos[] = { "vol02", "vol04", "vol06", "vol08", "vol10", "vol12", "vol14", "vol16" };
        const float niveis[] = { 35.0f, 55.0f, 35.0f, 70.0f, 35.0f, 55.0f, 35.0f, 80.0f };
        for (int i = 0; i < 8; ++i) setParam (p, fracos[i], niveis[i]);
    }
    if (name == "PRISMA")  { setParam (p, "macro", 55.0f); setParam (p, "drive1", 6.0f); }
    if (name == "TORNO")   { setParam (p, "squeeze", 40.0f); }
    if (name == "BARRAGEM") { setParam (p, "drive", 8.0f); }
    if (name == "CONSOLE") { setParam (p, "drive", 65.0f); }
    if (name == "SOCO")    { setParam (p, "attack", 60.0f); setParam (p, "sustain", -30.0f); }
    if (name == "LIXA")    { setParam (p, "thresh", -42.0f); setParam (p, "range", 10.0f); }
    if (name == "HORIZONTE") { setParam (p, "width", 140.0f); }
    if (name == "LIMAO")
    {
        // o sequenciador toca o padrão do preset (host tocando a 145); ácido e delay para a foto ter vida
        setParam (p, "acid", 35.0f); setParam (p, "dlon", 1.0f); setParam (p, "drive", 35.0f);
    }
    if (name == "QUIMERA")
    {
        // os 3 osciladores (analógico, modelado e wavetable), o filtro B e alguns efeitos ligados
        setParam (p, "o1tab", 23.0f); setParam (p, "o1pos", 35.0f); setParam (p, "o1uni", 4.0f);
        setParam (p, "o2on", 1.0f);   setParam (p, "o2tab", 5.0f);  setParam (p, "o2pos", 60.0f); setParam (p, "o2semi", 7.0f);
        setParam (p, "o3on", 1.0f);   setParam (p, "o3tab", 17.0f); setParam (p, "o3pos", 45.0f); setParam (p, "o3oct", 1.0f);
        setParam (p, "o3warp", 8.0f); setParam (p, "o3wamt", 30.0f);
        setParam (p, "facut", 1400.0f); setParam (p, "fares", 55.0f); setParam (p, "faenv", 40.0f);
        setParam (p, "fbon", 1.0f);
        setParam (p, "e2d", 900.0f); setParam (p, "e2s", 30.0f);
        setParam (p, "m01src", 4.0f); setParam (p, "m01dst", 2.0f);  setParam (p, "m01amt", 40.0f);   // LFO 1 → osc 1 posição
        setParam (p, "m02src", 9.0f); setParam (p, "m02dst", 19.0f); setParam (p, "m02amt", 35.0f);   // pressão → corte A
        setParam (p, "l1div", 10.0f);
        setParam (p, "hyon", 1.0f); setParam (p, "chon", 1.0f); setParam (p, "dlon", 1.0f); setParam (p, "rvon", 1.0f);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    ShotPlayHead head;
    proc->setPlayHead (&head);
    proc->setPlayConfigDetails (SHOT_SYNTH ? 0 : (SHOT_ALIGN ? 4 : 2), 2, fs, block);   // ALIGN: baixo + sidechain
    proc->prepareToPlay (fs, block);
    applyLook (*proc);
   #if SHOT_MESA
    {
        // o plugin principal é a CONSOLE (master); os canais e grupos são MESA CANAL abertos no mesmo programa
        struct Ch { const char* name; juce::uint32 col; int mode, grp; float pre, eq, gain, drive, hpf, fader, pan, thr; };
        const Ch chs[] = {
            { "KICK", 0xffe8473c, 0, 1, 2, 0, 3, 3, 30, -2.4f, 0, -16 },   { "CAIXA", 0xffff8a3d, 0, 1, 2, 0, 2, 2, 90, -1.5f, 0.1f, -14 },
            { "HIHAT", 0xfff2d24b, 0, 1, 0, 0, 0, 0, 300, -6, 0.3f, 10 },  { "OH L", 0xfff2a33a, 0, 1, 1, 0, 1, 1, 120, -4.5f, -0.7f, -20 },
            { "OH R", 0xfff2a33a, 0, 1, 1, 0, 1, 1, 120, -4.5f, 0.7f, -20 }, { "BAIXO", 0xff3d8bff, 0, 2, 2, 1, 4, 4, 35, -0.6f, 0, -18 },
            { "GTR L", 0xff3fbf6a, 0, 2, 2, 1, 2, 3, 110, -2.7f, -0.75f, -22 }, { "GTR R", 0xff3fbf6a, 0, 2, 2, 1, 2, 3, 110, -2.7f, 0.75f, -22 },
            { "TECLA", 0xff22d8ff, 0, 2, 0, 1, 0, 0, 60, -4.2f, -0.2f, 10 }, { "VOZ", 0xffc49bff, 0, 3, 1, 0, 3, 3, 90, 0.8f, 0, -24 },
            { "CORO", 0xffc49bff, 0, 3, 1, 0, 1, 1, 140, -5.5f, 0.2f, -20 }, { "FX", 0xff9aa0a8, 0, 0, 0, 0, 0, 0, 200, -9.0f, 0.4f, 10 },
            { "BATERIA", 0xffe8473c, 1, 1, 0, 0, 0, 0, 20, -1.2f, 0, -14 }, { "BASE", 0xff3d8bff, 1, 2, 0, 0, 0, 0, 20, -2.0f, 0, -12 },
            { "VOZES", 0xffc49bff, 1, 3, 0, 0, 0, 0, 20, -0.5f, 0, -16 } };
        juce::AudioProcessor::TrackProperties tp;
        tp.name = juce::String ("Master"); tp.colourARGB = 0xffd0d0d0u;
        proc->updateTrackProperties (tp);
        setParam (*proc, "mthr", -12); setParam (*proc, "mmkp", 3);
        for (auto& c : chs)
        {
            std::unique_ptr<juce::AudioProcessor> q (mesaCriaCanal());
            q->setPlayHead (&head);
            q->setPlayConfigDetails (2, 2, fs, block);
            q->prepareToPlay (fs, block);
            juce::AudioProcessor::TrackProperties t;
            // como no Ableton: "N-Nome" (o número vira a posição na console)
            t.name = juce::String ((int) extras.size() + 1) + "-" + juce::String (c.name); t.colourARGB = c.col;
            q->updateTrackProperties (t);
            setParam (*q, "mode", (float) c.mode); setParam (*q, "grp", (float) c.grp); setParam (*q, "pre", c.pre);
            setParam (*q, "eqm", c.eq); setParam (*q, "gain", c.gain); setParam (*q, "drive", c.drive); setParam (*q, "hpf", c.hpf);
            setParam (*q, "fader", c.fader); setParam (*q, "pan", c.pan); setParam (*q, "thr", c.thr);
            setParam (*q, "hfg", (float) ((int) c.col % 7 - 3)); setParam (*q, "lmfg", (float) ((int) (c.col >> 8) % 9 - 4));
            if (c.mode == 1) { setParam (*q, "rat", 4); setParam (*q, "cmix", 70); }
            // modelos variados de compressor e de EQ, para a foto mostrar todos
            const int k = (int) extras.size();
            const int dyns[] = { 2, 0, 0, 3, 3, 3, 0, 0, 0, 3, 2, 0, 1, 1, 1 };
            setParam (*q, "dynm", (float) dyns[k % 15]);
            if (k == 2 || k == 6 || k == 7) setParam (*q, "eqm", 2);
            if (k == 0) { setParam (*q, "lfbell", 1); setParam (*q, "lpf", 9000); setParam (*q, "rat", 20); }
            extras.push_back (std::move (q));
        }
    }
   #endif

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditorAndMakeActive());
    editor->setSize (SHOT_W, SHOT_H);   // 200%

    AudioThread audio (*proc, head);
    audio.startThread();
   #if SHOT_ALIGN
    // desligado (ouvindo e aprendendo) e depois ligado
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (3500);
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto imgOff = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto fOff = outDir.getChildFile ("ALIGN-OFF.png");
        fOff.deleteFile();
        juce::FileOutputStream osOff (fOff);
        juce::PNGImageFormat pngOff;
        std::printf ("ALIGN-OFF: %s\n", osOff.openedOk() && pngOff.writeImageToStream (imgOff, osOff) ? "ok" : "falhou");
        setParam (*proc, "align", 1.0f);
    }
   #endif
    juce::MessageManager::getInstance()->runDispatchLoopUntil (4500);
    // o editor pode ter voltado ao tamanho salvo depois de abrir: de novo em 200% para a foto
    editor->setSize (SHOT_W, SHOT_H);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
   #if SHOT_MESA
    // a mesma console com zoom de 140% e as linhas INPUT e DYN recolhidas: a lateral e a seção central não podem mudar
    {
        juce::MemoryBlock mb;
        proc->getStateInformation (mb);
        if (auto x = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        {
            x->setAttribute ("uizoom", 1.4); x->setAttribute ("uirec", 6); x->setAttribute ("uigeral", 0);
            juce::MemoryBlock m2;
            juce::AudioProcessor::copyXmlToBinary (*x, m2);
            proc->setStateInformation (m2.getData(), (int) m2.getSize());
        }
        editor->setSize (SHOT_W, SHOT_H);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (800);
        const auto img3 = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto f3 = outDir.getChildFile ("MESA-ZOOM.png");
        f3.deleteFile();
        juce::FileOutputStream os3 (f3);
        juce::PNGImageFormat png3;
        std::printf ("MESA-ZOOM: %s\n", os3.openedOk() && png3.writeImageToStream (img3, os3) ? "ok" : "falhou");
    }
    // a janela do MESA CANAL (o canal VOZ), a 200%
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (extras[9]->createEditorAndMakeActive());
        ed2->setSize (800, 2000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (1200);
        ed2->setSize (800, 2000);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const auto img2 = ed2->createComponentSnapshot (ed2->getLocalBounds(), true, 1.0f);
        const auto f2 = outDir.getChildFile ("MESA-CANAL.png");
        f2.deleteFile();
        juce::FileOutputStream os2 (f2);
        juce::PNGImageFormat png2;
        std::printf ("MESA-CANAL: %s\n", os2.openedOk() && png2.writeImageToStream (img2, os2) ? "ok" : "falhou");
    }
   #endif
    audio.stopThread (3000);

    const auto file = outDir.getChildFile (juce::String (SHOT_PLUGIN) + ".png");
    file.deleteFile();
    bool ok = false;
    {
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        ok = os.openedOk() && png.writeImageToStream (image, os);
    }

    editor.reset();
    extras.clear();
    proc->releaseResources();
    proc.reset();
    std::printf ("%s: %s\n", SHOT_PLUGIN, ok ? "ok" : "falhou");
    return ok ? 0 : 1;
}

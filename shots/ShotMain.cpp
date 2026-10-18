// Abre o editor do plugin, toca um loop de psytrance a 145 BPM por alguns segundos (para medidores,
// mandala, rolos e fogo estarem vivos) e grava a tela em PNG a 200%.
// Uso: shot_<plugin> <pasta de saída>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstdio>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double fs = 48000.0, bpm = 145.0;
constexpr int    block = 512;

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
        juce::AudioBuffer<float> buf (2, block);
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
            head.ppq.store (head.ppq.load() + block * bpm / 60.0 / fs);
            proc.processBlock (buf, midi);
            juce::Thread::sleep (10);   // ~tempo real (512 amostras = 10,7 ms)
        }
    }

private:
    juce::AudioProcessor& proc;
    ShotPlayHead& head;
    double t = 0.0, kickPh = 0.0, bassPh = 0.0, padPh1 = 0.0, padPh2 = 0.0;
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
    proc->setPlayConfigDetails (2, 2, fs, block);
    proc->prepareToPlay (fs, block);
    applyLook (*proc);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (2000, 1160);   // 200%

    AudioThread audio (*proc, head);
    audio.startThread();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (4500);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
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
    proc->releaseResources();
    proc.reset();
    std::printf ("%s: %s\n", SHOT_PLUGIN, ok ? "ok" : "falhou");
    return ok ? 0 : 1;
}

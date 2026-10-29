#pragma once

#include <juce_core/juce_core.h>
#include "Rack.h"

// Textos do MONTA-MONTA em português e inglês. O idioma escolhido fica grande; o outro aparece menor, embaixo.
// Os nomes dos controles ficam em inglês nos dois idiomas, como no painel do Minimoog.
namespace mm
{
struct Txt
{
    const char* pt;
    const char* en;
    juce::String get (int lang) const { return juce::String::fromUTF8 (lang == 0 ? pt : en); }
    juce::String other (int lang) const { return get (1 - lang); }
};

struct PieceText
{
    const char* name;      // no painel e na bandeja
    Txt sub;               // embaixo do nome no painel
    Txt full;              // nome completo, no cartão
    Txt about;             // o que é
    Txt knobs;             // o que faz cada controle (uma linha por controle)
    Txt challenge;         // desafio
    Txt where;             // onde encaixa (mensagem de peça no lugar errado)
};

inline const PieceText& pieceText (int m)
{
    static const PieceText t[numModules] = {
        { "VCO",
          { "OSCILADOR", "OSCILLATOR" },
          { "OSCILADOR CONTROLADO POR TENSÃO", "VOLTAGE CONTROLLED OSCILLATOR" },
          { "É onde o som nasce. O oscilador vibra sem parar, e cada nota que você toca muda a velocidade dessa vibração: a afinação.",
            "This is where sound is born. The oscillator never stops vibrating, and each note you play changes how fast it vibrates: the pitch." },
          { "RANGE: a oitava, em pés de órgão. 32' é a mais grave, 2' a mais aguda.\n"
            "WAVEFORM: a forma da onda. Triângulo é macio, serra é brilhante, quadrada é oca.\n"
            "TUNE: afina um pouco para cima ou para baixo.",
            "RANGE: the octave, in organ feet. 32' is the lowest, 2' the highest.\n"
            "WAVEFORM: the wave shape. Triangle is soft, saw is bright, square is hollow.\n"
            "TUNE: tunes slightly up or down." },
          { "Segure uma nota e troque a forma de onda. Qual soa mais brilhante?",
            "Hold a note and switch the waveform. Which one sounds brightest?" },
          { "no começo do caminho, à esquerda", "at the start of the path, on the left" } },

        { "VCO 2",
          { "OSCILADOR 2", "OSCILLATOR 2" },
          { "SEGUNDO OSCILADOR", "SECOND OSCILLATOR" },
          { "Um oscilador igual ao primeiro. Somando os dois, levemente desafinados, o som engorda e ganha movimento.",
            "An oscillator just like the first one. Adding both, slightly detuned, makes the sound fatter and alive." },
          { "RANGE e WAVEFORM: iguais aos do VCO.\n"
            "TUNE: desafine um pouquinho em relação ao VCO e ouça o som \"batendo\". Em +7 st vira uma quinta.",
            "RANGE and WAVEFORM: same as the VCO.\n"
            "TUNE: detune it a little from the VCO and hear the sound \"beating\". At +7 st it becomes a fifth." },
          { "Ponha o TUNE do VCO 2 em +7 st e toque. Que acorde apareceu?",
            "Set VCO 2 TUNE to +7 st and play. What chord appeared?" },
          { "embaixo do MIXER, encaixado nele", "under the MIXER, locked into it" } },

        { "MIXER",
          { "MISTURADOR", "MIXER" },
          { "MISTURADOR", "MIXER" },
          { "Junta os sons: decide quanto de cada oscilador, e do ruído, segue para o filtro. Cada canal tem volume e chave liga/desliga.",
            "Blends the sounds: decides how much of each oscillator, and of the noise, goes on to the filter. Each channel has a level and an on/off switch." },
          { "OSC 1 e OSC 2: o volume de cada oscilador.\n"
            "NOISE: ruído, o chiado do rádio fora da estação. Bom para vento, chimbal e explosões.",
            "OSC 1 and OSC 2: the level of each oscillator.\n"
            "NOISE: noise, like a radio between stations. Great for wind, hi-hats and explosions." },
          { "Desligue os dois osciladores e deixe só o NOISE. Feche o filtro devagar: virou vento?",
            "Switch both oscillators off and keep only NOISE. Slowly close the filter: is it wind now?" },
          { "logo depois do VCO", "right after the VCO" } },

        { "VCF",
          { "FILTRO", "FILTER" },
          { "FILTRO CONTROLADO POR TENSÃO", "VOLTAGE CONTROLLED FILTER" },
          { "O filtro tira os agudos do som, como pôr a mão na frente da boca. É ele que deixa o som escuro ou brilhante.",
            "The filter removes the highs, like putting your hand in front of your mouth. It makes the sound dark or bright." },
          { "CUTOFF: até onde o som passa.\n"
            "EMPHASIS: reforça bem no ponto do corte, o \"uau\" do Moog. No máximo, o filtro assobia sozinho.\n"
            "CONTOUR: quanto o ENV MOD mexe no corte (lição 4).",
            "CUTOFF: how far the sound gets through.\n"
            "EMPHASIS: boosts right at the cutoff, the Moog \"wow\". At the top, the filter whistles by itself.\n"
            "CONTOUR: how much ENV MOD moves the cutoff (lesson 4)." },
          { "Gire o CUTOFF todo para a esquerda e toque uma nota. O que sumiu?",
            "Turn CUTOFF all the way down and play a note. What disappeared?" },
          { "depois do VCO e antes do ENV AMP", "after the VCO and before ENV AMP" } },

        { "ENV AMP",
          { "VOLUME", "LOUDNESS" },
          { "ENVELOPE DE VOLUME", "LOUDNESS ENVELOPE" },
          { "Liga o som quando você aperta a tecla e desliga quando solta, desenhando como o volume nasce e morre. Sem ele, o oscilador toca sem parar.",
            "Turns the sound on when you press a key and off when you release it, shaping how the loudness is born and dies. Without it, the oscillator never stops." },
          { "ATTACK: quanto tempo o som leva para chegar ao máximo.\n"
            "DECAY: quanto tempo leva para cair até o SUSTAIN.\n"
            "SUSTAIN: o volume enquanto a tecla fica apertada.\n"
            "RELEASE: quanto o som demora para sumir depois que você solta.",
            "ATTACK: how long the sound takes to reach the top.\n"
            "DECAY: how long it takes to fall to SUSTAIN.\n"
            "SUSTAIN: the level while the key is held.\n"
            "RELEASE: how long the sound takes to fade after you let go." },
          { "Faça um som de corda: ATTACK lento e RELEASE longo. Depois um pluck: tudo curto e SUSTAIN no zero.",
            "Make a string sound: slow ATTACK, long RELEASE. Then a pluck: everything short and SUSTAIN at zero." },
          { "logo antes da SAÍDA", "right before the OUTPUT" } },

        { "SAÍDA",
          { "OUTPUT", "OUTPUT" },
          { "SAÍDA DE ÁUDIO", "AUDIO OUTPUT" },
          { "O fim do caminho: daqui o som vai para o seu DAW e para a caixa de som. Sem ela nada sai, por mais que o resto esteja montado.",
            "The end of the path: from here the sound goes to your DAW and your speakers. Without it nothing comes out, however complete the rest is." },
          { "VOLUME: o volume final. O medidor lá no alto mostra o que está saindo.",
            "VOLUME: the final level. The meter at the top shows what is coming out." },
          { "Tire a SAÍDA do rack e toque. Depois devolva. Percebeu que tudo depende dela?",
            "Take the OUTPUT out of the rack and play. Then put it back. See how everything depends on it?" },
          { "no fim do caminho, à direita", "at the end of the path, on the right" } },

        { "ENV MOD",
          { "ENVELOPE DO FILTRO", "FILTER ENVELOPE" },
          { "ENVELOPE DE MODULAÇÃO", "MODULATION ENVELOPE" },
          { "Um segundo envelope, que não faz som: ele abre e fecha o filtro sozinho a cada nota. É o \"uou\" dos baixos de synth.",
            "A second envelope that makes no sound: it opens and closes the filter by itself on every note. It is the \"wow\" of synth basses." },
          { "ATTACK, DECAY, SUSTAIN e RELEASE: o mesmo desenho do ENV AMP, só que mexendo no corte do filtro.\n"
            "Quanto ele mexe, quem decide é o CONTOUR do VCF.",
            "ATTACK, DECAY, SUSTAIN and RELEASE: the same shape as ENV AMP, but moving the filter cutoff.\n"
            "How much it moves is set by the VCF CONTOUR." },
          { "Feche o CUTOFF, suba o CONTOUR, encurte o DECAY e toque. Ouviu o \"uou\"?",
            "Close CUTOFF, raise CONTOUR, shorten DECAY and play. Did you hear the \"wow\"?" },
          { "embaixo do VCF, encaixado nele", "under the VCF, locked into it" } },
    };
    return t[juce::jlimit (0, (int) numModules - 1, m)];
}

struct LessonText
{
    Txt title;
    Txt goal;
    Txt done;
};

inline const LessonText& lessonText (int lesson)
{
    static const LessonText t[numLessons] = {
        { { "O PRIMEIRO SOM", "The first sound" },
          { "Arraste o VCO e a SAÍDA da bandeja para o rack e toque uma nota. As placas cinza (PONTE) deixam o som passar direto por enquanto.",
            "Drag the VCO and the OUTPUT from the tray into the rack and play a note. The grey plates (BRIDGE) let the sound straight through for now." },
          { "O som saiu! Repare que ele não para nunca: o oscilador vibra o tempo todo. Quem desliga o som é o ENV AMP, a próxima peça.",
            "Sound! Notice it never stops: the oscillator vibrates all the time. ENV AMP, the next part, is what switches it off." } },
        { { "O VOLUME QUE RESPIRA", "Breathing loudness" },
          { "Encaixe o ENV AMP no lugar da ponte, antes da SAÍDA. Agora o som só toca enquanto a tecla está apertada.",
            "Fit ENV AMP where the bridge was, before the OUTPUT. Now the sound only plays while the key is held." },
          { "Agora a nota começa e termina. Brinque com o ATTACK e o RELEASE e ouça a diferença.",
            "Now the note starts and ends. Play with ATTACK and RELEASE and hear the difference." } },
        { { "O FILTRO", "The filter" },
          { "Encaixe o VCF entre o VCO e o ENV AMP. Gire o CUTOFF e ouça o som escurecer e clarear.",
            "Fit the VCF between the VCO and ENV AMP. Turn CUTOFF and hear the sound get darker and brighter." },
          { "Você já tem o caminho clássico: oscilador, filtro e volume. Suba o EMPHASIS e varra o CUTOFF.",
            "You now have the classic path: oscillator, filter and loudness. Raise EMPHASIS and sweep CUTOFF." } },
        { { "O FILTRO QUE SE MEXE", "The moving filter" },
          { "Encaixe o ENV MOD embaixo do VCF. Feche o CUTOFF, suba o CONTOUR e toque: o filtro abre sozinho a cada nota.",
            "Fit ENV MOD under the VCF. Close CUTOFF, raise CONTOUR and play: the filter opens by itself on every note." },
          { "Esse é o \"uou\" do synth. Com DECAY curto vira baixo; com ATTACK lento vira um som que desabrocha.",
            "That is the synth \"wow\". A short DECAY makes a bass; a slow ATTACK makes a sound that blooms." } },
        { { "DOIS OSCILADORES", "Two oscillators" },
          { "Encaixe o MIXER depois do VCO e o VCO 2 embaixo do MIXER. Desafine um pouco o VCO 2 e ouça o som engordar.",
            "Fit the MIXER after the VCO and VCO 2 under the MIXER. Detune VCO 2 a little and hear the sound get fat." },
          { "Synth completo! O modo CABOS foi liberado no alto: lá você liga as peças com fios, em qualquer ordem.",
            "Full synth! CABLES mode is now unlocked at the top: there you connect the parts with wires, in any order." } },
        { { "MODO LIVRE", "Free play" },
          { "O synth completo é seu. Toque, experimente e, se quiser, passe para o modo CABOS no alto.",
            "The full synth is yours. Play, experiment and, if you like, switch to CABLES mode at the top." },
          { "", "" } },
    };
    return t[juce::jlimit (1, numLessons, lesson) - 1];
}

inline const Txt& txtCablesHelp()
{
    static const Txt t { "Arraste da tomada de saída (OUT) de uma peça até a entrada (IN) de outra. Você decide o caminho do som. "
                         "Para soltar um fio, puxe ele pela ponta da entrada.",
                         "Drag from a part's output socket (OUT) to another part's input (IN). You decide the sound path. "
                         "To remove a wire, pull it out by its input end." };
    return t;
}
} // namespace mm

#pragma once

#include <vector>
#include "Params.h"

// GROOVADOR — presets de fábrica. Cada um é ESTILO + SEMENTE: ao carregar, o plugin monta o kit do estilo, ajusta
// os timbres e o gerador pela semente e sorteia o groove com ela (sempre o mesmo groove para o mesmo preset).
// Os presets do usuário guardam tudo (valores + a grade). Travas, mudo/solo e o modo de tocar não mudam.
namespace grv
{
inline constexpr int numCategories = 6;      // = os estilos
inline const char* const categoryNames[numCategories] = { "PSY FULL", "PSY PROG", "TECHNO", "HOUSE", "TRIBAL", "BREAK" };

struct FactoryPreset
{
    const char* name;
    int style;          // −1 = Init (fora das categorias; usa o PSY FULL)
    int seed;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "Init", -1, 1 },
        { "Tribo de Goa", 0, 145 }, { "Ácido no Pé", 0, 211 }, { "Pista às 4h", 0, 377 }, { "Fogueira", 0, 409 },
        { "Rolante", 0, 523 }, { "Templo", 0, 617 }, { "Chacoalha", 0, 733 }, { "Maré Alta", 0, 811 },
        { "Trance de Areia", 0, 929 }, { "Faísca", 0, 1031 }, { "Cogumelo", 0, 1123 }, { "Sol Nascendo", 0, 1259 },

        { "Neblina", 1, 151 }, { "Passo Leve", 1, 263 }, { "Vale", 1, 359 }, { "Lua Cheia", 1, 467 },
        { "Correnteza", 1, 571 }, { "Horizonte", 1, 659 }, { "Vapor", 1, 761 }, { "Deriva", 1, 857 },
        { "Seda", 1, 953 }, { "Caminho", 1, 1049 }, { "Floresta", 1, 1151 }, { "Brisa", 1, 1277 },

        { "Galpão", 2, 157 }, { "Concreto", 2, 269 }, { "Ferro", 2, 367 }, { "Túnel", 2, 479 },
        { "Máquina", 2, 577 }, { "Fumaça", 2, 661 }, { "Pulso", 2, 769 }, { "Aço", 2, 863 },
        { "Fábrica", 2, 967 }, { "Hangar", 2, 1061 }, { "Sirene", 2, 1163 }, { "Turbina", 2, 1283 },

        { "Calçada", 3, 163 }, { "Varanda", 3, 271 }, { "Esquina", 3, 373 }, { "Pista Quente", 3, 487 },
        { "Sexta", 3, 587 }, { "Baile", 3, 673 }, { "Ladeira", 3, 773 }, { "Gingado", 3, 877 },
        { "Disco", 3, 971 }, { "Swingueira", 3, 1063 }, { "Telhado", 3, 1171 }, { "Domingo", 3, 1289 },

        { "Terreiro", 4, 167 }, { "Batuque", 4, 277 }, { "Roda", 4, 379 }, { "Atabaque", 4, 491 },
        { "Cerimônia", 4, 593 }, { "Aldeia", 4, 677 }, { "Tambor", 4, 787 }, { "Raiz", 4, 881 },
        { "Mata", 4, 977 }, { "Ritual", 4, 1069 }, { "Chão Batido", 4, 1181 }, { "Ancestral", 4, 1291 },

        { "Quebrada", 5, 173 }, { "Breque", 5, 281 }, { "Esquema", 5, 383 }, { "Rua", 5, 499 },
        { "Grafite", 5, 599 }, { "Pé Quebrado", 5, 683 }, { "Vinil", 5, 797 }, { "Sample", 5, 883 },
        { "Laje", 5, 983 }, { "Gira", 5, 1087 }, { "Rasteira", 5, 1187 }, { "Beco", 5, 1297 },
    };
    return list;
}

// o jeito de cada estilo: centro do gerador, da variação e dos efeitos (a semente mexe em volta)
struct StyleFeel
{
    float density, swing, ghost, roll, human;
    int   vmode, every;
    float amount;
    int   fill;
    float rvMix, dlMix;
};
inline constexpr StyleFeel styleFeel[numCategories] = {
    { 62.0f,  5.0f, 30.0f, 25.0f, 10.0f, 1, 1, 45.0f, 2, 12.0f, 14.0f },   // PSY FULL
    { 50.0f, 12.0f, 25.0f, 15.0f, 20.0f, 2, 2, 40.0f, 2, 22.0f, 20.0f },   // PSY PROG
    { 55.0f,  8.0f, 20.0f, 20.0f, 15.0f, 0, 2, 50.0f, 2, 16.0f, 16.0f },   // TECHNO
    { 60.0f, 35.0f, 35.0f, 10.0f, 30.0f, 0, 1, 35.0f, 2, 14.0f,  0.0f },   // HOUSE
    { 70.0f, 20.0f, 45.0f, 20.0f, 35.0f, 1, 1, 55.0f, 1, 18.0f,  0.0f },   // TRIBAL
    { 60.0f, 30.0f, 55.0f, 20.0f, 30.0f, 2, 1, 40.0f, 1, 12.0f,  0.0f },   // BREAK
};

// kit do estilo: o que muda em relação ao som padrão de cada instrumento
struct KitTweak { int style, inst; const char* param; float value; };
inline const std::vector<KitTweak>& kitTweaks()
{
    static const std::vector<KitTweak> list {
        { 0, hatC, "dec", 45.0f }, { 0, hatC, "tone", 9000.0f }, { 0, perc1, "penv", 18.0f }, { 0, perc2, "model", (float) fm },
        { 0, perc2, "tune", -5.0f }, { 0, clap, "dec", 200.0f },
        { 1, snare, "model", (float) m808 }, { 1, hatC, "dec", 70.0f }, { 1, hatC, "tone", 6500.0f }, { 1, perc1, "model", (float) m808 },
        { 1, perc1, "tune", 3.0f }, { 1, clap, "dec", 280.0f }, { 1, shaker, "level", -7.0f },
        { 2, rim, "model", (float) metal }, { 2, rim, "tone", 6000.0f }, { 2, clap, "dec", 320.0f }, { 2, clap, "tone", 1200.0f },
        { 2, hatC, "tone", 10000.0f }, { 2, hatO, "dec", 300.0f }, { 2, perc1, "tune", -5.0f }, { 2, perc2, "model", (float) fm },
        { 3, perc1, "model", (float) analog }, { 3, perc1, "tune", 5.0f }, { 3, perc1, "noise", 5.0f }, { 3, perc2, "tune", 3.0f },
        { 3, shaker, "level", -6.0f }, { 3, clap, "dec", 260.0f }, { 3, clap, "body", 90.0f },
        { 4, perc1, "model", (float) analog }, { 4, perc1, "tune", -3.0f }, { 4, perc1, "dec", 300.0f }, { 4, perc2, "tune", -7.0f },
        { 4, perc2, "dec", 420.0f }, { 4, rim, "tune", -4.0f }, { 4, shaker, "level", -6.0f },
        { 5, snare, "body", 70.0f }, { 5, snare, "noise", 65.0f }, { 5, snare, "dec", 230.0f }, { 5, snare, "drive", 45.0f },
        { 5, hatC, "model", (float) noise }, { 5, hatC, "dec", 50.0f }, { 5, hatO, "model", (float) noise }, { 5, rim, "level", -6.0f },
    };
    return list;
}
} // namespace grv

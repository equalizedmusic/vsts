#pragma once

// Escalas para SCALE + ROOT (o mesmo arquivo na QUIMERA e no LIMÃO; mudou um, copie no outro).
// O parâmetro de escala guarda o ÍNDICE em scaleNames (0 = CHROMATIC = desligado); o de ROOT, a nota (0 = C … 11 = B).

#include <juce_core/juce_core.h>
#include <initializer_list>

namespace scales
{
inline const juce::StringArray scaleNames { "CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                            "LOCRIAN", "HARM MINOR", "MEL MINOR", "PENTA MAJ", "PENTA MIN", "BLUES",
                                            "PHRYG DOM" };
inline const juce::StringArray rootNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

// bit n = n semitons acima da tônica está na escala
constexpr int mask (std::initializer_list<int> degrees)
{
    int m = 0;
    for (int d : degrees) m |= 1 << d;
    return m;
}

inline constexpr int numScales = 14;
inline constexpr int masks[numScales] = {
    0xfff,                                   // CHROMATIC
    mask ({ 0, 2, 4, 5, 7, 9, 11 }),         // MAJOR
    mask ({ 0, 2, 3, 5, 7, 8, 10 }),         // MINOR (natural)
    mask ({ 0, 2, 3, 5, 7, 9, 10 }),         // DORIAN
    mask ({ 0, 1, 3, 5, 7, 8, 10 }),         // PHRYGIAN
    mask ({ 0, 2, 4, 6, 7, 9, 11 }),         // LYDIAN
    mask ({ 0, 2, 4, 5, 7, 9, 10 }),         // MIXOLYDIAN
    mask ({ 0, 1, 3, 5, 6, 8, 10 }),         // LOCRIAN
    mask ({ 0, 2, 3, 5, 7, 8, 11 }),         // HARMONIC MINOR
    mask ({ 0, 2, 3, 5, 7, 9, 11 }),         // MELODIC MINOR
    mask ({ 0, 2, 4, 7, 9 }),                // PENTATONIC MAJOR
    mask ({ 0, 3, 5, 7, 10 }),               // PENTATONIC MINOR
    mask ({ 0, 3, 5, 6, 7, 10 }),            // BLUES
    mask ({ 0, 1, 4, 5, 7, 8, 10 }),         // PHRYGIAN DOMINANT
};

inline bool active (int scale) { return scale > 0 && scale < numScales; }

inline bool contains (int note, int scale, int root)
{
    if (! active (scale)) return true;
    const int d = ((note - root) % 12 + 12) % 12;
    return ((masks[scale] >> d) & 1) != 0;
}

// a nota da escala mais perto (empate: a de baixo)
inline int snap (int note, int scale, int root)
{
    if (! active (scale)) return note;
    for (int k = 0; k < 12; ++k)
    {
        if (contains (note - k, scale, root)) return note - k;
        if (contains (note + k, scale, root)) return note + k;
    }
    return note;
}

// próxima nota da escala para cima (dir > 0) ou para baixo (dir < 0); sem escala, um semitom
inline int step (int note, int dir, int scale, int root)
{
    const int s = dir >= 0 ? 1 : -1;
    int n = snap (note, scale, root);
    for (int k = 0; k < 12; ++k)
    {
        n += s;
        if (contains (n, scale, root)) return n;
    }
    return note + s;
}
} // namespace scales

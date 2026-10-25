#pragma once
// GERADO por tools/recorta.py a partir do mapa.json do sprites.py — não editar à mão.
// Posições em pixels no espaço da imagem renderizada (1000 x 1284).
namespace sk::mapa
{
struct E { float x, y, rx, ry; };   // centro e pixels por unidade do Blender (x e y)
struct P { float x, y; };
struct R { int x0, y0, x1, y1; };
constexpr float W = 1000.0f, H = 1284.0f, un = 24.43f;
constexpr E lamp[11] = {
    { 331.14f, 715.85f, 10.83f, 10.67f },
    { 274.04f, 622.63f, 10.77f, 10.56f },
    { 266.71f, 514.83f, 10.72f, 10.44f },
    { 309.95f, 416.05f, 10.66f, 10.33f },
    { 393.76f, 347.25f, 10.61f, 10.25f },
    { 500.0f, 322.65f, 10.6f, 10.22f },
    { 606.24f, 347.25f, 10.62f, 10.25f },
    { 690.05f, 416.05f, 10.66f, 10.33f },
    { 733.29f, 514.83f, 10.71f, 10.44f },
    { 725.96f, 622.63f, 10.78f, 10.56f },
    { 668.86f, 715.85f, 10.83f, 10.67f }
};
constexpr E knobTampa = { 500.0f, 530.99f, 25.02f, 24.38f }, knobSaia = { 500.0f, 551.34f, 24.39f, 23.79f }, knobBase = { 500.0f, 553.7f, 24.31f, 23.72f };
constexpr E rateTampa = { 452.16f, 990.11f, 25.18f, 25.15f }, mixTampa = { 776.95f, 990.11f, 25.18f, 25.15f };
constexpr P txtTrigger = { 285.08f, 1060.13f }, txtRate = { 452.52f, 1060.13f }, txtShape = { 634.95f, 1060.13f }, txtMix = { 774.9f, 1060.13f };
constexpr P alavanca = { 284.88f, 989.79f };
constexpr float visor[4] = { 563.39f, 947.6f, 705.97f, 1027.33f };
constexpr R retKnob = { 295, 335, 779, 830 }, retRate = { 396, 935, 533, 1079 }, retMix = { 719, 935, 859, 1079 };
constexpr R retAlav = { 224, 922, 352, 1057 }, retNome = { 190, 772, 810, 876 };
}

# MAPA — componentes de terceiros

## Recursos/modelo.als
Molde de set do Ableton Live usado pelo exportador .als (reduzido a 1 faixa MIDI, 1 de áudio e os returns).
Derivado de `DefaultLiveSet.als` do projeto logic2ableton — https://github.com/Evilander/logic2ableton

MIT License

Copyright (c) 2025 evilander

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## IA (MAPA 0.2)

| Componente | Uso no MAPA | Licença |
|---|---|---|
| Demucs v4 — htdemucs (Meta Research), https://github.com/facebookresearch/demucs | modelo de separação em stems (`htdemucs.onnx`, exportado no workflow `mapa-modelos.yml`) | MIT |
| demucs.onnx (Sevag H.), https://github.com/sevagh/demucs.onnx | código C++ do STFT e da inferência, em `terceiros/demucs_onnx` (ver README-MAPA.md e LICENSE ali) | MIT |
| Basic Pitch (Spotify AB), https://github.com/spotify/basic-pitch | modelo de notas `Recursos/nmp.onnx` e o pós-processamento portado para C++ em `Source/IA.cpp` | Apache 2.0 |
| ONNX Runtime 1.20.1 (Microsoft), pacote NuGet Microsoft.ML.OnnxRuntime | roda os dois modelos; `libonnxruntime.dylib` / `onnxruntime.dll` vão junto do app | MIT |
| Eigen 3.4 | matrizes e FFT do Demucs (só cabeçalhos, `EIGEN_MPL2_ONLY`) | MPL 2.0 |

Basic Pitch — Copyright 2022 Spotify AB. Licensed under the Apache License, Version 2.0
(http://www.apache.org/licenses/LICENSE-2.0). O MAPA usa o modelo sem alterações e uma tradução para C++ do código de
pós-processamento (basic_pitch/inference.py e note_creation.py).

Demucs — Copyright (c) Meta Platforms, Inc. and affiliates. MIT License.
demucs.onnx — Copyright (c) 2023 Sevag H. MIT License (texto completo em terceiros/demucs_onnx/LICENSE).
ONNX Runtime — Copyright (c) Microsoft Corporation. MIT License.

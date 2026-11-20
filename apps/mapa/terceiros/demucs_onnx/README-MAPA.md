# demucs.onnx (terceiros)

Código de https://github.com/sevagh/demucs.onnx (MIT, ver LICENSE), pasta `src/`, usado pelo MAPA para rodar o
Demucs v4 (htdemucs, Meta, MIT) exportado para ONNX (o STFT/iSTFT fica fora da rede, aqui em C++).

Mudanças do MAPA:
- include `<onnxruntime_cxx_api.h>` (pacote NuGet do ONNX Runtime) e `"tensor.hpp"`;
- semente fixa no deslocamento aleatório (mesma música = mesmos stems);
- sem mensagens no console.
- `allocator` e `run_options` viraram estáticos locais (a ordem de inicialização de globais quebrava no MinGW).

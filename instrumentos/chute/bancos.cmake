# Banco de FÁBRICA do CHUTE (os 50 presets) dentro do plugin: bancos/fabrica/FABRICA.chutebank vira BinaryData
# (ChuteBancos::FABRICA_chutebank). Sem chave nenhuma: o banco é um arquivo do repositório (formato em Source/Bancos.h,
# gerador em tools/banco.py). Os outros bancos (bancos/*.chutebank) NÃO entram no binário: vão no instalador.
# Uso: include(.../chute/bancos.cmake) e chute_bancos(<alvo>)
function(chute_bancos alvo)
    juce_add_binary_data(${alvo}_Bancos HEADER_NAME ChuteBancos.h NAMESPACE ChuteBancos
        SOURCES "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/bancos/fabrica/FABRICA.chutebank")
    set_target_properties(${alvo}_Bancos PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${alvo} PRIVATE ${alvo}_Bancos)
endfunction()

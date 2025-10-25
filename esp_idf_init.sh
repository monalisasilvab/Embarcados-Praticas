#!/bin/bash

# Script de inicialização do ESP-IDF
# Adiciona automaticamente as variáveis de ambiente do ESP-IDF

# Caminho para o ESP-IDF (ajuste conforme necessário)
export IDF_PATH=/home/juan/esp-idf

# Verificar se o ESP-IDF existe
if [ -d "$IDF_PATH" ]; then
    # Verificar se já foi inicializado para evitar execução dupla
    if [ -z "$IDF_TOOLS_PATH" ]; then
        echo "🔧 Inicializando ESP-IDF..."
        source $IDF_PATH/export.sh > /dev/null 2>&1
        echo "✅ ESP-IDF pronto! Versão: $(idf.py --version 2>/dev/null | head -1)"
    else
        echo "✅ ESP-IDF já está ativo!"
    fi
else
    echo "❌ ESP-IDF não encontrado em: $IDF_PATH"
    echo "💡 Ajuste o caminho em ~/.esp_idf_init.sh"
fi

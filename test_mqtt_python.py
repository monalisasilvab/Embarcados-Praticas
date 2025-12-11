#!/usr/bin/env python3
"""
Script para testar publicação MQTT no AWS IoT Core
Útil para verificar se o problema é no ESP32 ou na configuração AWS
"""

import time
import json
from awscrt import mqtt
from awsiot import mqtt_connection_builder

# ===== CONFIGURAÇÕES - EDITE AQUI =====
ENDPOINT = "a1gqpq2oiyi1r1-ats.iot.sa-east-1.amazonaws.com"
CLIENT_ID = "PythonTestClient"
TOPIC = "esp32/test"

# Caminhos dos certificados (ajuste conforme sua localização)
CERT_FILEPATH = "./main/certs/21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a-certificate.pem.crt"
PRIVATE_KEY_FILEPATH = "./main/certs/21799d82cb8e53178d50791a8bce841ab0132bdc3596a4350b798c0b5dc0925a-private.pem.key"
CA_FILEPATH = "./main/certs/AmazonRootCA1.pem"
# ======================================

def on_connection_interrupted(connection, error, **kwargs):
    print(f"❌ Conexão interrompida. Erro: {error}")

def on_connection_resumed(connection, return_code, session_present, **kwargs):
    print(f"✅ Conexão retomada. Return code: {return_code}")

def main():
    print("=" * 60)
    print("🧪 TESTE DE PUBLICAÇÃO MQTT - AWS IOT CORE")
    print("=" * 60)
    print(f"📡 Endpoint: {ENDPOINT}")
    print(f"🔐 Client ID: {CLIENT_ID}")
    print(f"📤 Tópico: {TOPIC}")
    print("=" * 60)
    
    # Cria conexão MQTT
    print("\n🔌 Conectando ao AWS IoT Core...")
    mqtt_connection = mqtt_connection_builder.mtls_from_path(
        endpoint=ENDPOINT,
        cert_filepath=CERT_FILEPATH,
        pri_key_filepath=PRIVATE_KEY_FILEPATH,
        ca_filepath=CA_FILEPATH,
        client_id=CLIENT_ID,
        clean_session=False,
        keep_alive_secs=30,
        on_connection_interrupted=on_connection_interrupted,
        on_connection_resumed=on_connection_resumed
    )
    
    connect_future = mqtt_connection.connect()
    connect_future.result()
    print("✅ Conectado ao AWS IoT Core!")
    
    # Publica mensagens de teste
    print(f"\n📤 Publicando mensagens no tópico '{TOPIC}'...\n")
    
    messages = ["teste", "hello world"]
    
    for i in range(10):
        message = messages[i % 2]
        payload = message
        
        print(f"[#{i+1}] Publicando: '{payload}'")
        
        mqtt_connection.publish(
            topic=TOPIC,
            payload=payload,
            qos=mqtt.QoS.AT_LEAST_ONCE
        )
        
        time.sleep(1)
    
    print("\n✅ Todas as mensagens publicadas!")
    print("\n🔍 Verifique no AWS IoT Core Test Client se as mensagens chegaram")
    print(f"   Inscreva-se no tópico: {TOPIC} ou #")
    
    # Desconecta
    print("\n🔌 Desconectando...")
    disconnect_future = mqtt_connection.disconnect()
    disconnect_future.result()
    print("✅ Desconectado!")

if __name__ == "__main__":
    try:
        main()
    except FileNotFoundError as e:
        print(f"\n❌ ERRO: Arquivo não encontrado!")
        print(f"   {e}")
        print("\n💡 Verifique os caminhos dos certificados no script")
    except Exception as e:
        print(f"\n❌ ERRO: {e}")
        print("\n💡 Possíveis causas:")
        print("   1. Certificados incorretos ou expirados")
        print("   2. Endpoint incorreto")
        print("   3. Política AWS IoT não permite publicação")
        print("   4. Bibliotecas AWS IoT Python não instaladas")
        print("\n📦 Para instalar as bibliotecas:")
        print("   pip install awsiotsdk")

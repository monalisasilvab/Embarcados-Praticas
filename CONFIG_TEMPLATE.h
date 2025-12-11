/*
 * INSTRUÇÕES PARA CONFIGURAÇÃO
 * ============================
 * 
 * 1. CREDENCIAIS WIFI:
 *    Substitua "SEU_WIFI_SSID" e "SUA_SENHA_WIFI" pelas suas credenciais reais
 * 
 * 2. ENDPOINT AWS IoT:
 *    Substitua "seu-endpoint.iot.us-east-1.amazonaws.com" pelo seu endpoint real
 *    Encontre em: AWS Console > IoT Core > Configurações > Endpoint do dispositivo
 * 
 * 3. CERTIFICADOS:
 *    Substitua os arquivos em main/certs/ pelos seus certificados reais:
 *    - certificate.pem.crt (certificado do dispositivo)
 *    - private.pem.key (chave privada do dispositivo)
 *    - aws-root-ca.pem (já está correto)
 * 
 * 4. TÓPICO MQTT:
 *    Por padrão usa "esp32/data" - você pode alterar se necessário
 * 
 * 5. CLIENT ID:
 *    Por padrão usa "ESP32_Client" - você pode alterar se necessário
 */

// CONFIGURAÇÕES WIFI - OBRIGATÓRIO ALTERAR
#define WIFI_SSID "SEU_WIFI_SSID"        // ← Altere aqui
#define WIFI_PASS "SUA_SENHA_WIFI"       // ← Altere aqui

// CONFIGURAÇÕES AWS IoT - OBRIGATÓRIO ALTERAR
#define AWS_IOT_ENDPOINT "seu-endpoint.iot.us-east-1.amazonaws.com"  // ← Altere aqui
#define AWS_IOT_TOPIC "esp32/data"       // ← Opcional alterar
#define AWS_IOT_CLIENT_ID "ESP32_Client" // ← Opcional alterar

/*
 * EXEMPLO DE ENDPOINT REAL:
 * #define AWS_IOT_ENDPOINT "a1b2c3d4e5f6g7.iot.us-east-1.amazonaws.com"
 * 
 * EXEMPLO DE CREDENCIAIS WIFI:
 * #define WIFI_SSID "MinhaRede2.4G"
 * #define WIFI_PASS "minhaSenhaSecreta123"
 */

<h1 align="center"> Proccio 🚀 </h1>
<h4 align="center">
  Sistema completo de gerenciamento de tags RFiD que roda em um microcontrolador ESP8266. 
  Ele hospeda uma página de cadastro e publica informações via MQTT.
</h4>
<br />



# :pushpin: Tabela de Conteúdos

  - [ :gear: Como usar](#gear-como-usar)
  - [ :construction_worker: Créditos](#construction_worker-créditos)
  - [ :page_facing_up: Licença](#page_facing_up-licença)



# :gear: Como usar

1. Clone este repositório:
   ```
   git clone https://github.com/inpacta/Proccio.git
   ```

2. Configure as credenciais do Wi-Fi no arquivo `./src/main.cpp`:
   ```cpp
   const char* ssid = "NOME_DA_REDE";
   const char* password = "SENHA_DO_WIFI";
   ```

3. Configure as credenciais do Broker MQTT no arquivo `./src/main.cpp`:
   ```cpp
   const char* mqttServer = "endereco_do_broker";
   const int mqttPort = 1883;
   const char* mqttUser = "usuario"; // Opcional
   const char* mqttPassword = "senha"; // Opcional
   ```

4. Configure os tópicos para as informações `./src/main.cpp` :
   ```cpp
   const char* topicStorageInfoSPIFFS = "SEU/TOPICO";
    const char* topicStorageInfoFLASH = "SEU/TOPICO";
    const char* topicStorageContent = "SEU/TOPICO";
    const char* topicLastAcess = "SEU/TOPICO";
    const char* topicLock = "SEU/TOPICO";
    const char* topicUnlock = "SEU/TOPICO";
    const char* topicLog = "SEU/TOPICO";
   ```

5. Compile e faça o upload para o ESP8266 usando o Platformio.
    ```cpp
    /* O projeto acima, está baseado na utilização do Platformio, extensão do Visual Studio Code.
    Aqui está um tutorial para que você consiga subir os arquivos para o repositório SPIFFS do ESP. */
    ```

    - [SPIFFS Upload](https://www.youtube.com/watch?v=Pxpg9eZLoBI)
    - [Como baixar e instalar Vscode e a extensão Platformio](https://www.youtube.com/watch?v=OZJ4niOrJ2k)
  
# :construction_worker: Créditos

O projeto está baseado no projeto pré configurado do blog [smartkits](https://blog.smartkits.com.br/esp8266-cadastro-rfid-mfrc522-com-webserver/)

Então, para mais informações sobre o circuito ou como passar o código utilizando o Arduino IDE. <a href="https://blog.smartkits.com.br/esp8266-cadastro-rfid-mfrc522-com-webserver/">Clique aqui</a>


# :page_facing_up: Licença

Contribuições são bem-vindas! Sinta-se à vontade para abrir issues ou enviar pull requests.

Este projeto está sob a Licença MIT. Para mais informações sobre, <a href="/LICENSE">Clique aqui</a>


<img src="https://github.com/inpacta/.github/blob/main/profile/InPACTA-logo.png" alt="InPACTA Logo" width="100" align="right" />

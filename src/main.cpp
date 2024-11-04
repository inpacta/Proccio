#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <FS.h>
#include <SPI.h>
#include <MFRC522.h>
#include <vector>

// Define os Pinos utilizados
#define SS_PIN 2  //D2
#define RST_PIN 0 //D3
#define FILENAME "/Cadastro.txt"
#define LED_RED 16 //D0
#define LED_GREEN 4 //D2
#define LED_BLUE 1 //TX
#define Buzzer 5 //D1

// Define as frequências das notas musicais
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_G4  392
#define NOTE_C5  523
#define NOTE_G3  196
#define NOTE_D4  294

// Variáveis globais
int internal_condition = 0;
const int attemptsLimit = 3;

using namespace std;

// Informações de conexão
const char* ssid = "NOME_DA_REDE";
const char* password = "SENHA_DO_WIFI";
const char* mqttServer = "endereco_do_broker";
const int mqttPort = 1883;
const char* mqttUser = "usuario"; // Opcional
const char* mqttPassword = "senha"; // Opcional

// Tópicos MQTT

const char* topicStorageInfoSPIFFS = "SEU/TOPICO";
const char* topicStorageInfoFLASH = "SEU/TOPICO";
const char* topicStorageContent = "SEU/TOPICO";
const char* topicLastAcess = "SEU/TOPICO";
const char* topicLock = "SEU/TOPICO";
const char* topicUnlock = "SEU/TOPICO";
const char* topicLog = "SEU/TOPICO";


String info_data; //Informação sobre o usuario.Ex nome, cpf, etc.
String id_data;   //Id para o usuario.
int index_user_for_removal = -1;

String rfid_card = ""; //UID RFID obtido pelo Leitor
String sucess_msg = ""; 
String failure_msg = "";

String myIp = "";

// Cria os objetos necessários
WiFiClient espClient;
MFRC522 mfrc522(SS_PIN, RST_PIN);   
PubSubClient client(espClient);
AsyncWebServer server(80);

//Função para tratar requisições não encontradas.
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//Inicializa o sistema de arquivos.
bool initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("Erro ao abrir o sistema de arquivos");
    return false;
  }
  Serial.println("Sistema de arquivos carregado com sucesso!");
  return true;
}

// Função para tocar o som de inicialização
void playStartupTone() {
  tone(Buzzer, NOTE_C4, 200);
  delay(250); // Espera a nota terminar + um pequeno intervalo
  tone(Buzzer, NOTE_E4, 200);
  delay(250);
  tone(Buzzer, NOTE_G4, 200);
  delay(250);
  tone(Buzzer, NOTE_C5, 200);
  delay(250);
  noTone(Buzzer); // Para de tocar
}

// Função para tocar o som de usuário aceito
void playUserAcceptedTone() {
  tone(Buzzer, NOTE_G4, 100);
  delay(150);
  tone(Buzzer, NOTE_C5, 100);
  delay(150);
  noTone(Buzzer); // Para de tocar
}

// Função para tocar o som de usuário negado
void playUserDeniedTone() {
  tone(Buzzer, NOTE_C4, 100);
  delay(150);
  tone(Buzzer, NOTE_G3, 100);
  delay(150);
  noTone(Buzzer); // Para de tocar
}

// Função para tocar o som de usuário adicionado ou retirado
void playUserAddedOrRemovedTone() {
  tone(Buzzer, NOTE_D4, 200);
  delay(250); // Espera a nota terminar + um pequeno intervalo
  tone(Buzzer, NOTE_E4, 200);
  delay(250);
  tone(Buzzer, NOTE_C4, 200);
  delay(250);
  noTone(Buzzer); // Para de tocar
}

// Publica informações de armazenamento no MQTT
void publishStorageInfo() {
  FSInfo fs_info;
  SPIFFS.info(fs_info);

  // Informações do SPIFFS
  unsigned long totalBytesSPIFFS = fs_info.totalBytes / 1024;
  unsigned long usedBytesSPIFFS = fs_info.usedBytes / 1024;
  float percentUsedSPIFFS = (float)usedBytesSPIFFS / (float)totalBytesSPIFFS * 100;
  String messageSPIFFS = "Espaço Usado: " + String(usedBytesSPIFFS) + "KB, Espaço Total: " + String(totalBytesSPIFFS) + "KB, Usado: " + String(percentUsedSPIFFS, 2) + "%";

  // Publica a mensagem no tópico SPIFFS
  if (!client.publish(topicStorageInfoSPIFFS, messageSPIFFS.c_str())) Serial.println("Falha ao publicar informações de armazenamento do SPIFFS");
  else Serial.println("Informações de armazenamento do SPIFFS publicadas com sucesso");

  // Informações do Flash
  uint32_t sketchSize = ESP.getSketchSize() / 1024;
  uint32_t freeSketchSpace = ESP.getFreeSketchSpace() / 1024;
  uint32_t flashChipSize = ESP.getFlashChipRealSize() / 1024;
  unsigned long totalUsedFlash = (usedBytesSPIFFS + sketchSize);
  float percentUsedFlash = (float)totalUsedFlash / (float)flashChipSize * 100;
  String messageFLASH = "Programa - Tamanho: " + String(sketchSize) + "KB, Espaço Livre: " + String(freeSketchSpace) + "KB, Chip de Flash - Tamanho Total: " + String(flashChipSize) + "KB, Usado: " + String(percentUsedFlash, 2) + "%";

  // Publica a mensagem no tópico Flash
  if (!client.publish(topicStorageInfoFLASH, messageFLASH.c_str())) Serial.println("Falha ao publicar informações de armazenamento do Flash");
  else Serial.println("Informações de armazenamento do Flash publicadas com sucesso");

}

//Lista todos os arquivos salvos na flash.
void listAllFiles() {
  String str = "";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    str += dir.fileName();
    str += " / ";
    str += dir.fileSize();
    str += "\r\n";
  }
  Serial.print(str);
}

//Faça a leitura de um arquivo e retorne um vetor com todas as linhas.
vector <String> readFile(String path) {
  vector <String> file_lines;
  String content;
  File myFile = SPIFFS.open(path.c_str(), "r");
  if (!myFile) {
    myFile.close();
    return {};
  }
  Serial.println("###################### - FILE- ############################");
  while (myFile.available()) {
    content = myFile.readStringUntil('\n');
    file_lines.push_back(content);
    Serial.println(content);
  }
  Serial.println("###########################################################");
  myFile.close();
  return file_lines;
}


//Faça a busca de um usuario pelo ID e pela INFO.
int findUser(vector <String> users_data, String id, String info) {
  String newID = "<td>" + id + "</td>";
  String newinfo = "<td>" + info + "</td>";

  for (int i = 0; i < users_data.size(); i++) {
    if (users_data[i].indexOf(newID) > 0 || users_data[i].indexOf(newinfo) > 0) {
      return i;
    }
  }
  return -1;
}

//Adiciona um novo usuario ao sistema
bool addNewUser(String id, String data) {
  File myFile = SPIFFS.open(FILENAME, "a+");
  if (!myFile) {
    Serial.println("Erro ao abrir arquivo!");
    myFile.close();
    return false;
  } else {
    myFile.printf("<tr><td>%s</td><td>%s</td>\n", id.c_str(), data.c_str());
    Serial.println("Arquivo gravado");
  }
  myFile.close();
  publishStorageInfo();
  playUserAddedOrRemovedTone();

  return true;
}

//Remove um usuario do sistema
bool removeUser(int user_index) {
  vector <String> users_data = readFile(FILENAME);
  if (user_index == -1)//Caso usuário não exista retorne falso
    return false;

  File myFile = SPIFFS.open(FILENAME, "w");
  if (!myFile) {
    Serial.println("Erro ao abrir arquivo!");
    myFile.close();
    return false;
  } else {
    for (int i = 0; i < users_data.size(); i++) {
      if (i != user_index)
        myFile.println(users_data[i]);
    }
    Serial.println("Usuário removido");
  }
  myFile.close();
  publishStorageInfo();
  playUserAddedOrRemovedTone();
  
  return true;
}

//Esta função substitui trechos de paginas html marcadas entre %
String processor(const String& var) {
  String msg = "";
  if (var == "TABLE") {
    msg = "<table><tr><td>RFID Code</td><td>User Info</td><td>Delete</td></tr>";
    vector <String> lines = readFile(FILENAME);
    for (int i = 0; i < lines.size(); i++) {
      msg += lines[i];
      msg += "<td><a href=\"get?remove=" + String(i + 1) + "\"><button>Excluir</button></a></td></tr>"; //Adiciona um botão com um link para o indice do usuário na tabela
    }
    msg += "</table>";
  }
  else if (var == "SUCESS_MSG")
    msg = sucess_msg;
  else if (var == "FAILURE_MSG")
    msg = failure_msg;
  return msg;
}

// Reconecta ao MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    String mac = WiFi.macAddress();
    if (client.connect(mac.c_str(), mqttUser, mqttPassword)) {
      Serial.println("Conectado");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// Configuração inicial
void setup() {
  Serial.begin(115200);

  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, LOW);

  SPI.begin();
  mfrc522.PCD_Init();   // Inicia MFRC522

  // Inicialize o SPIFFS
  if (!initFS())
    return;
  listAllFiles();
  readFile(FILENAME);

  // Conectando ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando WiFi..");
  }

  myIp = WiFi.localIP().toString();

  Serial.println("Conectado a rede WiFi");
  Serial.println("SSID: " + WiFi.SSID() + " Senha: " + WiFi.psk());
  Serial.print("Endereço IP: ");
  Serial.println(myIp);

  // Conectando ao servidor MQTT
  client.setServer(mqttServer, mqttPort);
  reconnectMQTT();

  //Rotas.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    rfid_card = "";
    request->send(SPIFFS, "/home.html", String(), false, processor);
  });
  server.on("/home", HTTP_GET, [](AsyncWebServerRequest * request) {
    rfid_card = "";
    request->send(SPIFFS, "/home.html", String(), false, processor);
  });
  server.on("/sucess", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/sucess.html", String(), false, processor);
  });
  server.on("/warning", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/warning.html");
  });
  server.on("/failure", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/failure.html");
  });
  server.on("/deleteuser", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (removeUser(index_user_for_removal)) {
      sucess_msg = "Usuário excluido do registro.";
      request->send(SPIFFS, "/sucess.html", String(), false, processor);
    }
    else
      request->send(SPIFFS, "/failure.html", String(), false, processor);
    return;
  });
  server.on("/stylesheet.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/stylesheet.css", "text/css");
  });
  server.on("/rfid", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", rfid_card.c_str());
  });
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    vector <String> users_data = readFile(FILENAME);

    if (request->hasParam("info")) {
      info_data = request->getParam("info")->value();
      info_data.toUpperCase();
      Serial.printf("info: %s\n", info_data.c_str());
    }
    if (request->hasParam("rfid")) {
      id_data = request->getParam("rfid")->value();
      Serial.printf("ID: %s\n", id_data.c_str());
    }
    if (request->hasParam("remove")) {
      String user_removed = request->getParam("remove")->value();
      Serial.printf("Remover o usuário da posição : %s\n", user_removed.c_str());
      index_user_for_removal = user_removed.toInt();
      index_user_for_removal -= 1;
      request->send(SPIFFS, "/warning.html");
      return;
    }
    if(id_data == "" || info_data == ""){
      failure_msg = "Informações de usuário estão incompletas.";
      request->send(SPIFFS, "/failure.html", String(), false, processor);
      return;
      }
    int user_index = findUser(users_data, id_data, info_data);
    if (user_index < 0) {
      Serial.println("Cadastrando novo usuário");
      addNewUser(id_data, info_data);
      sucess_msg = "Novo usuário cadastrado.";
      request->send(SPIFFS, "/sucess.html", String(), false, processor);
    }
    else {
      Serial.printf("Usuário numero %d ja existe no banco de dados\n", user_index);
      failure_msg = "Ja existe um usuário cadastrado.";
      request->send(SPIFFS, "/failure.html", String(), false, processor);
    }
  });
  server.on("/logo.jpg", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/logo.jpg", "image/jpg");
});
  
  server.onNotFound(notFound);
  // Inicia o serviço
  server.begin();

  // Pisca o LED azul para indicar que o servidor foi iniciado
  digitalWrite(LED_BLUE, HIGH);
  delay(100);
  digitalWrite(LED_BLUE, LOW);
  delay(100);
  digitalWrite(LED_BLUE, HIGH);
  delay(100);
  digitalWrite(LED_BLUE, LOW);
  delay(100);
  digitalWrite(LED_BLUE, HIGH);
  delay(100);
  digitalWrite(LED_BLUE, LOW);
  delay(100);

  playStartupTone();

  Serial.println("Servidor iniciado!");

  // Publica informações de armazenamento no MQTT
  publishStorageInfo();
  
  client.publish(topicLog, myIp.c_str());

}

// Loop principal
void loop() {
  // Reconecta ao MQTT se a conexão for perdida
  if (!client.connected()) reconnectMQTT();

  // Procure por novos cartões.
  if (!mfrc522.PICC_IsNewCardPresent()) return;

  //Faça a leitura do ID do cartão
  if (mfrc522.PICC_ReadCardSerial() && internal_condition <= attemptsLimit) {
    Serial.print("UID da tag :");
    String rfid_data = "";
    for (uint8_t i = 0; i < mfrc522.uid.size; i++)
    {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
      rfid_data.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
      rfid_data.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    Serial.println();
    rfid_data.toUpperCase();
    rfid_card = rfid_data;

    //Carregue o arquivo de cadastro
    vector <String> users_data = readFile(FILENAME);
    //Faça uma busca pelo id
    int user_index = findUser(users_data, rfid_data, "");
    
    // Verifica se o usuário foi encontrado
    if (user_index < 0) {
      Serial.printf("Nenhum usuário encontrado\n");
      tone(Buzzer, 1000, 500);
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_RED, LOW);
      playUserDeniedTone();
      internal_condition ++;
    } else {
      // Extrai o nome do usuário da entrada encontrada
      String user_entry = users_data[user_index];
      int startNameIndex = user_entry.indexOf("<td>", user_entry.indexOf("</td>") + 4) + 4; // Localiza o início do nome do usuário
      int endNameIndex = user_entry.indexOf("</td>", startNameIndex); // Localiza o fim do nome do usuário
      String user_name = user_entry.substring(startNameIndex, endNameIndex); // Extrai o nome do usuário
      user_name.trim(); // Remove espaços em branco antes e depois do nome

      Serial.printf("Usuário encontrado: %s\n", user_name.c_str());
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_GREEN, LOW);
      playUserAcceptedTone();
      
      // Publica o nome do usuário no tópico MQTT
      if (!client.publish(topicLastAcess, user_name.c_str())) Serial.println("Falha ao publicar o nome do usuário");
      else Serial.println("Nome do usuário publicado com sucesso");
      internal_condition = 0;
    }

    // Aguarda 1 segundo entre as leituras
    delay(1000);

    // Verifica se o limite de tentativas foi atingido
    if (internal_condition >= attemptsLimit){

      // Reseta a condição interna
      internal_condition = 0;

      // Liga a cor roxa para indicar que o sistema está bloqueado
      digitalWrite(LED_BLUE, LOW);
      digitalWrite(LED_RED, LOW);
      Serial.printf("Após tentativas negadas consecutivas, o sistema irá aguardar 10 segundos de bloqueio!\n");

      // Publica informações de log no MQTT
      if (!client.publish(topicLock, "Houveram tentativas de acesso negadas consecutivas, sistema bloqueado!")) Serial.println("Falha ao publicar log de bloqueio");
      else Serial.println("log de bloqueio publicado com sucesso");

      // Espera 10 segundos antes de desbloquear o sistema
      delay(10000);

      // Verifica se a conexão com o MQTT foi perdida
      if (!client.connected()) reconnectMQTT();

      // Publica informações de log no MQTT
      if (!client.publish(topicUnlock, "Sistema desbloqueado!")) Serial.println("Falha ao publicar log de desbloqueio");
      else Serial.println("log de bloqueio publicado com sucesso");
      Serial.printf("Sistema desbloqueado!\n");

      // Pisca o LED verde para indicar que o sistema foi desbloqueado
      digitalWrite(LED_BLUE, HIGH);
      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GREEN,LOW);
      delay(100);
      digitalWrite(LED_GREEN, HIGH);
      delay(100);
      digitalWrite(LED_GREEN,LOW);
      delay(100);
      digitalWrite(LED_GREEN, HIGH);
      delay(100);
      digitalWrite(LED_GREEN,LOW);
      delay(100);
      digitalWrite(LED_GREEN, HIGH);
      delay(100);
    } 

    // Acende o LED azul para indicar que o sistema está em standby
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_BLUE, LOW);
    Serial.println();

    // Aguarda 1 segundo entre as leituras
    delay(1000);
  }
}

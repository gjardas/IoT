#include <WiFi.h>
#include <HTTPClient.h>

// Definições de Pinos
#define BUZZER_PIN   14
#define GREEN_LED    26
#define RED_LED      25
#define RELAY_PIN    27

// Constantes de Conexão
const char* ssid = "Wokwi-GUEST";
const char* password = ""; 

const char* firebaseURL = "https://mottu-e25a9-default-rtdb.firebaseio.com";
const int TOTAL_VAGAS = 5; 
const char* DEVICE_ID = "WOKWI_C"; 

const char* firebaseLogPath = "/movimento.json";
const char* firebaseOccupancyPath = "/estacionadas.json"; 

const int NUM_MOTOS_VALIDAS = 5; 

const char* MOTO_IDS[NUM_MOTOS_VALIDAS] = {
    "RFID-HNIPORNE", 
    "RFID002", 
    "RFID003",
    "RFID004",
    "RFID005"
};

// --- Protótipos das Funções ---
void entradaMoto(String id, String vaga);
void saidaMoto(String id, String vaga);
void acessoNegado(String id);

bool isValidFormat(String input);
void logMovimento(String id, String status, String vaga);
int findMotoIndex(String id);

String readOccupancyJson();
String findOccupiedVaga(String id, String allOccupancyJson); 
String findNextAvailableVaga(String allOccupancyJson);       
void occupyVaga(String vaga, String id);                    
void releaseVaga(String vaga);                              
int countOccupiedVagas(String allOccupancyJson);            


void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(RELAY_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  String currentOccupancyJson = readOccupancyJson();
  Serial.println("\nWi-Fi conectado!");
  Serial.println("--- Sistema de Estacionamento ---");
  Serial.print("ID do Aparelho: "); Serial.println(DEVICE_ID);
  Serial.print("Total de vagas: "); Serial.println(TOTAL_VAGAS);
  Serial.print("Vagas ocupadas: "); Serial.println(countOccupiedVagas(currentOccupancyJson));
  Serial.println("Digite o ID da moto (ex: RFID001):"); // Mensagem de exemplo atualizada
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (isValidFormat(input)) {
      
      int motoIndex = findMotoIndex(input);

      if (motoIndex != -1) {
        
        String allOccupancyJson = readOccupancyJson();
        int currentCount = countOccupiedVagas(allOccupancyJson);
        String occupiedVaga = findOccupiedVaga(input, allOccupancyJson);

        if (!occupiedVaga.equals("")) {
          saidaMoto(input, occupiedVaga); 
        } else {
          if (currentCount < TOTAL_VAGAS) {
            String nextVaga = findNextAvailableVaga(allOccupancyJson);
            entradaMoto(input, nextVaga);
          } else {
            Serial.println("Estacionamento lotado!");
            acessoNegado(input);
          }
        }
      } else {
        acessoNegado(input);
      }
    } else {
      Serial.println("Formato inválido."); 
    }
  }
}

bool isValidFormat(String input) {
  return input.length() > 3;
}

void entradaMoto(String id, String vaga) {
  Serial.print("Entrada autorizada na vaga: "); Serial.println(vaga);
  occupyVaga(vaga, id); // Ocupa a vaga no Firebase
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RELAY_PIN, HIGH);
  logMovimento(id, "entrada", vaga); // Passa a vaga para o log
  delay(3000);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RELAY_PIN, LOW);
}

void saidaMoto(String id, String vaga) {
  Serial.print("Saída registrada da vaga: "); Serial.println(vaga);
  releaseVaga(vaga); // Libera a vaga no Firebase
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(RED_LED, HIGH);
  logMovimento(id, "saida", vaga); // Passa a vaga para o log
  delay(3000);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
}

void acessoNegado(String id) {
  Serial.println("ID não reconhecido ou Estacionamento lotado.");
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(RED_LED, HIGH);
  logMovimento(id, "negado", "N/A"); 
  delay(3000);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
}

// Função para encontrar o índice (posição) de um ID válido
int findMotoIndex(String id) {
    for (int i = 0; i < NUM_MOTOS_VALIDAS; i++) {
        if (id.equals(MOTO_IDS[i])) {
            return i; 
        }
    }
    return -1; // Retorna -1 se não encontrar
}

// FUNÇÃO logMovimento 
void logMovimento(String id, String status, String vaga) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String fullUrl = String(firebaseURL) + String(firebaseLogPath);
    http.begin(fullUrl);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"device\": \"" + String(DEVICE_ID) + "\","; 
    payload += "\"id\": \"" + id + "\",";      
    payload += "\"vaga\": \"" + vaga + "\",";     
    payload += "\"status\": \"" + status + "\",";
    payload += "\"timestamp\": " + String(millis());
    payload += "}";

    int responseCode = http.POST(payload);
    Serial.print("Log Firebase (movimento): ");
    Serial.println(responseCode);
    http.end();
  } else {
    Serial.println("Wi-Fi desconectado!");
  }
}

String readOccupancyJson() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String fullUrl = String(firebaseURL) + String(firebaseOccupancyPath);
    http.begin(fullUrl);

    int responseCode = http.GET();
    if (responseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();
      return payload; 
    } else if (responseCode == HTTP_CODE_NO_CONTENT || responseCode == HTTP_CODE_NOT_FOUND) {
      http.end();
      return "{}";
    } else {
      Serial.print("Erro ao ler ocupacao (GET): ");
      Serial.println(responseCode);
      http.end();
      return "{}";
    }
  } else {
    return "{}";
  }
}

String findNextAvailableVaga(String allOccupancyJson) {
  for (int i = 1; i <= TOTAL_VAGAS; i++) {
    String vaga = "vaga_" + String(i);
    
    String searchKey = "\"" + vaga + "\":";
    if (allOccupancyJson.indexOf(searchKey) == -1) {
      return vaga; 
    }
  }
  return "";
}

String findOccupiedVaga(String id, String allOccupancyJson) {
  String search = "\"" + id + "\"";
  int motoPos = allOccupancyJson.indexOf(search);
  
  if (motoPos == -1) {
    return "";
  }

  // Procura pela "vaga_" antes da posição do ID
  int startPos = allOccupancyJson.lastIndexOf("vaga_", motoPos);
  if (startPos == -1) {
    return ""; 
  }

  // Encontra o final do nome da vaga (as aspas seguintes)
  int endPos = allOccupancyJson.indexOf("\"", startPos);
  
  if (endPos != -1) {
    return allOccupancyJson.substring(startPos, endPos);
  }

  return "";
}

int countOccupiedVagas(String allOccupancyJson) {
  if (allOccupancyJson.equals("{}") || allOccupancyJson.equals("null") || allOccupancyJson.length() < 5) return 0;
  
  int count = 0;
  int index = -1;
  String searchKey = "\"vaga_"; // Contar quantas vezes "vaga_" aparece
  
  while(true) {
    index = allOccupancyJson.indexOf(searchKey, index + 1);
    if (index == -1) {
      break;
    }
    count++;
  }
  return count;
}

void occupyVaga(String vaga, String id) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String fullUrl = String(firebaseURL) + "/estacionadas/" + vaga + ".json";
    http.begin(fullUrl);
    http.addHeader("Content-Type", "application/json");

    String payload = "\"" + id + "\""; 

    int responseCode = http.PUT(payload); 
    Serial.print("Ocupar Vaga Firebase: ");
    Serial.println(responseCode);
    http.end();
  }
}

void releaseVaga(String vaga) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String fullUrl = String(firebaseURL) + "/estacionadas/" + vaga + ".json";
    http.begin(fullUrl);

    int responseCode = http.sendRequest("DELETE"); 
    Serial.print("Liberar Vaga Firebase: ");
    Serial.println(responseCode);
    http.end();
  }
}
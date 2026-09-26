

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>


// ============================================================
// CONFIGURACAO WIFI
// ============================================================

const char* WIFI_SSID     = "SeuSSIDAqui";
const char* WIFI_PASSWORD = "SuaSenhaAqui";


// ============================================================
// LOCALIZACAO 
// ============================================================

const float LATITUDE  = -26.9194;
const float LONGITUDE = -49.0661;


// ============================================================
// LCD 16x2 I2C
// ============================================================

#define SDA_PIN 21
#define SCL_PIN 22

LiquidCrystal_I2C lcd(0x27, 16, 2);


// ============================================================
// HORARIOS
// ============================================================

// Atualizacao principal diaria
const int HORA_ATUALIZACAO = 18;

// Periodo considerado:
// 20:00 ate 06:00
const int HORA_INICIO_NOITE = 20;
const int HORA_FIM_NOITE    = 6;


// ============================================================
// INTERVALOS
// ============================================================

// Troca entre temperatura e roupa
const unsigned long INTERVALO_TELA = 5000;

// Tentativa de reconexao Wi-Fi
const unsigned long INTERVALO_WIFI = 30000;

// Se API falhar, tenta novamente em 10 minutos
const unsigned long INTERVALO_API_FALHA =
  10UL * 60UL * 1000UL;


// ============================================================
// DADOS DA PREVISAO
// ============================================================

float tempMedia  = 0.0;
float tempMinima = 0.0;
float tempMaxima = 0.0;

String roupa = "";

bool previsaoValida = false;
bool horarioValido  = false;


// ============================================================
// CONTROLE
// ============================================================

int ultimoDiaConsulta = -1;

unsigned long ultimaTrocaTela      = 0;
unsigned long ultimaTentativaWiFi  = 0;
unsigned long ultimaTentativaAPI   = 0;

int telaAtual = 0;


// ============================================================
// CONTROLE LCD + SERIAL
// ============================================================

String ultimaLinha1 = "";
String ultimaLinha2 = "";


// ============================================================
// FUNCAO - LCD + SERIAL MONITOR
// ============================================================

void exibirLCDSerial(String linha1, String linha2) {


  if (linha1.length() > 16) {
    linha1 = linha1.substring(0, 16);
  }

  if (linha2.length() > 16) {
    linha2 = linha2.substring(0, 16);
  }

 
  if (
    linha1 == ultimaLinha1 &&
    linha2 == ultimaLinha2
  ) {
    return;
  }

  ultimaLinha1 = linha1;
  ultimaLinha2 = linha2;

  String lcdLinha1 = linha1;
  String lcdLinha2 = linha2;

  
  while (lcdLinha1.length() < 16) {
    lcdLinha1 += " ";
  }

  while (lcdLinha2.length() < 16) {
    lcdLinha2 += " ";
  }

  // ----------------------------------------------------------
  // LCD
  // ----------------------------------------------------------

  lcd.setCursor(0, 0);
  lcd.print(lcdLinha1);

  lcd.setCursor(0, 1);
  lcd.print(lcdLinha2);

  // ----------------------------------------------------------
  // SERIAL MONITOR
  // ----------------------------------------------------------

  Serial.println();
  Serial.println("+----------------+");

  Serial.print("|");
  Serial.print(lcdLinha1);
  Serial.println("|");

  Serial.print("|");
  Serial.print(lcdLinha2);
  Serial.println("|");

  Serial.println("+----------------+");
}


// ============================================================
// WIFI
// ============================================================

void conectarWiFi() {

  Serial.println();
  Serial.println("[WIFI] Iniciando conexao...");

  exibirLCDSerial(
    "CONECTANDO WIFI",
    "AGUARDE..."
  );

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long inicio = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - inicio < 15000
  ) {

    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("[WIFI] Conectado!");

    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("[WIFI] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

  } else {

    Serial.println(
      "[WIFI] Nao foi possivel conectar."
    );

    exibirLCDSerial(
      "SEM INTERNET",
      "RECONECTANDO..."
    );
  }
}


// ============================================================
// SINCRONIZACAO NTP
// ============================================================

bool sincronizarHorario() {

  if (WiFi.status() != WL_CONNECTED) {

    horarioValido = false;

    return false;
  }

  Serial.println();
  Serial.println("[TIME] Sincronizando NTP...");

  exibirLCDSerial(
    "SINCRONIZANDO",
    "HORARIO..."
  );

  // Fuso horario de Brasilia.
  // Blumenau utiliza UTC-3.
  configTzTime(
    "BRT3",
    "pool.ntp.org",
    "time.google.com"
  );

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 10000)) {

    Serial.println(
      "[TIME] Falha ao sincronizar NTP."
    );

    horarioValido = false;

    return false;
  }

  horarioValido = true;

  Serial.printf(
    "[TIME] %02d/%02d/%04d %02d:%02d:%02d\n",
    timeinfo.tm_mday,
    timeinfo.tm_mon + 1,
    timeinfo.tm_year + 1900,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  return true;
}


// ============================================================
// ESCOLHER ROUPA
// ============================================================
//
// SOMENTE A TEMPERATURA MEDIA DECIDE A ROUPA.
//
// >= 24 C    -> Body curto
// 20-23.9 C  -> Body longo
// 16-19.9 C  -> Body + macacao
// < 16 C     -> Body + macacao grosso
//
// ============================================================

String escolherRoupa(float media) {

  if (media >= 24.0) {

    return "BODY CURTO";

  } else if (media >= 20.0) {

    return "BODY LONGO";

  } else if (media >= 16.0) {

    return "BODY + MACACAO";

  } else {

    return "BODY+MAC GROSSO";
  }
}


// ============================================================
// FUNCAO AUXILIAR PARA FORMATAR DATA
// ============================================================

String formatarData(struct tm data) {

  char buffer[11];

  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d",
    &data
  );

  return String(buffer);
}


// ============================================================
// DETERMINAR PERIODO DA NOITE
// ============================================================
//
// Exemplo:
//
// ESP ligado as 15h:
// 20h HOJE -> 06h AMANHA
//
// ESP ligado as 02h:
// 20h ONTEM -> 06h HOJE
//
// Isso evita buscar a noite errada durante a madrugada.
//
// ============================================================

void determinarPeriodoNoite(
  String &dataInicio,
  String &dataFim
) {

  struct tm agora;

  if (!getLocalTime(&agora)) {
    return;
  }

  time_t timestampAtual = mktime(&agora);

  // ----------------------------------------------------------
  // MADRUGADA: 00:00 ate 06:00
  // ----------------------------------------------------------

  if (agora.tm_hour <= 6) {

    time_t timestampOntem =
      timestampAtual - (24 * 60 * 60);

    struct tm ontem;

    localtime_r(
      &timestampOntem,
      &ontem
    );

    dataInicio = formatarData(ontem);
    dataFim    = formatarData(agora);
  }

  // ----------------------------------------------------------
  // RESTANTE DO DIA
  // ----------------------------------------------------------

  else {

    time_t timestampAmanha =
      timestampAtual + (24 * 60 * 60);

    struct tm amanha;

    localtime_r(
      &timestampAmanha,
      &amanha
    );

    dataInicio = formatarData(agora);
    dataFim    = formatarData(amanha);
  }
}


// ============================================================
// CONSULTAR OPEN-METEO
// ============================================================

bool consultarPrevisao() {

  // ----------------------------------------------------------
  // VERIFICACOES
  // ----------------------------------------------------------

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println(
      "[API] Consulta cancelada: sem Wi-Fi."
    );

    return false;
  }

  if (!horarioValido) {

    Serial.println(
      "[API] Consulta cancelada: horario invalido."
    );

    return false;
  }

  struct tm agora;

  if (!getLocalTime(&agora)) {

    Serial.println(
      "[API] Nao foi possivel obter horario."
    );

    return false;
  }


  // ----------------------------------------------------------
  // DETERMINA AS DATAS DA NOITE
  // ----------------------------------------------------------

  String dataInicio;
  String dataFim;

  determinarPeriodoNoite(
    dataInicio,
    dataFim
  );

  if (
    dataInicio.length() == 0 ||
    dataFim.length() == 0
  ) {

    Serial.println(
      "[API] Erro ao determinar datas."
    );

    return false;
  }

  Serial.println();
  Serial.println("==============================");

  Serial.println("[PERIODO] Noite analisada:");

  Serial.print("[PERIODO] ");
  Serial.print(dataInicio);
  Serial.print(" 20:00 -> ");
  Serial.print(dataFim);
  Serial.println(" 06:00");

  Serial.println("==============================");


  // ----------------------------------------------------------
  // MONTA URL OPEN-METEO
  // ----------------------------------------------------------

  String url =
    "https://api.open-meteo.com/v1/forecast"
    "?latitude=" + String(LATITUDE, 4) +
    "&longitude=" + String(LONGITUDE, 4) +
    "&hourly=temperature_2m"
    "&timezone=America%2FSao_Paulo"
    "&start_date=" + dataInicio +
    "&end_date=" + dataFim;


  Serial.println();
  Serial.println("[API] Consultando Open-Meteo...");

  Serial.println("[API] URL:");
  Serial.println(url);


  exibirLCDSerial(
    "BUSCANDO",
    "PREVISAO..."
  );


  // ----------------------------------------------------------
  // HTTPS
  // ----------------------------------------------------------

  WiFiClientSecure client;

  // Para esse projeto simplificamos a validacao TLS.
  client.setInsecure();

  HTTPClient http;

  http.setTimeout(15000);

  if (!http.begin(client, url)) {

    Serial.println(
      "[API] Falha em http.begin()."
    );

    return false;
  }


  // ----------------------------------------------------------
  // GET
  // ----------------------------------------------------------

  int httpCode = http.GET();

  Serial.print("[API] HTTP: ");
  Serial.println(httpCode);


  if (httpCode != HTTP_CODE_OK) {

    Serial.println(
      "[API] Open-Meteo retornou erro."
    );

    http.end();

    return false;
  }


  // ----------------------------------------------------------
  // RESPOSTA
  // ----------------------------------------------------------

  String payload = http.getString();

  http.end();

  Serial.print("[API] JSON recebido: ");
  Serial.print(payload.length());
  Serial.println(" bytes");


  // ----------------------------------------------------------
  // PARSE JSON
  // ----------------------------------------------------------

  JsonDocument doc;

  DeserializationError erro =
    deserializeJson(doc, payload);

  if (erro) {

    Serial.print("[JSON] Erro: ");
    Serial.println(erro.c_str());

    return false;
  }


  JsonArray horarios =
    doc["hourly"]["time"].as<JsonArray>();

  JsonArray temperaturas =
    doc["hourly"]["temperature_2m"].as<JsonArray>();


  if (
    horarios.isNull() ||
    temperaturas.isNull()
  ) {

    Serial.println(
      "[JSON] Dados horarios ausentes."
    );

    return false;
  }


  // ==========================================================
  // CALCULO DA NOITE
  // ==========================================================

  float soma   = 0.0;
  float minima = 100.0;
  float maxima = -100.0;

  int quantidade = 0;


  Serial.println();
  Serial.println("===== TEMPERATURAS =====");


  for (
    int i = 0;
    i < horarios.size();
    i++
  ) {

    String horario =
      horarios[i].as<String>();


    // Ignora valor nulo
    if (temperaturas[i].isNull()) {
      continue;
    }


    float temperatura =
      temperaturas[i].as<float>();


    // Exemplo:
    // 2026-09-15T20:00

    int hora =
      horario.substring(11, 13).toInt();


    bool diaInicio =
      horario.startsWith(dataInicio);

    bool diaFim =
      horario.startsWith(dataFim);


    bool periodoValido =
      (
        diaInicio &&
        hora >= HORA_INICIO_NOITE
      )
      ||
      (
        diaFim &&
        hora <= HORA_FIM_NOITE
      );


    if (!periodoValido) {
      continue;
    }


    // --------------------------------------------------------
    // LOG
    // --------------------------------------------------------

    Serial.print("[PREV] ");

    Serial.print(
      horario.substring(11, 16)
    );

    Serial.print(" -> ");

    Serial.print(
      temperatura,
      1
    );

    Serial.println(" C");


    // --------------------------------------------------------
    // CALCULOS
    // --------------------------------------------------------

    soma += temperatura;


    if (temperatura < minima) {

      minima = temperatura;
    }


    if (temperatura > maxima) {

      maxima = temperatura;
    }


    quantidade++;
  }


  Serial.println("========================");


  // ==========================================================
  // VALIDACAO
  // ==========================================================
  //
  // Horarios:
  //
  // 20
  // 21
  // 22
  // 23
  // 00
  // 01
  // 02
  // 03
  // 04
  // 05
  // 06
  //
  // Total = 11
  //
  // ==========================================================

  if (quantidade != 11) {

    Serial.print(
      "[API] ERRO: esperado 11 temperaturas, recebido "
    );

    Serial.println(quantidade);

    return false;
  }


  // ==========================================================
  // RESULTADOS
  // ==========================================================

  tempMedia =
    soma / quantidade;

  tempMinima =
    minima;

  tempMaxima =
    maxima;


  // SOMENTE A MEDIA DECIDE
  roupa =
    escolherRoupa(tempMedia);


  previsaoValida = true;


  // ----------------------------------------------------------
  // CONTROLE DE DATA
  // ----------------------------------------------------------

  ultimoDiaConsulta =
    agora.tm_yday;


  // ==========================================================
  // LOG RESULTADO
  // ==========================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("       RESULTADO NOITE");
  Serial.println("==============================");

  Serial.print("Media : ");
  Serial.print(tempMedia, 1);
  Serial.println(" C");

  Serial.print("Minima: ");
  Serial.print(tempMinima, 1);
  Serial.println(" C");

  Serial.print("Maxima: ");
  Serial.print(tempMaxima, 1);
  Serial.println(" C");

  Serial.print("Roupa : ");
  Serial.println(roupa);

  Serial.println("==============================");


  // Forca atualizacao imediata do display
  ultimaLinha1 = "";
  ultimaLinha2 = "";


  return true;
}


// ============================================================
// ATUALIZAR DISPLAY
// ============================================================

void atualizarDisplay() {

  // ==========================================================
  // AINDA NAO TEM PREVISAO
  // ==========================================================

  if (!previsaoValida) {

    if (
      WiFi.status() != WL_CONNECTED
    ) {

      exibirLCDSerial(
        "SEM INTERNET",
        "RECONECTANDO..."
      );

    } else {

      exibirLCDSerial(
        "SEM PREVISAO",
        "TENTANDO..."
      );
    }

    return;
  }


  // ==========================================================
  // TELA 1
  //
  // MEDIA: 17.3C
  // MIN14.2 MAX20.1
  //
  // ==========================================================

  if (telaAtual == 0) {

    String linha1 =
      "MEDIA: " +
      String(tempMedia, 1) +
      "C";


    // Se perdeu Wi-Fi depois da consulta,
    // adiciona ! para indicar dado armazenado.

    if (
      WiFi.status() != WL_CONNECTED
    ) {

      linha1 += " !";
    }


    String linha2 =
      "MIN" +
      String(tempMinima, 1) +
      " MAX" +
      String(tempMaxima, 1);


    exibirLCDSerial(
      linha1,
      linha2
    );
  }


  // ==========================================================
  // TELA 2
  //
  // ROUPA:
  // BODY + MACACAO
  //
  // ==========================================================

  else {

    exibirLCDSerial(
      "ROUPA:",
      roupa
    );
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  // ----------------------------------------------------------
  // SERIAL
  // ----------------------------------------------------------

  Serial.begin(115200);

  delay(500);


  Serial.println();
  Serial.println();
  Serial.println("==============================");
  Serial.println("       BOT AURORA");
  Serial.println("==============================");


  // ----------------------------------------------------------
  // LCD
  // ----------------------------------------------------------

  Wire.begin(
    SDA_PIN,
    SCL_PIN
  );


  lcd.init();

  lcd.backlight();


  exibirLCDSerial(
    "AURORA",
    "INICIANDO..."
  );


  delay(1500);


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  conectarWiFi();


  // ----------------------------------------------------------
  // NTP
  // ----------------------------------------------------------

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    if (
      sincronizarHorario()
    ) {

      // ------------------------------------------------------
      // CONSULTA IMEDIATAMENTE AO LIGAR
      // ------------------------------------------------------

      consultarPrevisao();
    }
  }


  ultimaTrocaTela =
    millis();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  unsigned long agoraMillis =
    millis();


  // ==========================================================
  // WIFI - RECONEXAO
  // ==========================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    if (
      agoraMillis - ultimaTentativaWiFi
      >= INTERVALO_WIFI
    ) {

      ultimaTentativaWiFi =
        agoraMillis;


      Serial.println();
      Serial.println(
        "[WIFI] Tentando reconectar..."
      );


      WiFi.disconnect();


      WiFi.begin(
        WIFI_SSID,
        WIFI_PASSWORD
      );
    }

  }


  // ==========================================================
  // WIFI VOLTOU
  // ==========================================================

  else {

    // Se ainda nao conseguimos horario
    if (!horarioValido) {

      sincronizarHorario();
    }


    // Se temos horario mas nunca conseguimos previsao
    if (
      horarioValido &&
      !previsaoValida
    ) {

      if (
        agoraMillis - ultimaTentativaAPI
        >= INTERVALO_API_FALHA
      ) {

        ultimaTentativaAPI =
          agoraMillis;


        Serial.println(
          "[API] Tentando obter primeira previsao..."
        );


        consultarPrevisao();
      }
    }
  }


  // ==========================================================
  // ATUALIZACAO AUTOMATICA DIARIA
  // ==========================================================

  if (
    horarioValido &&
    WiFi.status() == WL_CONNECTED
  ) {

    struct tm agora;


    if (getLocalTime(&agora)) {

      // ------------------------------------------------------
      // DEPOIS DAS 18H
      // ------------------------------------------------------

      if (
        agora.tm_hour >= HORA_ATUALIZACAO &&
        ultimoDiaConsulta != agora.tm_yday
      ) {

        // Evita tentar varias vezes por segundo
        if (
          agoraMillis - ultimaTentativaAPI
          >= INTERVALO_API_FALHA
        ) {

          ultimaTentativaAPI =
            agoraMillis;


          Serial.println();
          Serial.println(
            "[AUTO] Atualizacao diaria."
          );


          bool sucesso =
            consultarPrevisao();


          if (!sucesso) {

            Serial.println(
              "[AUTO] Falhou. Nova tentativa em 10 min."
            );
          }
        }
      }
    }
  }


  // ==========================================================
  // TROCA DE TELA
  // ==========================================================

  if (
    agoraMillis - ultimaTrocaTela
    >= INTERVALO_TELA
  ) {

    ultimaTrocaTela =
      agoraMillis;


    if (telaAtual == 0) {

      telaAtual = 1;

    } else {

      telaAtual = 0;
    }
  }


  // ==========================================================
  // LCD + SERIAL
  // ==========================================================

  atualizarDisplay();


  // Loop leve
  delay(100);
}
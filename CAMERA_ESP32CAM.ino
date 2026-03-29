#include "esp_camera.h"
#include <WiFi.h>
#include "LittleFS.h"
#include <WebServer.h>

// ===== WIFI =====
const char* ssid = "SUA_REDE";  //<--- AQUI VC DEVERA COLOCAR O NOME DA SUA REDE
const char* password = "SUA_SENHA";  //<---- AQUI A SENHA DA SUA REDE

// ===== BOTÃO =====
#define BUTTON_PIN 12
unsigned long lastButtonPress = 0;

// ===== CONTADOR =====
int photoCount = 0;

// ===== WEBSERVER =====
WebServer server(80);

// ===== CONFIG CAMERA =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ===== CAPTURA FOTO =====
void capturePhoto() {
  camera_fb_t * fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Erro na captura");
    return;
  }

  String path = "/foto" + String(photoCount++) + ".jpg";

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Erro ao abrir arquivo");
  } else {
    file.write(fb->buf, fb->len);
    Serial.println("Salvo: " + path);
  }

  file.close();
  esp_camera_fb_return(fb);
}

// ===== CAPTURA VIA WEB =====
void handleCapture() {
  capturePhoto();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== APAGAR TUDO (SOLUÇÃO DEFINITIVA) =====
void handleDeleteAll() {
  Serial.println("FORMATANDO LITTLEFS...");

  if (LittleFS.format()) {
    Serial.println("Formatado com sucesso!");
  } else {
    Serial.println("Erro ao formatar!");
  }

  LittleFS.begin(); // reinicia FS
  photoCount = 0;

  server.sendHeader("Location", "/");
  server.send(303);
}

// ===== PAGINA WEB =====
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:sans-serif;background:#f4f4f4;text-align:center;padding:20px;}";
  html += ".gallery{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:15px;padding:20px;}";
  html += ".card{background:white;padding:10px;border-radius:8px;box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  html += "img{width:100%;border-radius:5px;}";
  html += ".btn{padding:8px 12px;margin:5px;border:none;border-radius:5px;cursor:pointer;text-decoration:none;display:inline-block;}";
  html += ".btn-take{background:#28a745;color:white;}";
  html += ".btn-del{background:#dc3545;color:white;}";
  html += ".btn-download{background:#007bff;color:white;}";
  html += "</style></head><body>";

  html += "<h1>Galeria ESP32-CAM</h1>";
  html += "<a href='/capture' class='btn btn-take'>Tirar Foto</a>";
  html += "<a href='/deleteAll' class='btn btn-del'>Apagar Tudo</a>";

  html += "<div class='gallery'>";

  File root = LittleFS.open("/");
  File file = root.openNextFile();

  while (file) {
    String name = file.name();

    if (name.endsWith(".jpg")) {
      html += "<div class='card'>";
      html += "<img src='" + name + "'>";
      html += "<p>" + name + "</p>";
      html += "<a href='" + name + "' download class='btn btn-download'>Baixar</a>";
      html += "</div>";
    }

    file = root.openNextFile();
  }

  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

// ===== SERVIR ARQUIVO =====
void handleFile() {
  String path = server.uri();

  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");

    server.sendHeader("Content-Disposition", "attachment; filename=" + path);
    server.streamFile(file, "image/jpeg");

    file.close();
  } else {
    server.send(404, "text/plain", "Arquivo não encontrado");
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if (!LittleFS.begin(true)) {
    Serial.println("Erro no LittleFS");
    return;
  }

  // ===== CAMERA =====
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    Serial.println("PSRAM OK");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    Serial.println("Sem PSRAM");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Erro na câmera");
    return;
  }

  // ===== WIFI =====
  WiFi.begin(ssid, password);
  Serial.print("Conectando");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado!");
  Serial.println(WiFi.localIP());

  // ===== ROTAS =====
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/deleteAll", handleDeleteAll);
  server.onNotFound(handleFile);

  server.begin();
}

// ===== LOOP =====
void loop() {
  server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW && (millis() - lastButtonPress > 1000)) {
    capturePhoto();
    lastButtonPress = millis();
  }
}
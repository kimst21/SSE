// 필요한 라이브러리 포함
#include <WiFi.h>                // ESP32 Wi-Fi 기능을 위한 라이브러리
#include <AsyncTCP.h>            // 비동기 TCP 통신을 위한 라이브러리 (AsyncWebServer 사용 필수)
#include <ESPAsyncWebServer.h>   // 비동기 웹 서버 라이브러리
#include <Adafruit_BME280.h>     // BME280 센서를 위한 라이브러리
#include <Adafruit_Sensor.h>     // Adafruit 센서 라이브러리 (BME280 사용)

// 네트워크 자격 증명 설정
const char* ssid = "";           // Wi-Fi SSID (사용자가 입력해야 함)
const char* password = "";       // Wi-Fi 비밀번호 (사용자가 입력해야 함)

// 웹 서버 객체 생성 (포트 80 사용)
AsyncWebServer server(80);

// 클라이언트로 데이터를 지속적으로 전송할 이벤트 소스 생성
AsyncEventSource events("/events");

// 타이머 변수 (데이터 전송 간격 설정)
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000; // 30초마다 센서 데이터 업데이트

// BME280 센서 객체 생성
Adafruit_BME280 bme;  // BME280 센서를 I2C로 연결

// 센서 데이터 저장 변수
float temperature;
float humidity;
float pressure;

// BME280 센서 초기화
void initBME(){
    if (!bme.begin(0x76)) {  // BME280 센서 I2C 주소 0x76에서 찾기
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        while (1);  // 센서를 찾을 수 없으면 무한 루프
    }
}

// 센서 값 읽기
void getSensorReadings(){
    temperature = bme.readTemperature();  // 온도 측정 (섭씨)
    //temperature = 1.8 * bme.readTemperature() + 32;  // 화씨 변환 (필요 시 주석 해제)
    humidity = bme.readHumidity();        // 습도 측정
    pressure = bme.readPressure() / 100.0F; // 기압 측정 (hPa 단위 변환)
}

// Wi-Fi 연결 초기화
void initWiFi() {
    WiFi.mode(WIFI_STA);  // Wi-Fi를 스테이션(STA) 모드로 설정
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED) {  // Wi-Fi 연결될 때까지 대기
        Serial.print('.');
        delay(1000);
    }
    Serial.println(WiFi.localIP());  // 연결된 IP 주소 출력
}

// 웹 페이지의 자리 표시자(TEMPERATURE, HUMIDITY, PRESSURE) 값 변경
String processor(const String& var){
    getSensorReadings();
    if(var == "TEMPERATURE"){
        return String(temperature);
    }
    else if(var == "HUMIDITY"){
        return String(humidity);
    }
    else if(var == "PRESSURE"){
        return String(pressure);
    }
    return String();
}

// 웹 페이지 HTML 코드 (ESP32의 플래시 메모리에 저장)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p { font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #50B8B4; color: white; font-size: 1rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 800px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
    .reading { font-size: 1.4rem; }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>BME280 WEB SERVER (SSE)</h1>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card">
        <p><i class="fas fa-thermometer-half" style="color:#059e8a;"></i> TEMPERATURE</p><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-tint" style="color:#00add6;"></i> HUMIDITY</p><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card">
        <p><i class="fas fa-angle-double-down" style="color:#e1e437;"></i> PRESSURE</p><p><span class="reading"><span id="pres">%PRESSURE%</span> hPa</span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');

 source.addEventListener('temperature', function(e) {
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('pressure', function(e) {
  document.getElementById("pres").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void setup() {
    Serial.begin(115200);
    initWiFi();  // Wi-Fi 초기화
    initBME();   // BME280 센서 초기화

    // 웹 서버 처리
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html, processor);
    });

    // 클라이언트가 이벤트 스트림에 연결될 때 실행
    events.onConnect([](AsyncEventSourceClient *client){
        if(client->lastId()){
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // 클라이언트가 연결되면 "hello!" 메시지를 전송하고, 재접속 간격을 10초로 설정
        client->send("hello!", NULL, millis(), 10000);
    });

    server.addHandler(&events);  // 이벤트 소스를 웹 서버에 추가
    server.begin();  // 웹 서버 시작
}

void loop() {
    if ((millis() - lastTime) > timerDelay) {  // 30초마다 센서 데이터 전송
        getSensorReadings();
        Serial.printf("Temperature = %.2f ºC \n", temperature);
        Serial.printf("Humidity = %.2f \n", humidity);
        Serial.printf("Pressure = %.2f hPa \n", pressure);
        Serial.println();

        // 클라이언트에 센서 데이터 전송 (SSE 방식)
        events.send(String(temperature).c_str(),"temperature",millis());
        events.send(String(humidity).c_str(),"humidity",millis());
        events.send(String(pressure).c_str(),"pressure",millis());

        lastTime = millis();  // 마지막 업데이트 시간 갱신
    }
}

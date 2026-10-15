/*
  RoboDrive ESP32 4WD Robot
  Features: Wi-Fi AP dashboard, manual commands, PWM speed, line following,
  ultrasonic obstacle avoidance, servo pan, emergency stop.

  Hardware assumptions (adjust for your board):
  L298N: ENA=25 IN1=26 IN2=27; ENB=14 IN3=32 IN4=33
  5-channel digital line sensor: GPIO 16,17,18,19,21 (left to right)
  Servo signal: GPIO 13
  HC-SR04: TRIG=22, ECHO=34 THROUGH A VOLTAGE DIVIDER (5V echo is unsafe for ESP32)
  Power: use a suitable regulated supply for ESP32 and servo, common GND.
  Two 18650 cells in series need a proper 2S BMS/protected pack and 8.4V charger.
  Upload data/index.html to SPIFFS using the Arduino IDE filesystem uploader.

  Libraries: ESP32Servo (Kevin Harrington / John K. Bennett), ESP32 Arduino core.
*/
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESP32Servo.h>

const char* AP_SSID = "RobotCar";
const char* AP_PASSWORD = "robot123";

WebServer server(80);
Servo scanner;

constexpr int ENA=25, IN1=26, IN2=27, ENB=14, IN3=32, IN4=33;
constexpr int SERVO_PIN=13, TRIG_PIN=22, ECHO_PIN=34;
const int LINE_PINS[5] = {16,17,18,19,21};

// Set true if your line sensor outputs LOW when it sees the dark line.
constexpr bool LINE_ACTIVE_LOW = true;
int motorSpeed = 150;
String mode = "manual";
String lastCommand = "S";
bool emergencyStopped = true;
unsigned long lastAutoTick = 0;
unsigned long lastTelemetry = 0;

void corsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-store");
}
void sendText(int code, const String& body) {
  corsHeaders();
  server.send(code, "text/plain; charset=utf-8", body);
}
void handleOptions() { corsHeaders(); server.send(204, "text/plain", ""); }

void setMotor(int left, int right) {
  left = constrain(left, -255, 255); right = constrain(right, -255, 255);
  digitalWrite(IN1, left > 0); digitalWrite(IN2, left < 0);
  digitalWrite(IN3, right > 0); digitalWrite(IN4, right < 0);
  analogWrite(ENA, abs(left)); analogWrite(ENB, abs(right));
}
void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  lastCommand = "S";
}
void driveCommand(String c) {
  c.toUpperCase();
  if (c == "S") { stopMotors(); return; }
  if (emergencyStopped || mode != "manual") return;
  int s = motorSpeed;
  if (c == "F") { setMotor(s, s); lastCommand = "F"; }
  else if (c == "B") { setMotor(-s, -s); lastCommand = "B"; }
  else if (c == "L") { setMotor(-s, s); lastCommand = "L"; }
  else if (c == "R") { setMotor(s, -s); lastCommand = "R"; }
  else stopMotors();
}
long distanceCm() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10); digitalWrite(TRIG_PIN, LOW);
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return 400;
  return (long)(duration * 0.0343 / 2.0);
}
bool lineSeen(int i) {
  int value = digitalRead(LINE_PINS[i]);
  return LINE_ACTIVE_LOW ? value == LOW : value == HIGH;
}
void lineFollowTick() {
  bool s[5]; int count=0, weighted=0;
  for (int i=0;i<5;i++) { s[i]=lineSeen(i); if(s[i]) { count++; weighted += (i-2); } }
  if (count == 0) { setMotor(105,105); lastCommand="F"; return; }
  int error = weighted;
  if (error < 0) { int turn = min(220, 90 + abs(error)*28); setMotor(max(0,motorSpeed-turn), min(motorSpeed,turn+45)); lastCommand="L"; }
  else if (error > 0) { int turn = min(220, 90 + abs(error)*28); setMotor(min(motorSpeed,turn+45), max(0,motorSpeed-turn)); lastCommand="R"; }
  else { setMotor(motorSpeed,motorSpeed); lastCommand="F"; }
}
void avoidTick() {
  long d = distanceCm();
  if (d > 0 && d < 22) {
    stopMotors(); delay(100);
    setMotor(-motorSpeed,motorSpeed); delay(350);
    stopMotors(); delay(80);
    // Re-check after the turn. Tune the turn duration for your chassis.
    if (distanceCm() < 22) { setMotor(motorSpeed,-motorSpeed); delay(600); }
    stopMotors();
  } else { setMotor(motorSpeed,motorSpeed); lastCommand="F"; }
}
void serveRoot() {
  if (SPIFFS.exists("/index.html")) {
    File f=SPIFFS.open("/index.html","r");
    corsHeaders(); server.streamFile(f,"text/html"); f.close();
  } else sendText(200,"RoboDrive ESP32 is running. Upload data/index.html to SPIFFS for the dashboard.");
}
void setupRoutes() {
  server.on("/", HTTP_GET, serveRoot);
  server.on("/cmd", HTTP_OPTIONS, handleOptions);
  server.on("/mode", HTTP_OPTIONS, handleOptions);
  server.on("/speed", HTTP_OPTIONS, handleOptions);
  server.on("/cmd", HTTP_GET, [](){
    String c=server.hasArg("c")?server.arg("c"):"S"; c.toUpperCase();
    if (c=="S") { emergencyStopped=true; stopMotors(); sendText(200,"STOPPED"); return; }
    if (mode!="manual") { sendText(409,"Switch to manual mode to use joystick"); return; }
    emergencyStopped=false; driveCommand(c);
    if (server.hasArg("speed")) motorSpeed=constrain(server.arg("speed").toInt(),0,255);
    sendText(200,"OK");
  });
  server.on("/mode", HTTP_GET, [](){
    String m=server.hasArg("m")?server.arg("m"):"manual";
    if(m!="manual" && m!="line" && m!="avoid") { sendText(400,"Invalid mode"); return; }
    mode=m; emergencyStopped=false; stopMotors();
    sendText(200,"MODE="+mode);
  });
  server.on("/speed", HTTP_GET, [](){
    if(!server.hasArg("v")) { sendText(400,"Missing v"); return; }
    motorSpeed=constrain(server.arg("v").toInt(),0,255);
    sendText(200,String(motorSpeed));
  });
  server.on("/status", HTTP_GET, [](){
    sendText(200,"mode="+mode+";speed="+String(motorSpeed)+";distance="+String(distanceCm())+";stopped="+String(emergencyStopped));
  });
  server.onNotFound([](){ sendText(404,"Not found"); });
}
void setup() {
  Serial.begin(115200);
  pinMode(ENA,OUTPUT); pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(ENB,OUTPUT); pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  for(int p:LINE_PINS) pinMode(p,INPUT_PULLUP);
  pinMode(TRIG_PIN,OUTPUT); pinMode(ECHO_PIN,INPUT);
  stopMotors();
  scanner.setPeriodHertz(50); scanner.attach(SERVO_PIN,500,2400); scanner.write(90);
  if(!SPIFFS.begin(true)) Serial.println("SPIFFS mount failed");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID,AP_PASSWORD);
  Serial.print("Connect to Wi-Fi "); Serial.println(AP_SSID);
  Serial.print("Open http://"); Serial.println(WiFi.softAPIP());
  setupRoutes(); server.begin();
}
void loop() {
  server.handleClient();
  unsigned long now=millis();
  if(now-lastAutoTick >= 70) {
    lastAutoTick=now;
    if(!emergencyStopped) {
      if(mode=="line") lineFollowTick();
      else if(mode=="avoid") avoidTick();
    }
  }
}

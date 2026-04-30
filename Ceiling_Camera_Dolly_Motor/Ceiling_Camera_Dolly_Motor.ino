#include <SPI.h>
#include <Ethernet.h>
#include <TMCStepper.h>

byte mac[] = {0xDE,0xAD,0xBE,0xEF,0xFE,0x01};
IPAddress ip(192,168,1,100);
EthernetServer server(80);
#define W5500_CS_PIN 5
#define DRIVER_ADDRESS 0b00
#define R_SENSE 0.11f
#define SERIAL_PORT Serial2
TMC2209Stepper driver(&SERIAL_PORT, R_SENSE, DRIVER_ADDRESS);
#define STEP_PIN 25
#define DIR_PIN 26
#define EN_PIN 27
#define LIMIT_LEFT_PIN 34
#define LIMIT_RIGHT_PIN 35

volatile int direction=0;
int speedPercent=50;
bool enabled=false;

unsigned long stepDelayUs() {
  if (speedPercent<=0) return 9999999;
  return map(speedPercent, 1, 100, 5000, 500);
}

unsigned long lastStepTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  pinMode(LIMIT_LEFT_PIN, INPUT_PULLUP);
  pinMode(LIMIT_RIGHT_PIN, INPUT_PULLUP);

  SERIAL_PORT.begin(115200, SERIAL_8N1, 16, 17);
  driver.begin();
  driver.toff(5);
  driver.rms_current(1800);
  driver.microsteps(16);
  driver.en_spreadCycle(false);
  driver.pwm_autoscale(true);

  Ethernet.init(W5500_CS_PIN);
  Ethernet.begin(mac, ip);
  delay(1000);
  server.begin();
  Serial.print("Motor ESP32 ready at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  handleNetwork();
  handleMotion();
}

void handleNetwork() {
  EthernetClient client = server.available();
  if (!client) return;

  String cmd = "";
  unsigned long timeout = millis() + 200;
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') break;
      cmd += c;
    }
  }
  cmd.trim();

  if (cmd == "FWD") {
    direction = 1;
    enabled = true;
    digitalWrite(EN_PIN, LOW);
    client.println("OK FWD");
  } else if (cmd == "BWD") {
    direction = -1;
    enabled = true;
    digitalWrite(EN_PIN, LOW);
    client.println("OK BWD");
  } else if (cmd == "STP") {
    direction = 0;
    enabled = false;
    digitalWrite(EN_PIN, HIGH);
    client.println("OK STP");
  } else if (cmd.startsWith("SPD:")) {
    int val = cmd.substring(4).toInt();
    speedPercent = constrain(val, 0, 100);
    client.println("OK SPD:" + String(speedPercent));
  } else if (cmd == "STATUS") {
    String s = "DIR:" + String(direction) + " SPD:" + String(speedPercent) +
               " LIM_L:" + String(!digitalRead(LIMIT_LEFT_PIN)) + " LIM_R:" +
               String(!digitalRead(LIMIT_RIGHT_PIN));
    client.println(s);
  } else {
    client.println("ERR unknown command");
  }

  client.stop();
}

void handleMotion() {
  if (direction == 0 || !enabled) return;

  bool hitLeft = digitalRead(LIMIT_LEFT_PIN) == LOW;
  bool hitRight = digitalRead(LIMIT_RIGHT_PIN) == LOW;

  if ((direction == -1 && hitLeft) || (direction == 1 && hitRight)) {
    direction = 0;
    enabled = false;
    digitalWrite(EN_PIN, HIGH);
    Serial.println("Limit switch hit - stopped");
    return;
  }

  unsigned long now = micros();
  if (now - lastStepTime >= stepDelayUs()) {
    digitalWrite(DIR_PIN, direction == 1 ? HIGH : LOW);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(STEP_PIN, LOW);
    lastStepTime = now;
  }
}

#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

byte mac[] = {0xDE,0xAD,0xBE,0xEF,0xFE,0x02};
IPAddress myIP(192,168,1,101);
IPAddress motorIP(192,168,1,100);
#define MOTOR_PORT 80
#define W5500_CS_PIN 5

LiquidCrystal_I2C lcd(0x27,16,2);

#define BTN_FWD_PIN 32
#define BTN_BWD_PIN 33
#define ESTOP_PIN 13

#define ENC_CLK_PIN 18
#define ENC_DT_PIN 19

int speedPercent=50;
int lastDirection=0;
bool estopActive=false;
int lastEncCLK=HIGH;
unsigned long lastBtnTime=0;
#define DEBOUNCE_MS 50

void setup() {
  Serial.begin(115200);

  pinMode(BTN_FWD_PIN, INPUT_PULLUP);
  pinMode(BTN_BWD_PIN, INPUT_PULLUP);
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  updateDisplay("Connecting...",0);

  Ethernet.init(W5500_CS_PIN);
  Ethernet.begin(mac, myIP);
  delay(1000);
  Serial.print("Controller ready at ");
  Serial.println(Ethernet.localIP());

  updateDisplay("Ready",speedPercent);
}

void loop() {
  handleEstop();
  if (!estopActive) {
    handleEncoder();
    handleButtons();
  }
}

void handleEstop() {
  bool pressed = (digitalRead(ESTOP_PIN) == LOW);
  if (pressed && !estopActive) {
    estopActive = true;
    sendCommand("STP");
    updateDisplay("!! E-STOP !!",0);
  } else if (!pressed && estopActive) {
    estopActive = false;
    updateDisplay("Ready",speedPercent);
  }
}

void handleEncoder() {
  int clk = digitalRead(ENC_CLK_PIN);
  if (clk != lastEncCLK && clk == LOW) {
    int dt = digitalRead(ENC_DT_PIN);
    if (dt != clk) speedPercent = min(100, speedPercent + 5);
    else speedPercent = max(10, speedPercent - 5);
    sendCommand("SPD:" + String(speedPercent));
    updateDisplay(lastDirection == 1 ? "Forward" :
                  lastDirection == -1 ? "Backward" : "Ready",
                  speedPercent);
  }
  lastEncCLK = clk;
}

void handleButtons() {
  if (millis() - lastBtnTime < DEBOUNCE_MS) return;

  bool fwd = (digitalRead(BTN_FWD_PIN) == LOW);
  bool bwd = (digitalRead(BTN_BWD_PIN) == LOW);

  if (fwd && bwd) {
    if (lastDirection != 0) { sendCommand("STP"); lastDirection = 0; updateDisplay("Stopped", speedPercent); lastBtnTime = millis(); }
  } else if (fwd) {
    if (lastDirection != 1) { sendCommand("FWD"); lastDirection = 1; updateDisplay("Forward", speedPercent); lastBtnTime = millis(); }
  } else if (bwd) {
    if (lastDirection != -1){ sendCommand("BWD"); lastDirection = -1; updateDisplay("Backward",speedPercent); lastBtnTime = millis(); }
  } else {
    if (lastDirection != 0) { sendCommand("STP"); lastDirection = 0; updateDisplay("Ready", speedPercent); lastBtnTime = millis(); }
  }
}

void sendCommand(String cmd) {
  EthernetClient client;
  if (!client.connect(motorIP, MOTOR_PORT)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NO CONNECTION");
    return;
  }
  client.println(cmd);
  delay(50);
  if (client.available()) Serial.println("Motor: " + client.readStringUntil('\n'));
  client.stop();
}

void updateDisplay(String status, int spd) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(status.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print("Spd:");
  lcd.print(spd);
  lcd.print("% ");
  int barChars = map(spd, 0, 100, 0, 6);
  for (int i = 0; i < barChars; i++) lcd.print((char)255);
}

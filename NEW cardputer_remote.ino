#include <M5Cardputer.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <SD.h>
#include <SPI.h>

#define IR_RECV_PIN 1
#define IR_SEND_PIN 2
#define SD_CS_PIN   12

IRrecv irrecv(IR_RECV_PIN);
IRsend irsend(IR_SEND_PIN);
decode_results results;

#define COLOR_BG      0x0000
#define COLOR_BAR     0x1082
#define COLOR_GREEN   0x07E0
#define COLOR_WHITE   0xFFFF
#define COLOR_GRAY    0x8410
#define COLOR_YELLOW  0xFFE0
#define COLOR_RED     0xF800
#define COLOR_DARK    0x18C3

#define TOP_BAR_H    18
#define SCREEN_W     240
#define SCREEN_H     135
#define SIGNAL_BOX_Y (TOP_BAR_H + 2)
#define SIGNAL_BOX_H 44
#define LIST_BOX_Y   (SIGNAL_BOX_Y + SIGNAL_BOX_H + 6)
#define LIST_BOX_H   (SCREEN_H - LIST_BOX_Y - 3)
#define LIST_START_Y (LIST_BOX_Y + 14)
#define LIST_ROW_H   13
#define LIST_VISIBLE ((LIST_BOX_H - 16) / LIST_ROW_H)

struct IRFile {
  String name;
  String path;
};

IRFile   irFiles[50];
int      fileCount         = 0;
int      selectedFile      = 0;
int      scrollOffset      = 0;
String   lastHex           = "";
String   lastProtocol      = "";
uint64_t lastValue         = 0;
bool     newSignalReceived = false;
bool     sdMounted         = false;

unsigned long lastNewSignalTime = 0;

String pendingSaveName = "";
bool   namingMode      = false;

unsigned long lastBlinkTime = 0;
bool          blinkState    = false;

// ─── Key helper ──────────────────────────────────────────────────────────────

bool keyPressed(const Keyboard_Class::KeysState &ks, char c) {
  for (char ch : ks.word) {
    if (ch == c) return true;
  }
  return false;
}

// M5Cardputer'da OK/Enter tuşu tespiti
// Klavyede ortadaki büyük tuş: key code 0x0D veya özel OK tuşu
bool enterPressed(const Keyboard_Class::KeysState &ks) {
  // word içinde \n, \r, veya 0x0D ara
  for (char ch : ks.word) {
    if (ch == '\n' || ch == '\r' || ch == 0x0D || ch == 0x0A) return true;
  }
  // ok tuşu ayrıca kontrol
  if (ks.enter) return true;
  return false;
}

// ─── SD ──────────────────────────────────────────────────────────────────────

bool mountSD() {
  sdMounted = SD.begin(SD_CS_PIN);
  return sdMounted;
}

void scanIRFiles() {
  fileCount = 0;
  if (!sdMounted) return;
  if (!SD.exists("/remote module")) return;
  File dir = SD.open("/remote module");
  if (!dir || !dir.isDirectory()) return;
  while (fileCount < 50) {
    File f = dir.openNextFile();
    if (!f) break;
    String fname = String(f.name());
    if (!f.isDirectory() && fname.endsWith(".ir")) {
      irFiles[fileCount].name = fname.substring(0, fname.lastIndexOf('.'));
      irFiles[fileCount].path = "/remote module/" + fname;
      fileCount++;
    }
    f.close();
  }
  dir.close();
}

bool saveIRSignalNamed(const String &hex, const String &protocol,
                       uint64_t value, const String &customName) {
  if (!sdMounted) return false;
  if (!SD.exists("/remote module")) SD.mkdir("/remote module");

  String safeName = customName;
  safeName.trim();
  if (safeName.length() == 0) {
    int idx = 0;
    String fname;
    do { fname = "/remote module/signal_" + String(idx++) + ".ir"; }
    while (SD.exists(fname));
    safeName = "signal_" + String(idx - 1);
  }

  String fpath = "/remote module/" + safeName + ".ir";
  if (SD.exists(fpath)) {
    int idx = 1;
    while (SD.exists("/remote module/" + safeName + "_" + String(idx) + ".ir")) idx++;
    fpath = "/remote module/" + safeName + "_" + String(idx) + ".ir";
  }

  File f = SD.open(fpath, FILE_WRITE);
  if (!f) return false;
  f.println("# IR Signal saved by Remote Control Module");
  f.println("Protocol: " + protocol);
  char buf[20];
  snprintf(buf, sizeof(buf), "%llX", (unsigned long long)value);
  f.println("Value: " + String(buf));
  f.println("Hex: " + hex);
  f.close();
  return true;
}

// ─── IR ──────────────────────────────────────────────────────────────────────

String decodeToHex(decode_results *res) {
  char buf[20];
  snprintf(buf, sizeof(buf), "0x%08llX", (unsigned long long)res->value);
  return String(buf);
}

bool sendIRFile(const String &path) {
  if (!sdMounted) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  String   protocol = "";
  uint64_t value    = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if      (line.startsWith("Protocol: ")) protocol = line.substring(10);
    else if (line.startsWith("Value: "))    value    = strtoull(line.substring(7).c_str(), nullptr, 16);
  }
  f.close();
  if (value == 0) return false;
  if      (protocol == "NEC" || protocol == "NEC2") irsend.sendNEC(value, 32);
  else if (protocol == "SONY")                       irsend.sendSony(value, 12);
  else if (protocol == "RC5")                        irsend.sendRC5(value, 12);
  else if (protocol == "RC6")                        irsend.sendRC6(value, 20);
  else if (protocol == "SAMSUNG")                    irsend.sendSAMSUNG(value, 32);
  else if (protocol == "LG")                         irsend.sendLG(value, 28);
  else if (protocol == "JVC")                        irsend.sendJVC(value, 16, 0);
  else                                               irsend.sendNEC(value, 32);
  return true;
}

// ─── Draw ────────────────────────────────────────────────────────────────────

void drawTopBar() {
  M5Cardputer.Display.fillRect(0, 0, SCREEN_W, TOP_BAR_H, COLOR_BAR);
  M5Cardputer.Display.drawFastHLine(0, TOP_BAR_H, SCREEN_W, COLOR_GRAY);
  M5Cardputer.Display.setTextColor(COLOR_WHITE);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 5);
  M5Cardputer.Display.print("REMOTE CONTROL MODULE");
  M5Cardputer.Display.setTextColor(COLOR_GRAY);
  M5Cardputer.Display.setCursor(SCREEN_W - 36, 5);
  M5Cardputer.Display.print("by MOY");
}

void drawStaticFrames() {
  M5Cardputer.Display.drawRect(2, SIGNAL_BOX_Y, SCREEN_W - 4, SIGNAL_BOX_H, COLOR_GRAY);
  M5Cardputer.Display.drawRect(2, LIST_BOX_Y,   SCREEN_W - 4, LIST_BOX_H,   COLOR_GRAY);
}

void redrawSignalContent() {
  int y = SIGNAL_BOX_Y;
  M5Cardputer.Display.fillRect(3, y + 1, SCREEN_W - 6, SIGNAL_BOX_H - 2, COLOR_BG);

  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(COLOR_GRAY);
  M5Cardputer.Display.setCursor(6, y + 4);
  M5Cardputer.Display.print("RECEIVED:");

  M5Cardputer.Display.setCursor(72, y + 4);
  if (lastProtocol.length() > 0) {
    M5Cardputer.Display.setTextColor(COLOR_WHITE);
    M5Cardputer.Display.print(lastProtocol);
  } else {
    M5Cardputer.Display.setTextColor(COLOR_GRAY);
    M5Cardputer.Display.print("---");
  }

  M5Cardputer.Display.fillCircle(SCREEN_W - 8, y + 7, 4,
                                  sdMounted ? COLOR_GREEN : COLOR_RED);

  if (lastHex.length() > 0) {
    M5Cardputer.Display.setTextColor(newSignalReceived ? COLOR_GREEN : COLOR_WHITE);
    M5Cardputer.Display.setTextSize(2);
    int xPos = (SCREEN_W - (int)lastHex.length() * 12) / 2;
    M5Cardputer.Display.setCursor(xPos, y + 18);
    M5Cardputer.Display.print(lastHex);
  } else {
    M5Cardputer.Display.setTextColor(COLOR_GRAY);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(6, y + 22);
    M5Cardputer.Display.print("Point remote & press any button...");
  }
  M5Cardputer.Display.setTextSize(1);
}

void redrawListContent() {
  M5Cardputer.Display.fillRect(3, LIST_BOX_Y + 1, SCREEN_W - 6, 12, COLOR_BAR);
  M5Cardputer.Display.drawFastHLine(3, LIST_BOX_Y + 13, SCREEN_W - 6, COLOR_GRAY);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(6, LIST_BOX_Y + 3);
  if (fileCount == 0) {
    M5Cardputer.Display.setTextColor(COLOR_GRAY);
    M5Cardputer.Display.print("No .ir files in /remote module/");
  } else {
    M5Cardputer.Display.setTextColor(COLOR_YELLOW);
    M5Cardputer.Display.print("FILES");
    M5Cardputer.Display.print(fileCount);
    M5Cardputer.Display.print(" OK:send S:save R:resnd T:rfrsh");
  }

  M5Cardputer.Display.fillRect(3, LIST_START_Y - 1, SCREEN_W - 10,
                                LIST_BOX_H - 15, COLOR_BG);

  for (int i = 0; i < LIST_VISIBLE && (i + scrollOffset) < fileCount; i++) {
    int  idx = i + scrollOffset;
    int  y   = LIST_START_Y + i * LIST_ROW_H;
    bool sel = (idx == selectedFile);

    if (sel) {
      M5Cardputer.Display.fillRoundRect(4, y - 1, SCREEN_W - 10,
                                        LIST_ROW_H, 2, COLOR_DARK);
      M5Cardputer.Display.setTextColor(COLOR_GREEN);
    } else {
      M5Cardputer.Display.setTextColor(COLOR_WHITE);
    }

    String dispName = irFiles[idx].name;
    if (dispName.length() > 26) dispName = dispName.substring(0, 25) + "~";
    M5Cardputer.Display.setCursor(8, y);
    M5Cardputer.Display.print(sel ? "> " : "  ");
    M5Cardputer.Display.print(dispName);
  }

  if (fileCount > LIST_VISIBLE) {
    int barH = LIST_BOX_H - 16;
    int segH = max(4, barH / fileCount);
    int segY = LIST_START_Y +
               (selectedFile * (barH - segH)) / max(1, fileCount - 1);
    M5Cardputer.Display.drawFastVLine(SCREEN_W - 4, LIST_START_Y, barH, COLOR_GRAY);
    M5Cardputer.Display.drawFastVLine(SCREEN_W - 4, segY, segH, COLOR_WHITE);
  }
}

void fullRedraw() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  drawTopBar();
  drawStaticFrames();
  redrawSignalContent();
  redrawListContent();
}

void drawSendFlash() {
  M5Cardputer.Display.fillRect(SCREEN_W - 80, SIGNAL_BOX_Y + 24, 76, 14, COLOR_GRAY);
  M5Cardputer.Display.setTextColor(COLOR_BG);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(SCREEN_W - 76, SIGNAL_BOX_Y + 26);
  M5Cardputer.Display.print(">> SENDING");
  delay(300);
  redrawSignalContent();
}

void drawSaveFlash(bool ok) {
  M5Cardputer.Display.fillRect(SCREEN_W - 72, SIGNAL_BOX_Y + 24, 68, 14,
                                ok ? COLOR_GREEN : COLOR_RED);
  M5Cardputer.Display.setTextColor(COLOR_BG);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(SCREEN_W - 66, SIGNAL_BOX_Y + 26);
  M5Cardputer.Display.print(ok ? "SAVED!" : "SD ERR");
  delay(600);
  redrawSignalContent();
}

// ─── Naming screen ───────────────────────────────────────────────────────────

void drawNamingScreenFull() {
  M5Cardputer.Display.fillScreen(COLOR_BG);
  M5Cardputer.Display.fillRect(0, 0, SCREEN_W, TOP_BAR_H, COLOR_BAR);
  M5Cardputer.Display.drawFastHLine(0, TOP_BAR_H, SCREEN_W, COLOR_GRAY);
  M5Cardputer.Display.setTextColor(COLOR_WHITE);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(4, 5);
  M5Cardputer.Display.print("SAVE IR SIGNAL - Enter a name");

  M5Cardputer.Display.setTextColor(COLOR_GRAY);
  M5Cardputer.Display.setCursor(4, TOP_BAR_H + 8);
  M5Cardputer.Display.print("Signal: ");
  M5Cardputer.Display.setTextColor(COLOR_WHITE);
  M5Cardputer.Display.print(lastHex);
  M5Cardputer.Display.setTextColor(COLOR_GRAY);
  M5Cardputer.Display.print("  Proto: ");
  M5Cardputer.Display.setTextColor(COLOR_WHITE);
  M5Cardputer.Display.print(lastProtocol);

  int boxY = TOP_BAR_H + 28;
  M5Cardputer.Display.drawRect(4, boxY, SCREEN_W - 8, 22, COLOR_WHITE);

  M5Cardputer.Display.setTextColor(COLOR_GRAY);
  M5Cardputer.Display.setCursor(4, boxY + 30);
  M5Cardputer.Display.print("OK:Save  `:Cancel  BKSP:Delete");
  M5Cardputer.Display.setCursor(4, boxY + 42);
  M5Cardputer.Display.print("(Leave empty for auto-name)");
}

void updateNamingInputBox() {
  int boxY = TOP_BAR_H + 28;
  M5Cardputer.Display.fillRect(5, boxY + 1, SCREEN_W - 10, 20, COLOR_BG);
  M5Cardputer.Display.setTextColor(COLOR_GREEN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setCursor(8, boxY + 7);
  M5Cardputer.Display.print(pendingSaveName);
  int cursorX = 8 + pendingSaveName.length() * 6;
  if (blinkState) {
    M5Cardputer.Display.drawFastVLine(cursorX, boxY + 4, 14, COLOR_WHITE);
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);
  M5Cardputer.Display.setTextFont(0);

  irrecv.enableIRIn();
  irsend.begin();

  sdMounted = mountSD();
  if (sdMounted) scanIRFiles();

  fullRedraw();
}

// ─── Loop ────────────────────────────────────────────────────────────────────

void loop() {
  M5Cardputer.update();

  // ── Naming mode ──────────────────────────────────────────────────────────
  if (namingMode) {
    unsigned long now = millis();
    if (now - lastBlinkTime > 500) {
      lastBlinkTime = now;
      blinkState    = !blinkState;
      updateNamingInputBox();
    }

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

      // OK/Enter → kaydet
      if (enterPressed(ks)) {
        namingMode = false;
        bool ok    = saveIRSignalNamed(lastHex, lastProtocol, lastValue, pendingSaveName);
        pendingSaveName = "";
        if (ok && sdMounted) scanIRFiles();
        fullRedraw();
        drawSaveFlash(ok);
        redrawListContent();
        return;
      }

      // Backspace
      if (keyPressed(ks, '\b') || ks.del) {
        if (pendingSaveName.length() > 0) {
          pendingSaveName.remove(pendingSaveName.length() - 1);
          updateNamingInputBox();
        }
        return;
      }

      // ` → iptal
      if (keyPressed(ks, '`')) {
        namingMode      = false;
        pendingSaveName = "";
        fullRedraw();
        return;
      }

      // Normal karakter
      bool changed = false;
      for (char c : ks.word) {
        if (c >= 32 && c < 127 && pendingSaveName.length() < 24) {
          if (c != '/' && c != '\\' && c != ':' && c != '*' &&
              c != '?' && c != '"' && c != '<' && c != '>' && c != '|') {
            pendingSaveName += c;
            changed = true;
          }
        }
      }
      if (changed) {
        blinkState = true;
        updateNamingInputBox();
      }
    }
    delay(10);
    return;
  }

  // ── IR receive ───────────────────────────────────────────────────────────
  if (irrecv.decode(&results)) {
    if (results.value != 0xFFFFFFFFFFFFFFFF &&
        results.value != 0 &&
        results.decode_type != UNKNOWN) {
      lastHex           = decodeToHex(&results);
      lastProtocol      = typeToString(results.decode_type, results.repeat);
      lastValue         = results.value;
      newSignalReceived = true;
      lastNewSignalTime = millis();
      redrawSignalContent();
    }
    irrecv.resume();
  }

  if (newSignalReceived && millis() - lastNewSignalTime > 2000) {
    newSignalReceived = false;
    redrawSignalContent();
  }

  // ── Keyboard ─────────────────────────────────────────────────────────────
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    bool redrawList = false;

    // ';' → yukarı
    if (keyPressed(ks, ';')) {
      if (selectedFile > 0) {
        selectedFile--;
        if (selectedFile < scrollOffset) scrollOffset = selectedFile;
        redrawList = true;
      }
    }
    // '.' → aşağı
    else if (keyPressed(ks, '.')) {
      if (selectedFile < fileCount - 1) {
        selectedFile++;
        if (selectedFile >= scrollOffset + LIST_VISIBLE)
          scrollOffset = selectedFile - LIST_VISIBLE + 1;
        redrawList = true;
      }
    }
    // OK/Enter → seçili dosyayı gönder
    else if (enterPressed(ks)) {
      if (fileCount > 0 && selectedFile < fileCount) {
        drawSendFlash();
        sendIRFile(irFiles[selectedFile].path);
      }
    }
    // S → kaydet
    else if (keyPressed(ks, 's') || keyPressed(ks, 'S')) {
      if (lastHex.length() > 0) {
        pendingSaveName = "";
        namingMode      = true;
        blinkState      = true;
        lastBlinkTime   = millis();
        drawNamingScreenFull();
        updateNamingInputBox();
        return;
      }
    }
    // R → tekrar gönder
    else if (keyPressed(ks, 'r') || keyPressed(ks, 'R')) {
      if (lastValue != 0) {
        drawSendFlash();
        irsend.sendNEC(lastValue, 32);
      }
    }
    // T → SD yenile
    else if (keyPressed(ks, 't') || keyPressed(ks, 'T')) {
      sdMounted = mountSD();
      if (sdMounted) scanIRFiles();
      fullRedraw();
      return;
    }

    if (redrawList) redrawListContent();
  }

  delay(10);
}

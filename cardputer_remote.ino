#include <M5Cardputer.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <vector>

#define IR_RECV_PIN 1
#define IR_SEND_PIN 2
#define SD_CS_PIN   12

#define COLOR_BG      TFT_BLACK
#define COLOR_TEXT    TFT_WHITE
#define COLOR_SUCCESS 0x00FF66
#define COLOR_WARNING 0xFFCC00
#define COLOR_ACCENT  0xFF4422

#define SCREEN_W 240
#define SCREEN_H 135

IRrecv irRecv(IR_RECV_PIN);
IRsend irSend(IR_SEND_PIN);
decode_results irResults;

enum AppState {
  STATE_MAIN_MENU,
  STATE_CAPTURE,
  STATE_SAVE_PROMPT,
  STATE_CAPTURE_SAVE,
  STATE_LIBRARY,
  STATE_VIEW_CODE,
  STATE_SD_BROWSER,
  STATE_SEND_CONFIRM
};

struct IRSignal {
  String name;
  decode_type_t protocol;
  uint64_t value;
  uint16_t bits;
  uint32_t frequency;
  std::vector<uint16_t> rawData;
  bool isRaw;
  String filename;
};

AppState currentState  = STATE_MAIN_MENU;
AppState previousState = STATE_MAIN_MENU;
IRSignal capturedSignal;
IRSignal selectedSignal;
std::vector<IRSignal> library;
std::vector<String>   sdFiles;

int  menuIndex     = 0;
int  libraryIndex  = 0;
int  sdFileIndex   = 0;
int  scrollOffset  = 0;
bool sdAvailable   = false;
bool capturing     = false;
bool signalCaptured= false;
String inputText   = "";
String statusMsg   = "";
unsigned long statusTime  = 0;
unsigned long lastCapture = 0;
unsigned long animTimer   = 0;
bool needsRedraw = true;

const char* mainMenuItems[] = {
  "Capture IR Signal",
  "My Library",
  "SD Card Browser"
};
const uint32_t menuColors[] = {
  0xFF4422,
  0x0088FF,
  0x00CC44
};
const int MAIN_MENU_COUNT = 3;

#define ITEM_H     28
#define MENU_START_Y 28

M5Canvas canvas(&M5Cardputer.Display);

//==============================================================
// UTILITIES
//==============================================================
String myU64ToStr(uint64_t val) {
  if(val == 0) return "0";
  String r = ""; uint64_t t = val;
  while(t > 0){ r = String((char)('0' + (t % 10))) + r; t /= 10; }
  return r;
}

String myU64ToHex(uint64_t val) {
  if(val == 0) return "0x00000000";
  String h = ""; uint64_t t = val;
  while(t > 0){ int n = t & 0xF; h = String((char)(n < 10 ? '0' + n : 'A' + n - 10)) + h; t >>= 4; }
  while(h.length() < 8) h = "0" + h;
  return "0x" + h;
}

String protoName(decode_type_t p) {
  switch(p){
    case NEC:        return "NEC";
    case SONY:       return "SONY";
    case RC5:        return "RC5";
    case RC6:        return "RC6";
    case SAMSUNG:    return "SAMSUNG";
    case LG:         return "LG";
    case PANASONIC:  return "PANASONIC";
    case JVC:        return "JVC";
    case SHARP:      return "SHARP";
    case DENON:      return "DENON";
    case PIONEER:    return "PIONEER";
    case MITSUBISHI: return "MITSUBISHI";
    default:         return "RAW";
  }
}

void setStatus(String msg){ statusMsg = msg; statusTime = millis(); }

//==============================================================
// SD
//==============================================================
bool initSD(){
  SPI.begin(40, 39, 14, SD_CS_PIN);
  if(!SD.begin(SD_CS_PIN)){ sdAvailable = false; return false; }
  sdAvailable = true;
  if(!SD.exists("/IR_Signals")) SD.mkdir("/IR_Signals");
  return true;
}

bool saveSignalToSD(IRSignal& sig){
  if(!sdAvailable) return false;
  // .ir uzantısı kullan
  String fn = "/IR_Signals/" + sig.name + ".ir";
  fn.replace(" ", "_");
  sig.filename = fn;
  if(SD.exists(fn)) SD.remove(fn);
  File f = SD.open(fn, FILE_WRITE);
  if(!f) return false;
  DynamicJsonDocument doc(4096);
  doc["name"]          = sig.name;
  doc["protocol"]      = (int)sig.protocol;
  doc["protocol_name"] = protoName(sig.protocol);
  doc["value"]         = myU64ToStr(sig.value);
  doc["bits"]          = sig.bits;
  doc["frequency"]     = sig.frequency;
  doc["is_raw"]        = sig.isRaw;
  JsonArray ra = doc.createNestedArray("raw_data");
  for(auto& v : sig.rawData) ra.add(v);
  serializeJsonPretty(doc, f);
  f.close();
  return true;
}

bool loadSignalFromSD(String filename, IRSignal& sig){
  File f = SD.open(filename, FILE_READ);
  if(!f) return false;
  DynamicJsonDocument doc(4096);
  if(deserializeJson(doc, f)){ f.close(); return false; }
  f.close();
  sig.name      = doc["name"].as<String>();
  sig.protocol  = (decode_type_t)(int)doc["protocol"];
  sig.bits      = doc["bits"];
  sig.frequency = doc["frequency"] | 38000;
  sig.isRaw     = doc["is_raw"] | false;
  sig.filename  = filename;
  String vs     = doc["value"].as<String>();
  sig.value     = 0;
  for(char c : vs) if(c >= '0' && c <= '9') sig.value = sig.value * 10 + (c - '0');
  sig.rawData.clear();
  if(doc.containsKey("raw_data")){
    JsonArray ra = doc["raw_data"];
    for(auto v : ra) sig.rawData.push_back((uint16_t)v.as<int>());
  }
  return true;
}

void scanSDFiles(){
  sdFiles.clear();
  if(!sdAvailable) return;
  File dir = SD.open("/IR_Signals");
  if(!dir) return;
  while(true){
    File e = dir.openNextFile();
    if(!e) break;
    String nm = String(e.name());
    // .ir uzantılı dosyaları tara
    if(nm.endsWith(".ir")) sdFiles.push_back("/IR_Signals/" + nm);
    e.close();
  }
  dir.close();
}

void loadLibrary(){
  library.clear();
  scanSDFiles();
  for(auto& f : sdFiles){
    IRSignal s;
    if(loadSignalFromSD(f, s)) library.push_back(s);
  }
}

bool deleteFromSD(String fn){
  if(!sdAvailable) return false;
  return SD.remove(fn);
}

//==============================================================
// IR SEND
//==============================================================
bool sendIR(IRSignal& sig){
  irSend.begin();
  delay(10);
  if(sig.rawData.size() > 0 && (sig.isRaw || sig.protocol == UNKNOWN)){
    irSend.sendRaw(sig.rawData.data(), sig.rawData.size(),
                   sig.frequency > 0 ? sig.frequency / 1000 : 38);
    return true;
  }
  switch(sig.protocol){
    case NEC:       irSend.sendNEC(sig.value, sig.bits);        break;
    case SONY:      irSend.sendSony(sig.value, sig.bits);       break;
    case RC5:       irSend.sendRC5(sig.value, sig.bits);        break;
    case RC6:       irSend.sendRC6(sig.value, sig.bits);        break;
    case SAMSUNG:   irSend.sendSAMSUNG(sig.value, sig.bits);    break;
    case LG:        irSend.sendLG(sig.value, sig.bits);         break;
    case PANASONIC: irSend.sendPanasonic(sig.bits, sig.value);  break;
    case JVC:       irSend.sendJVC(sig.value, sig.bits, 0);     break;
    case SHARP:     irSend.sendSharpRaw(sig.value, sig.bits);   break;
    case DENON:     irSend.sendDenon(sig.value, sig.bits);      break;
    default:
      if(sig.rawData.size() > 0){
        irSend.sendRaw(sig.rawData.data(), sig.rawData.size(), 38);
        return true;
      }
      return false;
  }
  return true;
}

//==============================================================
// DRAW HELPERS
//==============================================================
void drawTopBar(String title){
  canvas.fillRect(0, 0, SCREEN_W, 22, 0x111111);
  canvas.drawFastHLine(0, 22, SCREEN_W, 0x333333);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  // Sol: uygulama adı
  canvas.setTextColor(0x4488FF);
  canvas.setCursor(4, 7);
  canvas.print("IR REMOTE");
  // Orta: başlık
  canvas.setTextColor(COLOR_TEXT);
  int tx = (SCREEN_W - (int)title.length() * 6) / 2;
  canvas.setCursor(tx, 7);
  canvas.print(title);
  // Sağ: SD göstergesi
  canvas.fillCircle(SCREEN_W - 8, 11, 4, sdAvailable ? 0x004400 : 0x440000);
  canvas.fillCircle(SCREEN_W - 8, 11, 2, sdAvailable ? COLOR_SUCCESS : COLOR_ACCENT);
}

void drawToast(){
  if(statusMsg.length() == 0) return;
  if(millis() - statusTime > 2500){ statusMsg = ""; return; }
  int w = min((int)statusMsg.length() * 6 + 16, SCREEN_W - 20);
  int x = (SCREEN_W - w) / 2, y = SCREEN_H - 28;
  canvas.fillRoundRect(x, y, w, 16, 4, 0x220011);
  canvas.drawRoundRect(x, y, w, 16, 4, 0x884466);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_TEXT);
  canvas.setCursor(x + 8, y + 4);
  canvas.print(statusMsg);
}

void drawBottomBar(String hint){
  canvas.fillRect(0, SCREEN_H - 13, SCREEN_W, 13, 0x0A0A0A);
  canvas.drawFastHLine(0, SCREEN_H - 13, SCREEN_W, 0x222222);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  canvas.setTextColor(0x555555);
  canvas.setCursor(3, SCREEN_H - 10);
  canvas.print(hint);
}

//==============================================================
// MAIN MENU — sade, animasyonsuz
//==============================================================
void drawMainMenu(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("MAIN MENU");

  for(int i = 0; i < MAIN_MENU_COUNT; i++){
    int y = MENU_START_Y + i * ITEM_H;
    bool sel = (i == menuIndex);
    uint32_t col = menuColors[i];

    if(sel){
      // Seçili satır: renkli arka plan
      canvas.fillRoundRect(4, y, SCREEN_W - 8, ITEM_H - 3, 4, col);
      canvas.fillRect(4, y, 4, ITEM_H - 3, TFT_WHITE);
      canvas.setTextColor(TFT_WHITE);
    } else {
      // Normal satır: koyu arka plan
      canvas.fillRoundRect(4, y, SCREEN_W - 8, ITEM_H - 3, 4, 0x111111);
      canvas.drawRoundRect(4, y, SCREEN_W - 8, ITEM_H - 3, 4, 0x222222);
      // Sol renkli çizgi
      canvas.fillRect(4, y, 3, ITEM_H - 3, col);
      canvas.setTextColor(0x888888);
    }

    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setCursor(14, y + 5);
    canvas.print(mainMenuItems[i]);

    if(sel){
      canvas.setFont(&fonts::Font0);
      canvas.setTextSize(1);
      canvas.setTextColor(TFT_WHITE);
      canvas.setCursor(SCREEN_W - 14, y + 9);
      canvas.print(">");
    }
  }

  drawBottomBar(",: Up   .: Down   Space/Enter: Select");
}

//==============================================================
// CAPTURE
//==============================================================
void drawCaptureScreen(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("CAPTURE IR");

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  if(!signalCaptured){
    // Basit animasyon halkası
    uint32_t t = millis() - animTimer;
    int r = 8 + (t / 80) % 16;
    canvas.drawCircle(30, 78, r, 0x001866);
    canvas.fillCircle(30, 78, 7, 0x0033BB);
    canvas.fillCircle(30, 78, 3, TFT_WHITE);

    canvas.setTextColor(COLOR_WARNING);
    canvas.setCursor(50, 42);
    canvas.print("Waiting for IR...");

    canvas.setTextColor(0x444444);
    canvas.setCursor(50, 58);
    canvas.print("Point remote at");
    canvas.setCursor(50, 70);
    canvas.print("the device and");
    canvas.setCursor(50, 82);
    canvas.print("press a button.");

    drawBottomBar("`/ESC: Back");
    needsRedraw = true;

  } else {
    // Sinyal yakalandı
    canvas.fillCircle(14, 42, 7, COLOR_SUCCESS);
    canvas.setTextColor(COLOR_BG);
    canvas.setCursor(9, 38); canvas.print("OK");

    canvas.fillRoundRect(26, 28, SCREEN_W - 30, 70, 5, 0x080808);
    canvas.drawRoundRect(26, 28, SCREEN_W - 30, 70, 5, COLOR_SUCCESS);

    String proto = protoName(capturedSignal.protocol);
    canvas.fillRoundRect(30, 31, proto.length() * 6 + 8, 12, 3, 0x002288);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(34, 34); canvas.print(proto);

    canvas.setTextColor(0x555555); canvas.setCursor(30, 50);
    canvas.print("HEX: ");
    canvas.setTextColor(COLOR_SUCCESS);
    String hx = myU64ToHex(capturedSignal.value);
    if(hx.length() > 14) hx = hx.substring(0, 14) + "..";
    canvas.print(hx);

    canvas.setTextColor(0x555555); canvas.setCursor(30, 62);
    canvas.print("Bits: ");
    canvas.setTextColor(COLOR_WARNING);
    canvas.print(capturedSignal.bits);

    canvas.setTextColor(0x555555); canvas.setCursor(30, 74);
    canvas.print("Raw: ");
    canvas.setTextColor(0xAAAAAA);
    canvas.print(capturedSignal.rawData.size());
    canvas.print(" pulses");

    // Alt buton satırı
    canvas.fillRoundRect(4,  102, 54, 14, 3, 0x003300);
    canvas.drawRoundRect(4,  102, 54, 14, 3, COLOR_SUCCESS);
    canvas.setTextColor(COLOR_SUCCESS);
    canvas.setCursor(8,   105); canvas.print("S: Save");

    canvas.fillRoundRect(62, 102, 54, 14, 3, 0x000033);
    canvas.drawRoundRect(62, 102, 54, 14, 3, 0x4444FF);
    canvas.setTextColor(0x4444FF);
    canvas.setCursor(66,  105); canvas.print("T: Test");

    canvas.fillRoundRect(120, 102, 54, 14, 3, 0x221100);
    canvas.drawRoundRect(120, 102, 54, 14, 3, COLOR_WARNING);
    canvas.setTextColor(COLOR_WARNING);
    canvas.setCursor(124, 105); canvas.print("V: View");

    canvas.fillRoundRect(178, 102, 54, 14, 3, 0x220000);
    canvas.drawRoundRect(178, 102, 54, 14, 3, COLOR_ACCENT);
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setCursor(182, 105); canvas.print("R: Redo");

    drawBottomBar("`/ESC: Back (save prompt)");
  }
}

//==============================================================
// SAVE PROMPT
//==============================================================
void drawSavePrompt(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("SAVE SIGNAL?");

  canvas.fillRoundRect(10, 32, SCREEN_W - 20, 68, 6, 0x0A0A0A);
  canvas.drawRoundRect(10, 32, SCREEN_W - 20, 68, 6, COLOR_WARNING);

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  canvas.setTextColor(COLOR_TEXT);
  canvas.setCursor(18, 42);
  canvas.print("Save before exiting?");

  canvas.setTextColor(0x555555); canvas.setCursor(18, 56);
  canvas.print("Proto: ");
  canvas.setTextColor(0x4488FF);
  canvas.print(protoName(capturedSignal.protocol));
  canvas.setTextColor(0x555555);
  canvas.print("  "); canvas.print(capturedSignal.bits); canvas.print("b");

  canvas.setTextColor(0x444444); canvas.setCursor(18, 68);
  canvas.print("Code: ");
  canvas.setTextColor(0x666666);
  String hx = myU64ToHex(capturedSignal.value);
  if(hx.length() > 14) hx = hx.substring(0, 14);
  canvas.print(hx);

  canvas.fillRoundRect(18, 82, 84, 16, 4, 0x003300);
  canvas.drawRoundRect(18, 82, 84, 16, 4, COLOR_SUCCESS);
  canvas.setTextColor(COLOR_SUCCESS);
  canvas.setCursor(28, 86); canvas.print("Y - Yes, Save");

  canvas.fillRoundRect(118, 82, 100, 16, 4, 0x220000);
  canvas.drawRoundRect(118, 82, 100, 16, 4, COLOR_ACCENT);
  canvas.setTextColor(COLOR_ACCENT);
  canvas.setCursor(128, 86); canvas.print("N - No, Discard");
}

//==============================================================
// SAVE DIALOG
//==============================================================
void drawSaveDialog(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("NAME SIGNAL");

  canvas.fillRoundRect(6, 26, SCREEN_W - 12, 90, 6, 0x080808);
  canvas.drawRoundRect(6, 26, SCREEN_W - 12, 90, 6, 0x0055FF);

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  canvas.setTextColor(0x777777);
  canvas.setCursor(14, 34);
  canvas.print("Enter signal name:");

  // Giriş kutusu
  canvas.fillRoundRect(14, 46, SCREEN_W - 28, 18, 3, 0x050505);
  canvas.drawRoundRect(14, 46, SCREEN_W - 28, 18, 3, 0x0055FF);
  canvas.setTextColor(TFT_WHITE);
  canvas.setCursor(18, 50);
  String disp = inputText;
  if(disp.length() > 24) disp = disp.substring(disp.length() - 24);
  canvas.print(disp);
  // Cursor yanıp sönme
  if((millis() / 400) % 2 == 0){
    int cx = 18 + disp.length() * 6;
    if(cx < SCREEN_W - 18) canvas.drawFastVLine(cx, 48, 12, 0x0088FF);
  }

  canvas.setTextColor(0x444444); canvas.setCursor(14, 72);
  canvas.print("Proto: ");
  canvas.setTextColor(0x4488FF);
  canvas.print(protoName(capturedSignal.protocol));
  canvas.setTextColor(0x444444); canvas.print("  Bits: ");
  canvas.setTextColor(0x4488FF); canvas.print(capturedSignal.bits);

  canvas.setTextColor(0x333333); canvas.setCursor(14, 84);
  canvas.print("Saved as: ");
  canvas.setTextColor(0x555555);
  String fn = inputText.length() > 0 ? inputText : "?";
  fn.replace(" ", "_");
  canvas.print(fn + ".ir");

  canvas.fillRoundRect(14, 100, 80, 14, 3, 0x003300);
  canvas.drawRoundRect(14, 100, 80, 14, 3, COLOR_SUCCESS);
  canvas.setTextColor(COLOR_SUCCESS);
  canvas.setCursor(20, 103); canvas.print("ENTER: Save");

  canvas.fillRoundRect(102, 100, 124, 14, 3, 0x220000);
  canvas.drawRoundRect(102, 100, 124, 14, 3, COLOR_ACCENT);
  canvas.setTextColor(COLOR_ACCENT);
  canvas.setCursor(108, 103); canvas.print("` / ESC: Cancel");

  needsRedraw = true;
}

//==============================================================
// LIBRARY
//==============================================================
void drawLibraryScreen(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("MY LIBRARY");

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  if(library.empty()){
    canvas.setTextColor(0x444444);
    canvas.setCursor(SCREEN_W / 2 - 52, 55);
    canvas.print("No saved signals yet");
    canvas.setCursor(SCREEN_W / 2 - 58, 68);
    canvas.print("Capture a signal first!");
    drawBottomBar("` : Back");
    return;
  }

  int visN = 4, itemH = 24, startY = 26;
  if(libraryIndex < scrollOffset) scrollOffset = libraryIndex;
  if(libraryIndex >= scrollOffset + visN) scrollOffset = libraryIndex - visN + 1;

  for(int i = 0; i < visN; i++){
    int idx = i + scrollOffset;
    if(idx >= (int)library.size()) break;
    int y = startY + i * itemH;
    bool sel = (idx == libraryIndex);

    if(sel){
      canvas.fillRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x001244);
      canvas.drawRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x0044FF);
      canvas.fillRect(2, y, 3, itemH - 2, 0x0088FF);
    } else {
      canvas.fillRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x080808);
      canvas.drawRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x1A1A1A);
    }

    canvas.setTextColor(sel ? TFT_WHITE : 0x777777);
    canvas.setCursor(10, y + 4);
    String nm = library[idx].name;
    if(nm.length() > 18) nm = nm.substring(0, 18) + "..";
    canvas.print(nm);

    String pt = protoName(library[idx].protocol);
    canvas.setTextColor(sel ? 0x66AAFF : 0x444444);
    canvas.setCursor(10, y + 14);
    String hx = myU64ToHex(library[idx].value);
    if(hx.length() > 12) hx = hx.substring(0, 12) + "..";
    canvas.print(pt + " | " + hx);
  }

  // Scroll bar
  if((int)library.size() > visN){
    int sbH = visN * itemH, sbX = SCREEN_W - 3, sbY = startY;
    canvas.fillRect(sbX, sbY, 2, sbH, 0x111111);
    int th = sbH * visN / library.size();
    int ty = sbY + sbH * scrollOffset / library.size();
    canvas.fillRect(sbX, ty, 2, th, 0x0055FF);
  }

  drawBottomBar(",/.: Nav  Enter:Send  V:View  D:Del  `:Back");
}

//==============================================================
// VIEW CODE
//==============================================================
void drawViewCodeScreen(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("IR CODE DETAIL");

  IRSignal& sig = selectedSignal;
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  // Protokol kartı
  canvas.fillRoundRect(2, 26, 88, 26, 4, 0x090909);
  canvas.drawRoundRect(2, 26, 88, 26, 4, 0x0033AA);
  canvas.setTextColor(0x334466); canvas.setCursor(6, 29); canvas.print("Protocol");
  canvas.setTextColor(0x4488FF); canvas.setCursor(6, 40); canvas.print(protoName(sig.protocol));

  // Bits kartı
  canvas.fillRoundRect(94, 26, 56, 26, 4, 0x090909);
  canvas.drawRoundRect(94, 26, 56, 26, 4, 0x003300);
  canvas.setTextColor(0x224422); canvas.setCursor(98, 29); canvas.print("Bits");
  canvas.setTextColor(COLOR_SUCCESS); canvas.setCursor(98, 40); canvas.print(sig.bits);

  // Frekans kartı
  canvas.fillRoundRect(154, 26, 82, 26, 4, 0x090909);
  canvas.drawRoundRect(154, 26, 82, 26, 4, 0x332200);
  canvas.setTextColor(0x443322); canvas.setCursor(158, 29); canvas.print("Freq");
  canvas.setTextColor(COLOR_WARNING); canvas.setCursor(158, 40);
  canvas.print(String(sig.frequency / 1000) + " kHz");

  // HEX satırı
  canvas.fillRoundRect(2, 56, SCREEN_W - 4, 16, 3, 0x080808);
  canvas.drawRoundRect(2, 56, SCREEN_W - 4, 16, 3, 0x222222);
  canvas.setTextColor(0x444444); canvas.setCursor(6, 60); canvas.print("HEX:");
  canvas.setTextColor(COLOR_SUCCESS); canvas.setCursor(36, 60);
  canvas.print(myU64ToHex(sig.value));

  // DEC satırı
  canvas.fillRoundRect(2, 76, SCREEN_W - 4, 16, 3, 0x080808);
  canvas.drawRoundRect(2, 76, SCREEN_W - 4, 16, 3, 0x222222);
  canvas.setTextColor(0x444444); canvas.setCursor(6, 80); canvas.print("DEC:");
  canvas.setTextColor(0xCCCCCC); canvas.setCursor(36, 80);
  String dc = myU64ToStr(sig.value);
  if(dc.length() > 18) dc = dc.substring(0, 18) + "..";
  canvas.print(dc);

  // İsim / raw satırı
  canvas.setTextColor(0x333333); canvas.setCursor(6, 100);
  if(sig.rawData.size() > 0){
    canvas.print("Raw: ");
    canvas.setTextColor(COLOR_WARNING); canvas.print(sig.rawData.size()); canvas.print(" pulses");
  } else {
    canvas.print("Name: ");
    canvas.setTextColor(0xAAAAAA);
    String nm = sig.name; if(nm.length() > 18) nm = nm.substring(0, 18) + "..";
    canvas.print(nm);
  }

  drawBottomBar("Enter/S: Send    `: Back");
}

//==============================================================
// SD BROWSER
//==============================================================
void drawSDBrowserScreen(){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("SD BROWSER (.ir)");

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  if(!sdAvailable){
    canvas.setTextColor(COLOR_ACCENT);
    canvas.setCursor(SCREEN_W / 2 - 48, 52); canvas.print("SD Card not found!");
    canvas.setTextColor(0x444444);
    canvas.setCursor(SCREEN_W / 2 - 54, 66); canvas.print("Insert card & restart");
    drawBottomBar("` : Back");
    return;
  }

  if(sdFiles.empty()){
    canvas.setTextColor(0x444444);
    canvas.setCursor(SCREEN_W / 2 - 48, 52); canvas.print("No .ir files found");
    canvas.setCursor(SCREEN_W / 2 - 52, 66); canvas.print("in /IR_Signals/ folder");
    drawBottomBar("R: Refresh   ` : Back");
    return;
  }

  int visN = 4, itemH = 24, startY = 26;
  if(sdFileIndex < scrollOffset) scrollOffset = sdFileIndex;
  if(sdFileIndex >= scrollOffset + visN) scrollOffset = sdFileIndex - visN + 1;

  for(int i = 0; i < visN; i++){
    int idx = i + scrollOffset;
    if(idx >= (int)sdFiles.size()) break;
    int y = startY + i * itemH;
    bool sel = (idx == sdFileIndex);

    if(sel){
      canvas.fillRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x001800);
      canvas.drawRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x00AA44);
      canvas.fillRect(2, y, 3, itemH - 2, COLOR_SUCCESS);
    } else {
      canvas.fillRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x080808);
      canvas.drawRoundRect(2, y, SCREEN_W - 4, itemH - 2, 3, 0x1A1A1A);
    }

    String fn = sdFiles[idx];
    int ls = fn.lastIndexOf('/');
    if(ls >= 0) fn = fn.substring(ls + 1);
    if(fn.endsWith(".ir")) fn = fn.substring(0, fn.length() - 3);
    fn.replace("_", " ");
    if(fn.length() > 26) fn = fn.substring(0, 26) + "..";

    canvas.setTextColor(sel ? COLOR_SUCCESS : 0x337733);
    canvas.setCursor(8, y + 4); canvas.print(".ir ");
    canvas.setTextColor(sel ? TFT_WHITE : 0x777777);
    canvas.print(fn);

    canvas.setTextColor(sel ? 0x338844 : 0x2A2A2A);
    String ctr = String(idx + 1) + "/" + String(sdFiles.size());
    canvas.setCursor(SCREEN_W - ctr.length() * 6 - 6, y + 14);
    canvas.print(ctr);
  }

  // Scroll bar
  if((int)sdFiles.size() > visN){
    int sbH = visN * itemH, sbX = SCREEN_W - 3, sbY = startY;
    canvas.fillRect(sbX, sbY, 2, sbH, 0x111111);
    int th = sbH * visN / sdFiles.size();
    int ty = sbY + sbH * scrollOffset / sdFiles.size();
    canvas.fillRect(sbX, ty, 2, th, COLOR_SUCCESS);
  }

  drawBottomBar(",/.: Nav  Enter:Load+Send  R:Refresh  `:Back");
}

//==============================================================
// SEND CONFIRM
//==============================================================
void drawSendConfirm(bool sending){
  canvas.fillScreen(COLOR_BG);
  drawTopBar("TRANSMIT");

  canvas.fillRoundRect(6, 26, SCREEN_W - 12, 80, 6, 0x080808);
  canvas.drawRoundRect(6, 26, SCREEN_W - 12, 80, 6, sending ? COLOR_SUCCESS : 0x0044FF);

  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);

  if(sending){
    canvas.setTextSize(2);
    canvas.setTextColor(COLOR_SUCCESS);
    canvas.setCursor(80, 52); canvas.print(">>>");
    canvas.setTextSize(1);
    canvas.setTextColor(0x777777);
    canvas.setCursor(SCREEN_W / 2 - 36, 82); canvas.print("Transmitting...");
    needsRedraw = true;
  } else {
    canvas.setTextColor(0x555555); canvas.setCursor(14, 34); canvas.print("Ready to send:");
    canvas.setTextColor(TFT_WHITE); canvas.setCursor(14, 46);
    String nm = selectedSignal.name;
    if(nm.length() > 26) nm = nm.substring(0, 26) + "..";
    canvas.print(nm);

    String pt = protoName(selectedSignal.protocol);
    int pw = pt.length() * 6 + 8;
    canvas.fillRoundRect(14, 58, pw, 12, 3, 0x001A44);
    canvas.setTextColor(0x4488FF); canvas.setCursor(18, 61); canvas.print(pt);

    canvas.setTextColor(0x333333); canvas.setCursor(14, 74);
    canvas.print("Code: ");
    canvas.setTextColor(0x555555);
    String hx = myU64ToHex(selectedSignal.value);
    if(hx.length() > 14) hx = hx.substring(0, 14) + "..";
    canvas.print(hx);

    canvas.fillRoundRect(14, 92, 88, 14, 4, 0x003300);
    canvas.drawRoundRect(14, 92, 88, 14, 4, COLOR_SUCCESS);
    canvas.setTextColor(COLOR_SUCCESS); canvas.setCursor(22, 95); canvas.print("Enter: SEND");

    canvas.fillRoundRect(114, 92, 110, 14, 4, 0x1A0000);
    canvas.drawRoundRect(114, 92, 110, 14, 4, COLOR_ACCENT);
    canvas.setTextColor(COLOR_ACCENT); canvas.setCursor(120, 95); canvas.print("` / ESC: Cancel");
  }
}

//==============================================================
// MASTER DRAW
//==============================================================
void drawScreen(){
  switch(currentState){
    case STATE_MAIN_MENU:    drawMainMenu();         break;
    case STATE_CAPTURE:      drawCaptureScreen();    break;
    case STATE_SAVE_PROMPT:  drawSavePrompt();       break;
    case STATE_CAPTURE_SAVE: drawSaveDialog();       break;
    case STATE_LIBRARY:      drawLibraryScreen();    break;
    case STATE_VIEW_CODE:    drawViewCodeScreen();   break;
    case STATE_SD_BROWSER:   drawSDBrowserScreen();  break;
    case STATE_SEND_CONFIRM: drawSendConfirm(false); break;
  }
  drawToast();
  canvas.pushSprite(0, 0);
}

//==============================================================
// IR RECEIVE
//==============================================================
void handleIRReceive(){
  if(!capturing || signalCaptured) return;
  if(!irRecv.decode(&irResults)) return;
  if(irResults.value != 0xFFFFFFFF && irResults.value != 0x0 && millis() - lastCapture > 300){
    capturedSignal.protocol  = irResults.decode_type;
    capturedSignal.value     = irResults.value;
    capturedSignal.bits      = irResults.bits;
    capturedSignal.frequency = 38000;
    capturedSignal.name      = "IR_" + String(millis() % 100000);
    capturedSignal.isRaw     = (irResults.decode_type == UNKNOWN);
    capturedSignal.rawData.clear();
    for(int i = 1; i < irResults.rawlen && i < 300; i++)
      capturedSignal.rawData.push_back(irResults.rawbuf[i] * RAWTICK);
    signalCaptured = true;
    lastCapture    = millis();
    capturing      = false;
    irRecv.pause();
    needsRedraw = true;
  }
  irRecv.resume();
}

//==============================================================
// KEY HANDLERS
//==============================================================
void handleMainMenu(char key){
  if(key == ',')
    menuIndex = (menuIndex - 1 + MAIN_MENU_COUNT) % MAIN_MENU_COUNT;
  else if(key == '.')
    menuIndex = (menuIndex + 1) % MAIN_MENU_COUNT;
  else if(key == '\n' || key == '\r' || key == ' '){
    scrollOffset = 0;
    switch(menuIndex){
      case 0:
        currentState   = STATE_CAPTURE;
        signalCaptured = false;
        capturing      = true;
        irRecv.resume();
        animTimer = millis();
        break;
      case 1:
        loadLibrary();
        currentState  = STATE_LIBRARY;
        libraryIndex  = 0;
        scrollOffset  = 0;
        break;
      case 2:
        scanSDFiles();
        currentState = STATE_SD_BROWSER;
        sdFileIndex  = 0;
        scrollOffset = 0;
        break;
    }
  }
}

void handleCapture(char key){
  if(key == '`' || key == 27){
    if(signalCaptured){
      currentState = STATE_SAVE_PROMPT;
    } else {
      capturing = false; irRecv.pause();
      currentState = STATE_MAIN_MENU;
    }
  } else if(signalCaptured){
    if(key == 's' || key == 'S'){
      if(sdAvailable){ currentState = STATE_CAPTURE_SAVE; inputText = ""; }
      else setStatus("SD Card not available!");
    } else if(key == 't' || key == 'T'){
      selectedSignal = capturedSignal;
      bool ok = sendIR(selectedSignal);
      setStatus(ok ? "Signal sent!" : "Send failed!");
    } else if(key == 'v' || key == 'V'){
      selectedSignal = capturedSignal;
      previousState  = STATE_CAPTURE;
      currentState   = STATE_VIEW_CODE;
    } else if(key == 'r' || key == 'R'){
      signalCaptured = false; capturing = true;
      irRecv.resume(); animTimer = millis();
    }
  }
}

void handleSavePrompt(char key){
  if(key == 'y' || key == 'Y'){
    currentState = STATE_CAPTURE_SAVE; inputText = "";
  } else if(key == 'n' || key == 'N' || key == '`' || key == 27){
    capturing      = false; irRecv.pause();
    signalCaptured = false;
    currentState   = STATE_MAIN_MENU;
  }
}

void handleSaveDialog(char key){
  if(key == '`' || key == 27){
    currentState = STATE_CAPTURE;
  } else if(key == '\n' || key == '\r'){
    if(inputText.length() == 0){ setStatus("Name cannot be empty!"); return; }
    capturedSignal.name = inputText;
    if(saveSignalToSD(capturedSignal)){
      library.push_back(capturedSignal);
      setStatus("Saved: " + inputText + ".ir");
      currentState   = STATE_MAIN_MENU;
      signalCaptured = false; capturing = false; inputText = "";
    } else setStatus("Save failed!");
  } else if(key == 8 || key == 127){
    if(inputText.length() > 0) inputText.remove(inputText.length() - 1);
  } else if(key >= 32 && key <= 126 && inputText.length() < 20){
    inputText += key;
  }
}

void handleLibrary(char key){
  if(key == '`' || key == 27)
    currentState = STATE_MAIN_MENU;
  else if(key == ',' && libraryIndex > 0)
    libraryIndex--;
  else if(key == '.' && libraryIndex < (int)library.size() - 1)
    libraryIndex++;
  else if((key == '\n' || key == '\r' || key == ' ') && !library.empty()){
    selectedSignal = library[libraryIndex];
    previousState  = STATE_LIBRARY;
    currentState   = STATE_SEND_CONFIRM;
  } else if((key == 'v' || key == 'V') && !library.empty()){
    selectedSignal = library[libraryIndex];
    previousState  = STATE_LIBRARY;
    currentState   = STATE_VIEW_CODE;
  } else if((key == 'd' || key == 'D') && !library.empty()){
    if(deleteFromSD(library[libraryIndex].filename)){
      library.erase(library.begin() + libraryIndex);
      if(libraryIndex >= (int)library.size() && libraryIndex > 0) libraryIndex--;
      setStatus("Signal deleted");
    } else setStatus("Delete failed!");
  }
}

void handleViewCode(char key){
  if(key == '`' || key == 27)
    currentState = previousState;
  else if(key == 's' || key == 'S' || key == '\n' || key == '\r'){
    bool ok = sendIR(selectedSignal);
    setStatus(ok ? "Signal sent!" : "Send failed!");
  }
}

void handleSDBrowser(char key){
  if(key == '`' || key == 27)
    currentState = STATE_MAIN_MENU;
  else if(key == ',' && sdFileIndex > 0)
    sdFileIndex--;
  else if(key == '.' && sdFileIndex < (int)sdFiles.size() - 1)
    sdFileIndex++;
  else if((key == '\n' || key == '\r' || key == ' ') && !sdFiles.empty()){
    IRSignal sig;
    if(loadSignalFromSD(sdFiles[sdFileIndex], sig)){
      selectedSignal = sig;
      previousState  = STATE_SD_BROWSER;
      currentState   = STATE_SEND_CONFIRM;
    } else setStatus("Failed to load!");
  } else if(key == 'r' || key == 'R'){
    scanSDFiles(); sdFileIndex = 0; scrollOffset = 0; setStatus("SD refreshed");
  }
}

void handleSendConfirm(char key){
  if(key == '`' || key == 27){
    currentState = previousState;
  } else if(key == '\n' || key == '\r' || key == ' ' || key == 's' || key == 'S'){
    // Gönderme animasyonu
    canvas.fillScreen(COLOR_BG);
    drawTopBar("TRANSMIT");
    drawSendConfirm(true);
    drawToast();
    canvas.pushSprite(0, 0);
    bool ok = sendIR(selectedSignal);
    delay(400);
    setStatus(ok ? "Sent successfully!" : "Send failed!");
    currentState = previousState;
  }
}

//==============================================================
// SETUP & LOOP
//==============================================================
void setup(){
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(150);

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);

  irRecv.enableIRIn();
  irSend.begin();

  sdAvailable = initSD();
  if(sdAvailable) loadLibrary();

  animTimer = millis();
  drawScreen();
}

void loop(){
  M5Cardputer.update();
  handleIRReceive();

  if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()){
    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
    char key = 0;

    if(!status.word.empty()) key = status.word[0];

    if(!status.hid_keys.empty()){
      switch(status.hid_keys[0]){
        case 0x28: key = '\n'; break;  // Enter
        case 0x2A: key = 127;  break;  // Backspace
        case 0x29: key = '`';  break;  // ESC
        default: break;
      }
    }

    if(key != 0){
      switch(currentState){
        case STATE_MAIN_MENU:    handleMainMenu(key);    break;
        case STATE_CAPTURE:      handleCapture(key);     break;
        case STATE_SAVE_PROMPT:  handleSavePrompt(key);  break;
        case STATE_CAPTURE_SAVE: handleSaveDialog(key);  break;
        case STATE_LIBRARY:      handleLibrary(key);     break;
        case STATE_VIEW_CODE:    handleViewCode(key);    break;
        case STATE_SD_BROWSER:   handleSDBrowser(key);   break;
        case STATE_SEND_CONFIRM: handleSendConfirm(key); break;
      }
      needsRedraw = true;
    }
  }

  // Sürekli yeniden çizim gerektiren durumlar
  if(currentState == STATE_MAIN_MENU ||
    (currentState == STATE_CAPTURE && !signalCaptured) ||
     currentState == STATE_CAPTURE_SAVE ||
     currentState == STATE_SEND_CONFIRM)
    needsRedraw = true;

  if(statusMsg.length() > 0 && millis() - statusTime < 2500)
    needsRedraw = true;

  if(needsRedraw){ drawScreen(); needsRedraw = false; }
  delay(16);
}
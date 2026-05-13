#pragma once

#include "./GlobalParentClass.h"
#include <Arduino.h>
#include <M5Cardputer.h>

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <SD.h>
#include <SPI.h>
#include <vector>

#define IR_RECV_PIN          1
#define IR_SEND_PIN_DEFAULT  44

#define COLOR_BG      TFT_BLACK
#define COLOR_TEXT    TFT_WHITE
#define COLOR_SUCCESS 0x00FF66
#define COLOR_WARNING 0xFFCC00
#define COLOR_ACCENT  0xFF4422

#define ITEM_H       28
#define MENU_START_Y 28

#ifndef SCREEN_W
  #define SCREEN_W 240
#endif
#ifndef SCREEN_H
  #define SCREEN_H 135
#endif

// ----------------------------------------------------------------
// Uygulama durumları
// ----------------------------------------------------------------
enum AppState
{
    STATE_MAIN_MENU,
    STATE_CAPTURE,
    STATE_SAVE_PROMPT,
    STATE_CAPTURE_SAVE,
    STATE_VIEW_CODE,
    STATE_SD_BROWSER,
    STATE_SEND_CONFIRM,
    STATE_SETTINGS,
    STATE_REMOTE_PAD
};

// ----------------------------------------------------------------
// Tek bir IR komutu  (Flipper .ir dosyasındaki bir blok)
// ----------------------------------------------------------------
struct IRCommand
{
    String   label;        // name: satırı
    String   type;         // "parsed" veya "raw"
    String   protocol;     // NEC, SAMSUNG, vb.
    uint32_t address;      // address: alanı (parsed)
    uint32_t command;      // command: alanı (parsed)
    uint32_t frequency;    // raw için
    float    dutyCycle;    // raw için
    std::vector<uint16_t> rawData;  // raw için data:
};

// ----------------------------------------------------------------
// IR sinyal dosyası  (bir .ir dosyası = birden fazla komut)
// ----------------------------------------------------------------
struct IRSignal
{
    String   name;         // Dosya/grup adı
    String   filename;     // SD'deki tam yol
    bool     isMulti;      // Birden fazla komut var mı?
    std::vector<IRCommand> commands;

    // Hızlı erişim (ilk komut veya tek komut)
    String   protocol;
    uint32_t address;
    uint32_t command;
    uint32_t frequency;
    bool     isRaw;
    std::vector<uint16_t> rawData;
};

// ----------------------------------------------------------------
// SD browser liste öğesi
// ----------------------------------------------------------------
struct SDFileInfo
{
    String path;
    String displayName;
    bool   isMulti;
    int    cmdCount;
};

// ================================================================
class Cardputer_Remote : public GlobalParentClass
{
public:
    Cardputer_Remote(MyOS *os)
        : GlobalParentClass(os),
          irRecv(IR_RECV_PIN),
          irSend(IR_SEND_PIN_DEFAULT)
    {}

    void Begin()   override;
    void Loop()    override;
    void Draw()    override;
    void OnExit()  override;

private:
    // Donanım
    IRrecv         irRecv;
    IRsend         irSend;
    decode_results irResults;

    // Durum
    AppState currentState  = STATE_MAIN_MENU;
    AppState previousState = STATE_MAIN_MENU;

    // Sinyaller
    IRSignal capturedSignal;
    IRSignal selectedSignal;
    std::vector<SDFileInfo> sdFiles;

    // UI
    int  menuIndex    = 0;
    int  sdFileIndex  = 0;
    int  scrollOffset = 0;
    int  settingsIndex= 0;
    int  remotePadIdx = 0;

    // Capture
    bool capturing      = false;
    bool signalCaptured = false;

    // Giriş & durum
    String        inputText  = "";
    String        statusMsg  = "";
    unsigned long statusTime = 0;
    unsigned long lastCapture= 0;
    unsigned long animTimer  = 0;
    bool          needsRedraw= true;

    // Ayarlar
    uint8_t sendPin  = IR_SEND_PIN_DEFAULT;
    bool    usePin44 = true;
    bool    sdAvailable = true;

    // Menü
    static const int MAIN_MENU_COUNT = 3;
    const char* mainMenuItems[MAIN_MENU_COUNT] = {
        "Capture IR Signal",
        "SD Card Browser",
        "Settings"
    };
    const uint32_t menuColors[MAIN_MENU_COUNT] = {
        0xFF4422, 0x00CC44, 0x886600
    };

    // ---- Yardımcı ----
    void    setStatus(String msg);
    String  protoName(decode_type_t p);
    String  protoNameFromStr(String p); // görüntü için
    decode_type_t strToProto(String s);

    // ---- Flipper IR format ----
    bool    saveSignalToSD(IRSignal &sig);
    bool    loadSignalFromSD(String filename, IRSignal &sig);
    void    scanSDFiles();
    void    scanSDRecursive(File dir, const String &basePath);
    bool    deleteFromSD(String fn);
    bool    initSD();

    // ---- IR gönder ----
    bool    sendIRCommand(IRCommand &cmd);
    bool    sendIR(IRSignal &sig);
    void    reinitIRSend();

    // ---- Draw ----
    void    drawTopBar(String title);
    void    drawToast();
    void    drawBottomBar(String hint);
    void    drawMainMenu();
    void    drawCaptureScreen();
    void    drawSavePrompt();
    void    drawSaveDialog();
    void    drawViewCodeScreen();
    void    drawSDBrowserScreen();
    void    drawSendConfirm(bool sending);
    void    drawSettingsScreen();
    void    drawRemotePad();
    void    drawScreen();

    // ---- Key handlers ----
    void    handleIRReceive();
    void    handleMainMenu(char key);
    void    handleCapture(char key);
    void    handleSavePrompt(char key);
    void    handleSaveDialog(char key);
    void    handleViewCode(char key);
    void    handleSDBrowser(char key);
    void    handleSendConfirm(char key);
    void    handleSettings(char key);
    void    handleRemotePad(char key);
};

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
#define IR_SEND_PIN_G44      44
#define IR_SEND_PIN_G2       2
#define IR_SEND_PIN_DEFAULT  IR_SEND_PIN_G44

#define COLOR_BG      TFT_BLACK
#define COLOR_TEXT    TFT_WHITE
#define COLOR_SUCCESS 0x00FF66
#define COLOR_WARNING 0xFFCC00
#define COLOR_ACCENT  0xFF4422

#define ITEM_H       28
#define MENU_START_Y 28
#define MAX_RAW_LEN  200

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
// Tek bir IR komutu
// ----------------------------------------------------------------
struct IRCommand
{
    String   label     = "";
    String   type      = "parsed";
    String   protocol  = "";
    uint32_t address   = 0;
    uint32_t command   = 0;
    uint32_t frequency = 38000;
    float    dutyCycle = 0.33f;
    std::vector<uint16_t> rawData;

    IRCommand() {}
};

// ----------------------------------------------------------------
// IR sinyal dosyası
// ----------------------------------------------------------------
struct IRSignal
{
    String   name      = "";
    String   filename  = "";
    bool     isMulti   = false;
    bool     isRaw     = false;
    String   protocol  = "";
    uint32_t address   = 0;
    uint32_t command   = 0;
    uint32_t frequency = 38000;
    std::vector<IRCommand> commands;
    std::vector<uint16_t>  rawData;

    void clear()
    {
        name = filename = protocol = "";
        isMulti = isRaw = false;
        address = command = 0;
        frequency = 38000;
        commands.clear();
        rawData.clear();
    }
};

// ----------------------------------------------------------------
// SD browser liste öğesi
// ----------------------------------------------------------------
struct SDFileInfo
{
    String path        = "";
    String displayName = "";
    bool   isMulti     = false;
    int    cmdCount    = 0;
};

// ================================================================
class Cardputer_Remote : public GlobalParentClass
{
public:
    // FIX: IRsend pointer olarak tutulacak, pin değişiminde yeniden oluşturulacak
    Cardputer_Remote(MyOS *os)
        : GlobalParentClass(os),
          irRecv(IR_RECV_PIN),
          irSendPtr(nullptr),
          sendPin(IR_SEND_PIN_DEFAULT),
          usePin44(true)
    {}

    ~Cardputer_Remote()
    {
        if (irSendPtr)
        {
            delete irSendPtr;
            irSendPtr = nullptr;
        }
    }

    void Begin()   override;
    void Loop()    override;
    void Draw()    override;
    void OnExit()  override;

private:
    // Donanım
    IRrecv         irRecv;
    IRsend*        irSendPtr;   // FIX: Pointer — runtime pin değişimine izin verir
    decode_results irResults;

    // Durum
    AppState currentState  = STATE_MAIN_MENU;
    AppState previousState = STATE_MAIN_MENU;

    // Sinyaller
    IRSignal capturedSignal;
    IRSignal selectedSignal;
    std::vector<SDFileInfo> sdFiles;

    // UI
    int  menuIndex     = 0;
    int  sdFileIndex   = 0;
    int  scrollOffset  = 0;
    int  settingsIndex = 0;
    int  remotePadIdx  = 0;

    // Capture
    bool capturing      = false;
    bool signalCaptured = false;
    bool spriteCreated  = false;

    // Giriş & durum
    String        inputText   = "";
    String        statusMsg   = "";
    unsigned long statusTime  = 0;
    unsigned long lastCapture = 0;
    unsigned long animTimer   = 0;
    bool          needsRedraw = true;

    // Ayarlar
    uint8_t sendPin;
    bool    usePin44;
    bool    sdAvailable = false;

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
    void          setStatus(String msg);
    String        protoName(decode_type_t p);
    decode_type_t strToProto(String s);
    static String trimStr(String s);
    static uint32_t parseHexBytes(String s);

    // ---- Flipper IR format ----
    bool saveSignalToSD(IRSignal &sig);
    bool loadSignalFromSD(const String &filename, IRSignal &sig);
    void scanSDFiles();
    void scanSDRecursive(const String &dirPath, int depth = 0);
    bool deleteFromSD(const String &fn);
    bool initSD();

    // ---- IR gönder ----
    bool sendIRCommand(IRCommand &cmd);
    bool sendIR(IRSignal &sig);
    void reinitIRSend();          // FIX: Pointer delete/new yapar
    void createIRSend(uint8_t pin); // FIX: Yeni IRsend oluşturur

    // ---- Draw ----
    void drawTopBar(const String &title);
    void drawToast();
    void drawBottomBar(const String &hint);
    void drawMainMenu();
    void drawCaptureScreen();
    void drawSavePrompt();
    void drawSaveDialog();
    void drawViewCodeScreen();
    void drawSDBrowserScreen();
    void drawSendConfirm(bool sending);
    void drawSettingsScreen();
    void drawRemotePad();
    void drawScreen();

    // ---- Key handlers ----
    void handleIRReceive();
    void handleMainMenu(char key);
    void handleCapture(char key);
    void handleSavePrompt(char key);
    void handleSaveDialog(char key);
    void handleViewCode(char key);
    void handleSDBrowser(char key);
    void handleSendConfirm(char key);
    void handleSettings(char key);
    void handleRemotePad(char key);
};

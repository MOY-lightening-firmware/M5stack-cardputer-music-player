#pragma once

/*
 * 🎧 MOY MUSIC - DJ Edition v6.0 🎧
 * M5Stack Cardputer
 * Made by Andy+AI
 * Header File
 */

#include <M5Cardputer.h>
#include <SPI.h>
#include <unit_audioplayer.hpp>

// ========== RENK TANIMLARI ==========
#define COLOR_BG       0x0000
#define COLOR_ACCENT1  0x07FF  // Cyan
#define COLOR_ACCENT2  0xF81F  // Magenta
#define COLOR_ACCENT3  0xFFE0  // Yellow
#define COLOR_GREEN    0x07E0
#define COLOR_ORANGE   0xFD20
#define COLOR_RED      0xF800
#define COLOR_PURPLE   0x801F
#define COLOR_PINK     0xF81F
#define COLOR_DARKBG   0x0821
#define COLOR_PANEL    0x0410

// ========== DİSK SABİTLERİ ==========
#define DISK_CX 182
#define DISK_CY 66
#define DISK_R  43

// ========== GLOBAL DEĞİŞKENLER (extern) ==========
extern AudioPlayerUnit audioplayer;

extern unsigned long lastClickTime;
extern int           clickCount;
extern const unsigned long clickTimeout;

extern uint8_t  lastPlayStatus;
extern uint16_t currentTrack;
extern uint16_t lastDisplayedTrack;
extern uint16_t totalTracks;

extern unsigned long trackStartTime;
extern unsigned long pausedTime;
extern unsigned long totalPausedTime;
extern bool isPlaying;

extern bool   isLoopEnabled;
extern bool   isShuffleEnabled;
extern bool   isInputMode;
extern String inputTrackNumber;

extern unsigned long lastBatteryUpdate;
extern const unsigned long batteryUpdateInterval;
extern unsigned long lastTimeUpdate;
extern const unsigned long timeUpdateInterval;
extern int currentVolume;

// Disk
extern float         diskAngle;
extern unsigned long lastDiskUpdate;
extern const unsigned long diskUpdateInterval;
extern float diskSpeed;
extern float targetDiskSpeed;
extern bool  lastDiskPlaying;

// VU Meter
extern int vuLeft[12];
extern int vuRight[12];
extern unsigned long lastVuUpdate;
extern const unsigned long vuUpdateInterval;

// Geçici mesaj
extern unsigned long tempMessageTime;
extern bool          tempMessageActive;
extern const unsigned long tempMessageDuration;
extern String   tempMessageText;
extern uint16_t tempMessageColor;

// Ekranlar
extern bool isFullscreenVisualizer;
extern int  visualizerMode;
extern bool isInfoScreen;

// Matrix
struct MatrixColumn {
    int  y;
    int  speed;
    int  trailLength;
    char character;
};

extern MatrixColumn matrixColumns[40];
extern const char   matrixChars[];
extern unsigned long lastMatrixUpdate;

extern int   fullscreenBarHeights[20];
extern float wavePhase;

// Animasyon
extern float         pulsePhase;
extern unsigned long lastPulse;
extern float         rainbowOffset;

// ========== YARDIMCI FONKSİYONLAR ==========
uint16_t hsvToRgb565(float h, float s, float v);
void     drawRainbowLine(int y, float br = 0.9f, float off = 0.0f);
void     drawGlowRect(int x, int y, int w, int h,
                      uint16_t col, uint16_t inner);
void     drawNeonText(int x, int y, const char* txt,
                      uint16_t col, uint8_t sz = 1);

// ========== SPLASH ==========
void showSplash();

// ========== DİSK ==========
void drawDiskBackground();
void drawDiskDynamic(float angle);
void updateDiskRotation();
void drawFullDisk(float angle);

// ========== VU METER ==========
void updateVuMeter();
void drawVuMeter();

// ========== PANELLER ==========
void drawLeftPanel();
void drawBottomBar();
void drawTopBar();
void drawMainScreen();

// ========== GEÇİCİ MESAJ ==========
void drawTempMsg();
void clearTempArea();
void showTempMsg(String msg, uint16_t col);

// ========== GÜNCELLEME ==========
void updateTrackDisplay();
void updateStatusDisplay();
void updateModeDisplay();
void updateTimeDisplay();
void updateBatteryDisplay();
void updateMainDisplay();

// ========== INPUT MODE ==========
void drawInputMode();

// ========== INFO ==========
void enterInfoScreen();
void exitInfoScreen();

// ========== FULLSCREEN VIZ ==========
void initMatrixColumns();
void drawVisualizerHeader();
void updateFullscreenVisualizer();
void drawFullscreenBars();
void drawFullscreenWaves();
void drawMatrixRain();
void drawFullscreenTempMessage(String msg, uint16_t col);
void enterFullscreenVisualizer();
void switchVisualizerMode();
void exitFullscreenVisualizer();

// ========== KONTROL ==========
void volumeUp();
void volumeDown();
void toggleLoop();
void toggleShuffle();
void goToTrack(int n);
void randomTrack();
void executeAction(int clicks);
void checkAutoNext();

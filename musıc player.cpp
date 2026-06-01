/*
 * 🎧 MOY MUSIC - DJ Edition v6.0 🎧
 * M5Stack Cardputer
 * Made by Andy+AI
 * Implementation File
 */

#include "moy_music.h"

// ========== GLOBAL DEĞİŞKENLER (tanım) ==========
AudioPlayerUnit audioplayer;

unsigned long lastClickTime = 0;
int           clickCount    = 0;
const unsigned long clickTimeout = 500;

uint8_t  lastPlayStatus     = AUDIO_PLAYER_STATUS_STOPPED;
uint16_t currentTrack       = 1;
uint16_t lastDisplayedTrack = 1;
uint16_t totalTracks        = 0;

unsigned long trackStartTime  = 0;
unsigned long pausedTime      = 0;
unsigned long totalPausedTime = 0;
bool isPlaying = false;

bool   isLoopEnabled    = false;
bool   isShuffleEnabled = false;
bool   isInputMode      = false;
String inputTrackNumber = "";

unsigned long lastBatteryUpdate = 0;
const unsigned long batteryUpdateInterval = 5000;
unsigned long lastTimeUpdate = 0;
const unsigned long timeUpdateInterval = 1000;
int currentVolume = 25;

// Disk
float         diskAngle   = 0.0f;
unsigned long lastDiskUpdate = 0;
const unsigned long diskUpdateInterval = 50;
float diskSpeed       = 0.0f;
float targetDiskSpeed = 0.0f;
bool  lastDiskPlaying = false;

// VU Meter
int vuLeft[12]  = {0};
int vuRight[12] = {0};
unsigned long lastVuUpdate = 0;
const unsigned long vuUpdateInterval = 80;

// Geçici mesaj
unsigned long tempMessageTime = 0;
bool          tempMessageActive = false;
const unsigned long tempMessageDuration = 2000;
String   tempMessageText  = "";
uint16_t tempMessageColor = WHITE;

// Ekranlar
bool isFullscreenVisualizer = false;
int  visualizerMode = 0;
bool isInfoScreen   = false;

// Matrix
MatrixColumn matrixColumns[40];
const char   matrixChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$%&*";
unsigned long lastMatrixUpdate = 0;

int   fullscreenBarHeights[20] = {0};
float wavePhase = 0.0f;

// Animasyon
float         pulsePhase    = 0.0f;
unsigned long lastPulse     = 0;
float         rainbowOffset = 0.0f;

// ========== YARDIMCI ==========

uint16_t hsvToRgb565(float h, float s, float v) {
    float r, g, b;
    int   i = (int)(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return M5Cardputer.Display.color565(
        (uint8_t)(r * 255),
        (uint8_t)(g * 255),
        (uint8_t)(b * 255));
}

void drawRainbowLine(int y, float br, float off) {
    for (int x = 0; x < 240; x++)
        M5Cardputer.Display.drawPixel(
            x, y,
            hsvToRgb565(fmod((float)x / 240.0f + off, 1.0f), 1.0f, br));
}

void drawGlowRect(int x, int y, int w, int h,
                  uint16_t col, uint16_t inner) {
    M5Cardputer.Display.fillRoundRect(x, y, w, h, 3, inner);
    M5Cardputer.Display.drawRoundRect(x, y, w, h, 3, col);
    M5Cardputer.Display.drawRoundRect(
        x - 1, y - 1, w + 2, h + 2, 4,
        M5Cardputer.Display.color565(
            ((col >> 11) & 0x1F) << 2,
            ((col >> 5)  & 0x3F) << 1,
            (col & 0x1F) << 2));
}

void drawNeonText(int x, int y, const char* txt,
                  uint16_t col, uint8_t sz) {
    M5Cardputer.Display.setTextSize(sz);
    uint16_t gc = M5Cardputer.Display.color565(
        (((col >> 11) & 0x1F) << 2) >> 2,
        (((col >> 5)  & 0x3F) << 1) >> 2,
        ((col & 0x1F) << 2) >> 2);
    M5Cardputer.Display.setTextColor(gc, COLOR_BG);
    M5Cardputer.Display.setCursor(x + 1, y + 1);
    M5Cardputer.Display.print(txt);
    M5Cardputer.Display.setTextColor(col, COLOR_BG);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(txt);
}

// ========== SPLASH ==========

void showSplash() {
    M5Cardputer.Display.fillScreen(COLOR_BG);

    // Arka plan grid
    for (int y = 40; y < 135; y += 8) {
        uint16_t gc = M5Cardputer.Display.color565(
            0, 0, (uint8_t)(15 * (y - 40) / 95));
        M5Cardputer.Display.drawFastHLine(0, y, 240, gc);
    }
    for (int x = 0; x < 240; x += 20) {
        M5Cardputer.Display.drawFastVLine(
            x, 40, 95,
            M5Cardputer.Display.color565(0, 0, 10));
    }

    // Üst neon çizgiler
    for (int i = 0; i < 3; i++)
        drawRainbowLine(i, 0.9f, (float)i * 0.05f);

    // DJ Sahne ışıkları
    for (int i = 0; i < 5; i++) {
        int      sx = 24 + i * 48, sy = 40;
        uint16_t sc = hsvToRgb565((float)i / 5.0f, 1.0f, 0.7f);
        for (int r = 0; r < 12; r++) {
            float alpha = 1.0f - (float)r / 12.0f;
            M5Cardputer.Display.drawLine(
                sx, sy - 5, sx - r * 2, sy + r * 3,
                M5Cardputer.Display.color565(0, 0, (uint8_t)(30 * alpha)));
            M5Cardputer.Display.drawLine(
                sx, sy - 5, sx + r * 2, sy + r * 3,
                M5Cardputer.Display.color565(0, 0, (uint8_t)(30 * alpha)));
        }
        M5Cardputer.Display.fillCircle(sx, sy, 4, sc);
        M5Cardputer.Display.drawCircle(sx, sy, 5, 0xFFFF);
    }

    // Ana logo
    M5Cardputer.Display.setTextSize(3);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, COLOR_BG);
    M5Cardputer.Display.setCursor(10, 50);
    M5Cardputer.Display.print("MOY");
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, COLOR_BG);
    M5Cardputer.Display.setCursor(64, 50);
    M5Cardputer.Display.print("MUSIC");

    // Alt çizgi
    M5Cardputer.Display.fillRect(0, 76, 240, 2, COLOR_ACCENT2);
    drawRainbowLine(77, 1.0f);
    M5Cardputer.Display.fillRect(0, 78, 240, 2, COLOR_ACCENT1);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, COLOR_BG);
    M5Cardputer.Display.setCursor(58, 84);
    M5Cardputer.Display.print("[ DJ AUDIO PLAYER v6.0 ]");

    M5Cardputer.Display.setTextColor(COLOR_GREEN, COLOR_BG);
    M5Cardputer.Display.setCursor(8, 84);
    M5Cardputer.Display.print("\x0E");
    M5Cardputer.Display.setCursor(224, 84);
    M5Cardputer.Display.print("\x0E");

    M5Cardputer.Display.setTextColor(0x528A, COLOR_BG);
    M5Cardputer.Display.setCursor(74, 94);
    M5Cardputer.Display.print("by Andy + AI");

    // EQ çubukları
    int eqH[] = {8, 14, 20, 16, 22, 18, 12, 24, 20, 16, 14, 10, 18, 22, 16};
    for (int i = 0; i < 15; i++) {
        uint16_t ec = hsvToRgb565((float)i / 15.0f, 1.0f, 0.9f);
        M5Cardputer.Display.fillRect(
            8 + i * 15, 120 - eqH[i], 12, eqH[i], ec);
        M5Cardputer.Display.drawRect(
            8 + i * 15, 120 - eqH[i], 12, eqH[i],
            M5Cardputer.Display.color565(255, 255, 255));
    }

    for (int i = 0; i < 3; i++)
        drawRainbowLine(120 + i, 0.8f, (float)i * 0.1f);

    M5Cardputer.Display.setTextColor(0x4208, COLOR_BG);
    M5Cardputer.Display.setCursor(52, 126);
    M5Cardputer.Display.print(">> Press any key to DROP <<");

    unsigned long t = millis();
    while (millis() - t < 4000) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed())
            break;
        delay(50);
    }
    M5Cardputer.Display.fillScreen(COLOR_BG);
}

// ========== VİNİL DİSK ==========

void drawDiskBackground() {
    for (int r = DISK_R + 3; r >= DISK_R; r--) {
        float br = 0.2f + (float)(DISK_R + 3 - r) * 0.15f;
        M5Cardputer.Display.drawCircle(
            DISK_CX, DISK_CY, r,
            hsvToRgb565(0.75f, 1.0f, br));
    }
    M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, DISK_R, 0x0821);

    int rr[] = {38, 35, 32, 29, 26, 23, 20, 17};
    for (int i = 0; i < 8; i++) {
        float hue = (float)i / 8.0f * 0.6f + 0.6f;
        M5Cardputer.Display.drawCircle(
            DISK_CX, DISK_CY, rr[i],
            hsvToRgb565(hue, 0.5f, 0.2f));
    }
}

void drawDiskDynamic(float angle) {
    // Dönen gökkuşağı dış halka
    for (int r = DISK_R - 1; r > DISK_R - 7; r--) {
        for (int a = 0; a < 360; a += 3) {
            float rad = a * 3.14159f / 180.0f;
            float hue = fmod((float)a / 360.0f + angle / 360.0f, 1.0f);
            int   px  = DISK_CX + (int)(r * cosf(rad));
            int   py  = DISK_CY + (int)(r * sinf(rad));
            M5Cardputer.Display.drawPixel(
                px, py, hsvToRgb565(hue, 1.0f, 0.6f));
        }
    }

    // Dönen parlak nokta
    static float lastSA = 0;
    {
        float lr = lastSA * 3.14159f / 180.0f;
        int   lx = DISK_CX + (int)((DISK_R - 8) * cosf(lr));
        int   ly = DISK_CY + (int)((DISK_R - 8) * sinf(lr));
        M5Cardputer.Display.fillCircle(lx, ly, 3, 0x0821);
    }
    lastSA    = angle;
    float rad = angle * 3.14159f / 180.0f;
    int   sx  = DISK_CX + (int)((DISK_R - 8) * cosf(rad));
    int   sy  = DISK_CY + (int)((DISK_R - 8) * sinf(rad));
    M5Cardputer.Display.fillCircle(sx, sy, 3, 0xFFFF);
    M5Cardputer.Display.fillCircle(sx, sy, 1, COLOR_ACCENT1);

    // Merkez etiket
    M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 15, 0x0821);
    if (isPlaying) {
        float hue = fmod(angle / 360.0f, 1.0f);
        for (int r = 15; r >= 13; r--)
            M5Cardputer.Display.drawCircle(
                DISK_CX, DISK_CY, r,
                hsvToRgb565(hue, 1.0f, 0.9f));
        M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 12, COLOR_BG);
        M5Cardputer.Display.fillCircle(
            DISK_CX, DISK_CY, 9,
            hsvToRgb565(fmod(hue + 0.5f, 1.0f), 0.9f, 1.0f));
        M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 6, COLOR_BG);
        M5Cardputer.Display.fillCircle(
            DISK_CX, DISK_CY, 3,
            hsvToRgb565(fmod(hue + 0.25f, 1.0f), 1.0f, 1.0f));
    } else {
        M5Cardputer.Display.drawCircle(DISK_CX, DISK_CY, 14, 0x2945);
        M5Cardputer.Display.drawCircle(DISK_CX, DISK_CY, 12, 0x1082);
        M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 9,  0x1082);
        M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 4,  0x2945);
    }
    M5Cardputer.Display.fillCircle(DISK_CX, DISK_CY, 2, COLOR_BG);
    M5Cardputer.Display.drawCircle(DISK_CX, DISK_CY, 2, 0x8410);
}

void updateDiskRotation() {
    targetDiskSpeed = isPlaying ? 5.0f : 0.0f;
    if (diskSpeed < targetDiskSpeed) {
        diskSpeed += 0.3f;
        if (diskSpeed > targetDiskSpeed) diskSpeed = targetDiskSpeed;
    } else if (diskSpeed > targetDiskSpeed) {
        diskSpeed -= 0.15f;
        if (diskSpeed < 0) diskSpeed = 0;
    }
    diskAngle = fmod(diskAngle + diskSpeed, 360.0f);
}

void drawFullDisk(float angle) {
    M5Cardputer.Display.fillRect(136, 18, 104, 100, COLOR_BG);
    drawDiskBackground();
    drawDiskDynamic(angle);
}

// ========== VU METER ==========

void updateVuMeter() {
    if (!isPlaying) {
        for (int i = 0; i < 12; i++) {
            if (vuLeft[i]  > 0) vuLeft[i]--;
            if (vuRight[i] > 0) vuRight[i]--;
        }
        return;
    }
    int peak = random(5, 12);
    for (int i = 0; i < 12; i++)
        vuLeft[i] = (i < peak) ? 1 : max(0, vuLeft[i] - 1);
    peak = random(4, 11);
    for (int i = 0; i < 12; i++)
        vuRight[i] = (i < peak) ? 1 : max(0, vuRight[i] - 1);
}

void drawVuMeter() {
    int bW = 7, bH = 5, bG = 1, sx = 4, lyY = 80, ryY = 88;

    // Sol kanal
    for (int i = 0; i < 12; i++) {
        uint16_t on_c, off_c;
        if      (i < 6)  { on_c = COLOR_GREEN;   off_c = 0x0841; }
        else if (i < 9)  { on_c = COLOR_ACCENT3; off_c = 0x0841; }
        else if (i < 11) { on_c = COLOR_ORANGE;  off_c = 0x0841; }
        else             { on_c = COLOR_RED;      off_c = 0x0841; }
        M5Cardputer.Display.fillRect(
            sx + i * (bW + bG), lyY, bW, bH,
            vuLeft[i] ? on_c : off_c);
        if (vuLeft[i])
            M5Cardputer.Display.drawFastHLine(
                sx + i * (bW + bG), lyY, bW, 0xFFFF);
    }

    // Sağ kanal
    for (int i = 0; i < 12; i++) {
        uint16_t on_c, off_c;
        if      (i < 6)  { on_c = COLOR_GREEN;   off_c = 0x0841; }
        else if (i < 9)  { on_c = COLOR_ACCENT3; off_c = 0x0841; }
        else if (i < 11) { on_c = COLOR_ORANGE;  off_c = 0x0841; }
        else             { on_c = COLOR_RED;      off_c = 0x0841; }
        M5Cardputer.Display.fillRect(
            sx + i * (bW + bG), ryY, bW, bH,
            vuRight[i] ? on_c : off_c);
        if (vuRight[i])
            M5Cardputer.Display.drawFastHLine(
                sx + i * (bW + bG), ryY, bW, 0xFFFF);
    }
}

// ========== SOL PANEL ==========

void drawLeftPanel() {
    // Panel arka plan
    for (int y = 18; y < 118; y++) {
        float t = (float)(y - 18) / 100.0f;
        M5Cardputer.Display.drawFastHLine(
            0, y, 136,
            M5Cardputer.Display.color565(
                (uint8_t)(4 * t),
                (uint8_t)(8 * (1 - t) * t * 4),
                (uint8_t)(16 * (1 - t) * t * 4)));
    }

    // Track header
    M5Cardputer.Display.drawFastHLine(0, 18, 136, COLOR_ACCENT2);
    M5Cardputer.Display.fillRect(0, 19, 136, 16, 0x0821);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, 0x0821);
    M5Cardputer.Display.setCursor(4, 23);
    M5Cardputer.Display.print("\x0E TRACK");

    char tot[10];
    sprintf(tot, "/%d", totalTracks);
    M5Cardputer.Display.setTextColor(0x528A, 0x0821);
    M5Cardputer.Display.setCursor(90, 23);
    M5Cardputer.Display.print(tot);
    M5Cardputer.Display.drawFastHLine(0, 35, 136, COLOR_ACCENT2);

    // Büyük track numarası
    M5Cardputer.Display.fillRect(0, 36, 136, 28, COLOR_BG);
    char     ts[6];
    sprintf(ts, "%03d", currentTrack);
    uint16_t numCol = isPlaying ? COLOR_ACCENT1 : 0x4208;
    M5Cardputer.Display.setTextSize(3);

    if (isPlaying) {
        M5Cardputer.Display.setTextColor(
            M5Cardputer.Display.color565(0, 80, 120), COLOR_BG);
        M5Cardputer.Display.setCursor(5, 39);
        M5Cardputer.Display.print(ts);
        M5Cardputer.Display.setTextColor(
            M5Cardputer.Display.color565(0, 180, 220), COLOR_BG);
        M5Cardputer.Display.setCursor(4, 38);
        M5Cardputer.Display.print(ts);
    }
    M5Cardputer.Display.setTextColor(numCol, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 37);
    M5Cardputer.Display.print(ts);

    // TIME
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, COLOR_BG);
    M5Cardputer.Display.setCursor(72, 37);
    M5Cardputer.Display.print("TIME");

    unsigned long el = isPlaying
        ? (millis() - trackStartTime - totalPausedTime) / 1000
        : 0;
    M5Cardputer.Display.setTextColor(0xFFFF, COLOR_BG);
    M5Cardputer.Display.setCursor(68, 47);
    M5Cardputer.Display.printf(
        "%02d:%02d", (int)(el / 60), (int)(el % 60));

    M5Cardputer.Display.drawFastHLine(0, 64, 136, 0x2945);

    // Play status bar
    M5Cardputer.Display.fillRect(0, 65, 136, 15, COLOR_BG);
    if (isPlaying) {
        M5Cardputer.Display.fillRoundRect(
            2, 66, 58, 12, 2,
            M5Cardputer.Display.color565(0, 60, 0));
        M5Cardputer.Display.drawRoundRect(2, 66, 58, 12, 2, COLOR_GREEN);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(
            COLOR_GREEN,
            M5Cardputer.Display.color565(0, 60, 0));
        M5Cardputer.Display.setCursor(7, 69);
        M5Cardputer.Display.print("\x10 PLAYING");
    } else {
        M5Cardputer.Display.fillRoundRect(
            2, 66, 55, 12, 2,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.drawRoundRect(2, 66, 55, 12, 2, COLOR_ACCENT3);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT3,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.setCursor(7, 69);
        M5Cardputer.Display.print("|| PAUSED");
    }

    // Mod rozeti
    M5Cardputer.Display.fillRect(64, 66, 70, 12, COLOR_BG);
    if (isLoopEnabled) {
        M5Cardputer.Display.fillRoundRect(
            64, 66, 68, 12, 2,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.drawRoundRect(64, 66, 68, 12, 2, COLOR_ACCENT3);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT3,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("\x1D LOOP");
    } else if (isShuffleEnabled) {
        M5Cardputer.Display.fillRoundRect(
            64, 66, 68, 12, 2,
            M5Cardputer.Display.color565(0, 0, 60));
        M5Cardputer.Display.drawRoundRect(64, 66, 68, 12, 2, COLOR_ACCENT1);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT1,
            M5Cardputer.Display.color565(0, 0, 60));
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("~ SHUF");
    } else {
        M5Cardputer.Display.setTextColor(0x2945, COLOR_BG);
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("= SEQ");
    }

    M5Cardputer.Display.drawFastHLine(0, 78, 136, 0x1082);

    // VU Meter alanı
    M5Cardputer.Display.fillRect(0, 79, 136, 19, COLOR_BG);
    drawVuMeter();
    M5Cardputer.Display.drawFastHLine(0, 98, 136, 0x1082);

    // Hot keys
    M5Cardputer.Display.fillRect(0, 99, 136, 19, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 101);
    M5Cardputer.Display.print("SPC");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Play ");
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, COLOR_BG);
    M5Cardputer.Display.print("+/-");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Vol");

    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 110);
    M5Cardputer.Display.print("G");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":GoTo ");
    M5Cardputer.Display.setTextColor(COLOR_GREEN, COLOR_BG);
    M5Cardputer.Display.print("V");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Viz ");
    M5Cardputer.Display.setTextColor(COLOR_ORANGE, COLOR_BG);
    M5Cardputer.Display.print("I");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Info");
}

// ========== ALT BAR ==========

void drawBottomBar() {
    M5Cardputer.Display.fillRect(0, 118, 240, 17, COLOR_BG);
    drawRainbowLine(118, 0.7f, rainbowOffset);
    M5Cardputer.Display.fillRect(0, 119, 240, 16, 0x0410);

    int vp = (currentVolume * 100) / 30;

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, 0x0410);
    M5Cardputer.Display.setCursor(3, 123);
    M5Cardputer.Display.print("VOL");

    int bX = 26, bY = 121, sW = 4, sH = 9, sG = 1, tot = 16;
    int act = (vp * tot) / 100;
    for (int i = 0; i < tot; i++) {
        if (i < act) {
            uint16_t c;
            if      (i < 7)  c = COLOR_GREEN;
            else if (i < 11) c = COLOR_ACCENT3;
            else if (i < 14) c = COLOR_ORANGE;
            else             c = COLOR_RED;
            M5Cardputer.Display.fillRect(bX + i * (sW + sG), bY, sW, sH, c);
            M5Cardputer.Display.drawFastHLine(bX + i * (sW + sG), bY, sW, 0xFFFF);
        } else {
            M5Cardputer.Display.fillRect(bX + i * (sW + sG), bY, sW, sH, 0x0820);
        }
    }

    char vstr[6];
    sprintf(vstr, "%3d%%", vp);
    M5Cardputer.Display.setTextColor(0xFFFF, 0x0410);
    M5Cardputer.Display.setCursor(110, 123);
    M5Cardputer.Display.print(vstr);

    M5Cardputer.Display.drawFastVLine(135, 119, 16, 0x2945);

    int  lv  = M5Cardputer.Power.getBatteryLevel();
    int  vol2 = M5Cardputer.Power.getBatteryVoltage();
    bool ch  = (vol2 > 4200);
    uint16_t bc = (lv > 60) ? COLOR_GREEN
                : (lv > 20) ? COLOR_ACCENT3
                :              COLOR_RED;

    int bx = 140, by = 121;
    M5Cardputer.Display.fillRect(bx, by, 30, 11, 0x0410);
    M5Cardputer.Display.drawRect(bx, by, 24, 11, 0x8410);
    M5Cardputer.Display.fillRect(bx + 24, by + 3, 3, 5, 0x8410);

    int fi = (lv * 20) / 100;
    if (fi > 22) fi = 22;
    if (fi > 0)
        M5Cardputer.Display.fillRect(bx + 2, by + 2, fi, 7, bc);

    if (ch) {
        M5Cardputer.Display.setTextColor(COLOR_ACCENT3, 0x0410);
        M5Cardputer.Display.setCursor(bx + 7, by + 1);
        M5Cardputer.Display.print("~");
    }

    char bstr[6];
    sprintf(bstr, "%d%%", lv);
    M5Cardputer.Display.setTextColor(bc, 0x0410);
    M5Cardputer.Display.setCursor(169, 123);
    M5Cardputer.Display.print(bstr);

    M5Cardputer.Display.drawFastVLine(195, 119, 16, 0x2945);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0x528A, 0x0410);
    M5Cardputer.Display.setCursor(198, 123);
    char trk[10];
    sprintf(trk, "#%03d", currentTrack);
    M5Cardputer.Display.print(trk);
}

// ========== ÜST BAR ==========

void drawTopBar() {
    for (int y = 0; y < 18; y++) {
        float t = (float)y / 18.0f;
        M5Cardputer.Display.drawFastHLine(
            0, y, 240,
            M5Cardputer.Display.color565(
                (uint8_t)(20 * t),
                (uint8_t)(10 * t),
                (uint8_t)(40 * (1 - t) + 10 * t)));
    }
    drawRainbowLine(17, 0.8f, rainbowOffset);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, 0x0000);
    M5Cardputer.Display.setCursor(4, 5);
    M5Cardputer.Display.print("MOY");
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, 0x0000);
    M5Cardputer.Display.print(" MUSIC");
    M5Cardputer.Display.setTextColor(0x4208, 0x0000);
    M5Cardputer.Display.print(" v6.0");

    M5Cardputer.Display.fillRoundRect(186, 2, 50, 12, 2, 0x2000);
    M5Cardputer.Display.drawRoundRect(186, 2, 50, 12, 2, COLOR_ACCENT2);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, 0x2000);
    M5Cardputer.Display.setCursor(190, 5);
    M5Cardputer.Display.print("DJ MODE");
}

// ========== ANA EKRAN ==========

void drawMainScreen() {
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawTopBar();
    drawLeftPanel();

    for (int y = 18; y < 118; y++) {
        float t = (float)(y - 18) / 100.0f;
        M5Cardputer.Display.drawPixel(
            136, y,
            hsvToRgb565(fmod(0.75f + t * 0.5f, 1.0f), 1.0f, 0.5f));
    }

    drawFullDisk(diskAngle);
    drawBottomBar();
}

// ========== GEÇİCİ MESAJ ==========

void drawTempMsg() {
    M5Cardputer.Display.fillRoundRect(
        2, 99, 132, 17, 3,
        M5Cardputer.Display.color565(
            ((tempMessageColor >> 11) & 0x1F),
            ((tempMessageColor >> 5)  & 0x3F) >> 2,
            (tempMessageColor & 0x1F)));
    M5Cardputer.Display.drawRoundRect(2, 99, 132, 17, 3, tempMessageColor);
    M5Cardputer.Display.drawRoundRect(
        1, 98, 134, 19, 4,
        M5Cardputer.Display.color565(
            (((tempMessageColor >> 11) & 0x1F) << 2) >> 1,
            (((tempMessageColor >> 5)  & 0x3F) << 1) >> 1,
            ((tempMessageColor & 0x1F) << 2) >> 1));
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(
        0xFFFF,
        M5Cardputer.Display.color565(
            ((tempMessageColor >> 11) & 0x1F),
            ((tempMessageColor >> 5)  & 0x3F) >> 2,
            (tempMessageColor & 0x1F)));
    int tx = (134 - (int)tempMessageText.length() * 6) / 2;
    if (tx < 5) tx = 5;
    M5Cardputer.Display.setCursor(tx, 104);
    M5Cardputer.Display.print(tempMessageText);
}

void clearTempArea() {
    M5Cardputer.Display.fillRect(0, 98, 136, 20, COLOR_BG);
    M5Cardputer.Display.fillRect(0, 99, 136, 19, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 101);
    M5Cardputer.Display.print("SPC");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Play ");
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, COLOR_BG);
    M5Cardputer.Display.print("+/-");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Vol");

    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 110);
    M5Cardputer.Display.print("G");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":GoTo ");
    M5Cardputer.Display.setTextColor(COLOR_GREEN, COLOR_BG);
    M5Cardputer.Display.print("V");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Viz ");
    M5Cardputer.Display.setTextColor(COLOR_ORANGE, COLOR_BG);
    M5Cardputer.Display.print("I");
    M5Cardputer.Display.setTextColor(0x8410, COLOR_BG);
    M5Cardputer.Display.print(":Info");
}

void showTempMsg(String msg, uint16_t col) {
    tempMessageText  = msg;
    tempMessageColor = col;
    tempMessageTime  = millis();
    tempMessageActive = true;
    drawTempMsg();
}

// ========== GÜNCELLEME ==========

void updateTrackDisplay() {
    M5Cardputer.Display.fillRect(0, 36, 136, 28, COLOR_BG);
    char ts[6];
    sprintf(ts, "%03d", currentTrack);
    uint16_t numCol = isPlaying ? COLOR_ACCENT1 : 0x4208;
    M5Cardputer.Display.setTextSize(3);

    if (isPlaying) {
        M5Cardputer.Display.setTextColor(
            M5Cardputer.Display.color565(0, 80, 120), COLOR_BG);
        M5Cardputer.Display.setCursor(5, 39);
        M5Cardputer.Display.print(ts);
        M5Cardputer.Display.setTextColor(
            M5Cardputer.Display.color565(0, 180, 220), COLOR_BG);
        M5Cardputer.Display.setCursor(4, 38);
        M5Cardputer.Display.print(ts);
    }
    M5Cardputer.Display.setTextColor(numCol, COLOR_BG);
    M5Cardputer.Display.setCursor(4, 37);
    M5Cardputer.Display.print(ts);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, COLOR_BG);
    M5Cardputer.Display.setCursor(72, 37);
    M5Cardputer.Display.print("TIME");

    M5Cardputer.Display.fillRect(196, 119, 44, 16, 0x0410);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0x528A, 0x0410);
    M5Cardputer.Display.setCursor(198, 123);
    char trk[10];
    sprintf(trk, "#%03d", currentTrack);
    M5Cardputer.Display.print(trk);

    M5Cardputer.Display.fillRect(80, 19, 55, 16, 0x0821);
    char tot[10];
    sprintf(tot, "/%d", totalTracks);
    M5Cardputer.Display.setTextColor(0x528A, 0x0821);
    M5Cardputer.Display.setCursor(90, 23);
    M5Cardputer.Display.print(tot);
}

void updateStatusDisplay() {
    M5Cardputer.Display.fillRect(0, 65, 136, 14, COLOR_BG);
    if (isPlaying) {
        M5Cardputer.Display.fillRoundRect(
            2, 66, 58, 12, 2,
            M5Cardputer.Display.color565(0, 60, 0));
        M5Cardputer.Display.drawRoundRect(2, 66, 58, 12, 2, COLOR_GREEN);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(
            COLOR_GREEN,
            M5Cardputer.Display.color565(0, 60, 0));
        M5Cardputer.Display.setCursor(7, 69);
        M5Cardputer.Display.print("\x10 PLAYING");
    } else {
        M5Cardputer.Display.fillRoundRect(
            2, 66, 55, 12, 2,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.drawRoundRect(2, 66, 55, 12, 2, COLOR_ACCENT3);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT3,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.setCursor(7, 69);
        M5Cardputer.Display.print("|| PAUSED");
    }
}

void updateModeDisplay() {
    M5Cardputer.Display.fillRect(64, 65, 72, 14, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    if (isLoopEnabled) {
        M5Cardputer.Display.fillRoundRect(
            64, 66, 68, 12, 2,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.drawRoundRect(64, 66, 68, 12, 2, COLOR_ACCENT3);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT3,
            M5Cardputer.Display.color565(60, 60, 0));
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("\x1D LOOP");
    } else if (isShuffleEnabled) {
        M5Cardputer.Display.fillRoundRect(
            64, 66, 68, 12, 2,
            M5Cardputer.Display.color565(0, 0, 60));
        M5Cardputer.Display.drawRoundRect(64, 66, 68, 12, 2, COLOR_ACCENT1);
        M5Cardputer.Display.setTextColor(
            COLOR_ACCENT1,
            M5Cardputer.Display.color565(0, 0, 60));
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("~ SHUF");
    } else {
        M5Cardputer.Display.setTextColor(0x2945, COLOR_BG);
        M5Cardputer.Display.setCursor(68, 69);
        M5Cardputer.Display.print("= SEQ");
    }
}

void updateTimeDisplay() {
    unsigned long el = isPlaying
        ? (millis() - trackStartTime - totalPausedTime) / 1000
        : 0;
    M5Cardputer.Display.fillRect(64, 46, 70, 12, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0xFFFF, COLOR_BG);
    M5Cardputer.Display.setCursor(68, 47);
    M5Cardputer.Display.printf(
        "%02d:%02d", (int)(el / 60), (int)(el % 60));
}

void updateBatteryDisplay() {
    M5Cardputer.Display.fillRect(136, 119, 60, 16, 0x0410);
    M5Cardputer.Display.drawFastVLine(135, 119, 16, 0x2945);

    int  lv  = M5Cardputer.Power.getBatteryLevel();
    int  vol2 = M5Cardputer.Power.getBatteryVoltage();
    bool ch  = (vol2 > 4200);
    uint16_t bc = (lv > 60) ? COLOR_GREEN
                : (lv > 20) ? COLOR_ACCENT3
                :              COLOR_RED;

    int bx = 140, by = 121;
    M5Cardputer.Display.drawRect(bx, by, 24, 11, 0x8410);
    M5Cardputer.Display.fillRect(bx + 24, by + 3, 3, 5, 0x8410);
    int fi = (lv * 20) / 100;
    if (fi > 22) fi = 22;
    if (fi > 0)
        M5Cardputer.Display.fillRect(bx + 2, by + 2, fi, 7, bc);
    if (ch) {
        M5Cardputer.Display.setTextColor(COLOR_ACCENT3, 0x0410);
        M5Cardputer.Display.setCursor(bx + 7, by + 1);
        M5Cardputer.Display.print("~");
    }
    char bstr[6];
    sprintf(bstr, "%d%%", lv);
    M5Cardputer.Display.setTextColor(bc, 0x0410);
    M5Cardputer.Display.setCursor(169, 123);
    M5Cardputer.Display.print(bstr);
}

void updateMainDisplay() {
    unsigned long now = millis();
    rainbowOffset = fmod(rainbowOffset + 0.002f, 1.0f);

    // Disk
    if (now - lastDiskUpdate >= diskUpdateInterval) {
        lastDiskUpdate = now;
        updateDiskRotation();
        bool pChg = (lastDiskPlaying != isPlaying);
        if (diskSpeed > 0.01f || pChg) {
            if (pChg) {
                drawFullDisk(diskAngle);
                lastDiskPlaying = isPlaying;
            } else {
                drawDiskDynamic(diskAngle);
            }
        }
    }

    // VU
    if (now - lastVuUpdate >= vuUpdateInterval) {
        lastVuUpdate = now;
        updateVuMeter();
        drawVuMeter();
    }

    // Zaman
    if (now - lastTimeUpdate >= timeUpdateInterval) {
        lastTimeUpdate = now;
        updateTimeDisplay();
    }

    // Temp mesaj
    if (tempMessageActive && (now - tempMessageTime > tempMessageDuration)) {
        tempMessageActive = false;
        clearTempArea();
    }

    // Batarya
    if (now - lastBatteryUpdate >= batteryUpdateInterval) {
        lastBatteryUpdate = now;
        updateBatteryDisplay();
    }

    // Rainbow
    static unsigned long lastRainbow = 0;
    if (now - lastRainbow > 100) {
        lastRainbow = now;
        drawRainbowLine(17, 0.8f, rainbowOffset);
        drawRainbowLine(118, 0.7f, fmod(rainbowOffset + 0.5f, 1.0f));
    }
}

// ========== INPUT MODE ==========

void drawInputMode() {
    M5Cardputer.Display.fillScreen(COLOR_BG);

    for (int y = 0; y < 135; y += 4) {
        M5Cardputer.Display.drawFastHLine(
            0, y, 240,
            M5Cardputer.Display.color565(0, 0, (uint8_t)(8 * (y % 8 == 0))));
    }

    drawRainbowLine(0, 0.9f);
    drawRainbowLine(1, 0.5f);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, COLOR_BG);
    M5Cardputer.Display.setCursor(68, 6);
    M5Cardputer.Display.print("\x0E GO TO TRACK \x0E");
    drawRainbowLine(15, 0.6f);

    M5Cardputer.Display.fillRoundRect(20, 24, 200, 72, 5, 0x0821);
    M5Cardputer.Display.drawRoundRect(20, 24, 200, 72, 5, COLOR_ACCENT2);
    M5Cardputer.Display.drawRoundRect(
        19, 23, 202, 74, 6,
        M5Cardputer.Display.color565(80, 0, 80));
    M5Cardputer.Display.drawRoundRect(
        21, 25, 198, 70, 4,
        M5Cardputer.Display.color565(40, 0, 40));

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, 0x0821);
    M5Cardputer.Display.setCursor(35, 30);
    M5Cardputer.Display.print("ENTER TRACK NUMBER:");

    M5Cardputer.Display.fillRoundRect(35, 40, 170, 40, 4, COLOR_BG);
    M5Cardputer.Display.drawRoundRect(35, 40, 170, 40, 4, 0x2945);

    M5Cardputer.Display.setTextSize(3);
    if (inputTrackNumber.length() > 0) {
        M5Cardputer.Display.setTextColor(
            M5Cardputer.Display.color565(0, 100, 150), COLOR_BG);
        int tw = inputTrackNumber.length() * 18;
        M5Cardputer.Display.setCursor(120 - tw / 2 + 1, 48);
        M5Cardputer.Display.print(inputTrackNumber);
        M5Cardputer.Display.setTextColor(COLOR_ACCENT1, COLOR_BG);
        M5Cardputer.Display.setCursor(120 - tw / 2, 47);
        M5Cardputer.Display.print(inputTrackNumber);
        M5Cardputer.Display.fillRect(120 - tw / 2 + tw + 2, 47, 3, 20, COLOR_ACCENT1);
    } else {
        M5Cardputer.Display.setTextColor(0x1082, COLOR_BG);
        M5Cardputer.Display.setCursor(82, 47);
        M5Cardputer.Display.print("_ _ _");
    }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0x528A, COLOR_BG);
    M5Cardputer.Display.setCursor(30, 100);
    M5Cardputer.Display.print("[ENTER] Confirm  [DEL] Delete  [G] Exit");

    M5Cardputer.Display.setTextColor(0x2945, COLOR_BG);
    M5Cardputer.Display.setCursor(70, 112);
    M5Cardputer.Display.printf("Library: %d tracks", totalTracks);

    drawRainbowLine(121, 0.6f);
    drawRainbowLine(122, 0.3f);
}

// ========== INFO ==========

void enterInfoScreen() {
    isInfoScreen = true;
    M5Cardputer.Display.fillScreen(COLOR_BG);

    for (int x = 0; x < 240; x += 20) {
        M5Cardputer.Display.drawFastVLine(
            x, 0, 135,
            M5Cardputer.Display.color565(0, 0, 5));
    }

    drawRainbowLine(0, 0.9f);
    drawRainbowLine(1, 0.5f);

    M5Cardputer.Display.fillRoundRect(0, 3, 240, 14, 0, 0x0821);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT3, 0x0821);
    M5Cardputer.Display.setCursor(70, 6);
    M5Cardputer.Display.print("[ ABOUT & KEYBINDS ]");

    drawRainbowLine(17, 0.6f);

    M5Cardputer.Display.fillRect(0, 18, 240, 26, 0x0410);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(COLOR_ACCENT2, 0x0410);
    M5Cardputer.Display.setCursor(6, 21);
    M5Cardputer.Display.print("MOY");
    M5Cardputer.Display.setTextColor(COLOR_ACCENT1, 0x0410);
    M5Cardputer.Display.print(" MUSIC");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COLOR_GREEN, 0x0410);
    M5Cardputer.Display.setCursor(174, 21);
    M5Cardputer.Display.print("DJ v6.0");
    M5Cardputer.Display.setTextColor(0x528A, 0x0410);
    M5Cardputer.Display.setCursor(6, 33);
    M5Cardputer.Display.print("by Andy + AI  |  M5Stack Cardputer");
    M5Cardputer.Display.drawFastHLine(0, 44, 240, 0x2945);

    struct KeyInfo {
        const char* key;
        const char* desc;
        uint16_t    kc;
    };
    KeyInfo keys[] = {
        {"SPACE",   "Play / Pause",           COLOR_GREEN},
        {"BtnA",    "1x=Pause 2x=>> 3x=<<",   COLOR_ACCENT1},
        {"+  / -",  "Volume Up / Down",        COLOR_ACCENT3},
        {"R",       "Random Track",            COLOR_ORANGE},
        {"G",       "Go To Track #",           COLOR_ACCENT2},
        {"L",       "Loop Toggle",             COLOR_ACCENT3},
        {"S",       "Shuffle Toggle",          COLOR_ACCENT1},
        {"V",       "Visualizer",              COLOR_GREEN},
        {"I / `",   "Info / Exit",             COLOR_PINK},
        {",  /  /", "Prev / Next Track",       COLOR_ACCENT1},
        {".  / ;",  "-10 / +10 Tracks",        COLOR_ORANGE},
    };

    int ky = 47;
    for (auto& k : keys) {
        M5Cardputer.Display.fillRoundRect(4, ky, 32, 8, 1, 0x0821);
        M5Cardputer.Display.drawRoundRect(4, ky, 32, 8, 1, k.kc);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(k.kc, 0x0821);
        M5Cardputer.Display.setCursor(6, ky + 1);
        M5Cardputer.Display.print(k.key);
        M5Cardputer.Display.setTextColor(0xBDF7, COLOR_BG);
        M5Cardputer.Display.setCursor(40, ky + 1);
        M5Cardputer.Display.print(k.desc);
        ky += 10;
    }

    drawRainbowLine(133, 0.5f);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0x4208, COLOR_BG);
    M5Cardputer.Display.setCursor(60, 125);
    M5Cardputer.Display.print("[ Press I or ` to exit ]");
}

void exitInfoScreen() {
    isInfoScreen    = false;
    lastDiskPlaying = !isPlaying;
    drawMainScreen();
}

// ========== FULLSCREEN VIZ ==========

void initMatrixColumns() {
    for (int i = 0; i < 40; i++) {
        matrixColumns[i].y            = random(-50, 0);
        matrixColumns[i].speed        = random(1, 4);
        matrixColumns[i].character    = matrixChars[random(0, strlen(matrixChars))];
        matrixColumns[i].trailLength  = random(5, 15);
    }
}

void drawVisualizerHeader() {
    M5Cardputer.Display.fillRect(0, 0, 240, 17, 0x0410);
    drawRainbowLine(0, 0.7f, rainbowOffset);
    M5Cardputer.Display.fillRect(0, 1, 240, 16, 0x0410);

    const char* mn = "";
    uint16_t    mc = COLOR_ACCENT1;
    if      (visualizerMode == 1) { mn = "SPECTRUM BARS"; mc = COLOR_ACCENT3; }
    else if (visualizerMode == 2) { mn = "WAVE FORM";     mc = COLOR_GREEN;   }
    else if (visualizerMode == 3) { mn = "MATRIX RAIN";   mc = COLOR_GREEN;   }

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(mc, 0x0410);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print(mn);

    M5Cardputer.Display.setTextColor(0x4208, 0x0410);
    M5Cardputer.Display.setCursor(120, 5);
    M5Cardputer.Display.print("V:Next  ESC:Exit  +/-:Vol");
}

void updateFullscreenVisualizer() {
    if (visualizerMode == 1) {
        if (!isPlaying) {
            for (int i = 0; i < 20; i++) fullscreenBarHeights[i] = 0;
            return;
        }
        for (int i = 0; i < 20; i++) {
            float c  = 9.5f;
            float d  = abs(i - c) / c;
            int   mH = (int)(106 * (1.0f - d * 0.3f));
            fullscreenBarHeights[i] = random(mH / 3, mH + 5);
        }
    }
}

void drawFullscreenBars() {
    int bW = 11, bG = 1, maxH = 110, baseY = 127;
    for (int i = 0; i < 20; i++) {
        int x = i * (bW + bG);
        M5Cardputer.Display.fillRect(x, 17, bW, maxH, COLOR_BG);
        if (fullscreenBarHeights[i] > 0) {
            for (int y = 0; y < fullscreenBarHeights[i]; y++) {
                float t = (float)y / maxH;
                M5Cardputer.Display.drawFastHLine(
                    x, baseY - y, bW,
                    hsvToRgb565(t * 0.4f, 1.0f, 0.95f));
            }
            M5Cardputer.Display.drawFastHLine(
                x, baseY - fullscreenBarHeights[i], bW, 0xFFFF);
        }
    }
}

void drawFullscreenWaves() {
    M5Cardputer.Display.fillRect(0, 17, 240, 111, COLOR_BG);
    wavePhase += 0.12f;
    if (wavePhase > 6.28f) wavePhase = 0.0f;
    float amp  = isPlaying ? random(25, 50) : 6;
    int   cy   = 72;
    float amp2 = amp * 0.6f;

    for (int x = 0; x < 239; x++) {
        int y1 = cy + (int)(amp  * sinf(x       * 0.04f + wavePhase));
        int y2 = cy + (int)(amp  * sinf((x + 1) * 0.04f + wavePhase));
        int y3 = cy + (int)(amp2 * sinf(x       * 0.07f - wavePhase * 1.3f));
        int y4 = cy + (int)(amp2 * sinf((x + 1) * 0.07f - wavePhase * 1.3f));
        y1 = constrain(y1, 17, 127); y2 = constrain(y2, 17, 127);
        y3 = constrain(y3, 17, 127); y4 = constrain(y4, 17, 127);

        float hue = (float)x / 240.0f;
        M5Cardputer.Display.drawLine(x, y1, x + 1, y2,
            hsvToRgb565(hue, 1.0f, 0.95f));
        M5Cardputer.Display.drawLine(x, y3, x + 1, y4,
            hsvToRgb565(fmod(hue + 0.5f, 1.0f), 1.0f, 0.6f));

        int mn = min(y1, cy), mx = max(y1, cy);
        if (mx > mn)
            M5Cardputer.Display.drawFastVLine(x, mn, mx - mn,
                hsvToRgb565(hue, 1.0f, 0.25f));
    }
}

void drawMatrixRain() {
    M5Cardputer.Display.fillRect(0, 17, 240, 111, COLOR_BG);
    M5Cardputer.Display.setTextSize(1);
    int sm = isPlaying ? 2 : 1;

    for (int i = 0; i < 40; i++) {
        matrixColumns[i].y += matrixColumns[i].speed * sm;
        if (matrixColumns[i].y > 135 + matrixColumns[i].trailLength * 8) {
            matrixColumns[i].y           = random(-60, -5);
            matrixColumns[i].speed       = random(1, 4);
            matrixColumns[i].trailLength = random(5, 15);
            matrixColumns[i].character   =
                matrixChars[random(0, strlen(matrixChars))];
        }
        int x = i * 6, y = matrixColumns[i].y;
        if (y >= 17 && y < 128) {
            M5Cardputer.Display.setTextColor(
                M5Cardputer.Display.color565(220, 255, 220), COLOR_BG);
            M5Cardputer.Display.setCursor(x, y);
            M5Cardputer.Display.print(matrixColumns[i].character);

            for (int j = 1; j < matrixColumns[i].trailLength; j++) {
                int ty = y - j * 8;
                if (ty >= 17 && ty < 128) {
                    int br = 200 - (j * 200 / matrixColumns[i].trailLength);
                    M5Cardputer.Display.setTextColor(
                        M5Cardputer.Display.color565(0, br, 0), COLOR_BG);
                    M5Cardputer.Display.setCursor(x, ty);
                    M5Cardputer.Display.print(
                        matrixChars[random(0, strlen(matrixChars))]);
                }
            }
        }
    }
}

void drawFullscreenTempMessage(String msg, uint16_t col) {
    tempMessageText   = msg;
    tempMessageColor  = col;
    tempMessageTime   = millis();
    tempMessageActive = (msg.length() > 0);
    M5Cardputer.Display.fillRect(0, 128, 240, 7, COLOR_BG);
    if (msg.length() > 0) {
        int x = (240 - (int)msg.length() * 6) / 2;
        if (x < 0) x = 0;
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(col, COLOR_BG);
        M5Cardputer.Display.setCursor(x, 129);
        M5Cardputer.Display.print(msg);
    }
}

void enterFullscreenVisualizer() {
    isFullscreenVisualizer = true;
    visualizerMode         = 1;
    M5Cardputer.Display.fillScreen(COLOR_BG);
    initMatrixColumns();
    drawVisualizerHeader();
}

void switchVisualizerMode() {
    visualizerMode++;
    if (visualizerMode > 3) { exitFullscreenVisualizer(); return; }
    M5Cardputer.Display.fillScreen(COLOR_BG);
    drawVisualizerHeader();
}

void exitFullscreenVisualizer() {
    isFullscreenVisualizer = false;
    visualizerMode         = 0;
    lastDiskPlaying        = !isPlaying;
    drawMainScreen();
}

// ========== KONTROL ==========

void volumeUp() {
    if (currentVolume < 30) {
        currentVolume++;
        audioplayer.setVolume(currentVolume);
        char m[20];
        sprintf(m, "VOL %d%%", (currentVolume * 100) / 30);
        if (isFullscreenVisualizer) drawFullscreenTempMessage(m, COLOR_GREEN);
        else { drawBottomBar(); showTempMsg(m, COLOR_GREEN); }
    }
}

void volumeDown() {
    if (currentVolume > 0) {
        currentVolume--;
        audioplayer.setVolume(currentVolume);
        char m[20];
        sprintf(m, "VOL %d%%", (currentVolume * 100) / 30);
        if (isFullscreenVisualizer) drawFullscreenTempMessage(m, COLOR_ACCENT3);
        else { drawBottomBar(); showTempMsg(m, COLOR_ACCENT3); }
    }
}

void toggleLoop() {
    isLoopEnabled = !isLoopEnabled;
    if (isLoopEnabled && isShuffleEnabled) isShuffleEnabled = false;
    String   msg = isLoopEnabled ? "LOOP ON" : "LOOP OFF";
    uint16_t c   = isLoopEnabled ? COLOR_ACCENT3 : 0x8410;
    if (isFullscreenVisualizer) drawFullscreenTempMessage(msg, c);
    else { updateModeDisplay(); showTempMsg(msg, c); }
}

void toggleShuffle() {
    isShuffleEnabled = !isShuffleEnabled;
    if (isShuffleEnabled && isLoopEnabled) isLoopEnabled = false;
    String   msg = isShuffleEnabled ? "SHUFFLE ON" : "SHUFFLE OFF";
    uint16_t c   = isShuffleEnabled ? COLOR_ACCENT1 : 0x8410;
    if (isFullscreenVisualizer) drawFullscreenTempMessage(msg, c);
    else { updateModeDisplay(); showTempMsg(msg, c); }
}

void goToTrack(int n) {
    if (n < 1 || n > (int)totalTracks) {
        char m[25];
        sprintf(m, "BAD! (1-%d)", totalTracks);
        if (isFullscreenVisualizer) drawFullscreenTempMessage(m, COLOR_RED);
        else showTempMsg(m, COLOR_RED);
        return;
    }
    audioplayer.selectAudioNum(n); delay(100);
    audioplayer.playAudio();
    isPlaying      = true;
    trackStartTime = millis();
    totalPausedTime = 0;
    currentTrack    = n;
    lastDisplayedTrack = n;
    char m[15];
    sprintf(m, "GOTO %d", n);
    if (isFullscreenVisualizer) drawFullscreenTempMessage(m, COLOR_ACCENT1);
    else { updateTrackDisplay(); updateStatusDisplay(); showTempMsg(m, COLOR_ACCENT1); }
}

void randomTrack() {
    if (totalTracks == 0) return;
    uint16_t rnd = random(1, totalTracks + 1);
    audioplayer.selectAudioNum(rnd);
    currentTrack       = rnd;
    lastDisplayedTrack = rnd;
    trackStartTime     = millis();
    totalPausedTime    = 0;
    if (lastPlayStatus == AUDIO_PLAYER_STATUS_PLAYING) {
        audioplayer.playAudio();
        isPlaying = true;
    }
    char m[15];
    sprintf(m, "RND %d!", rnd);
    if (isFullscreenVisualizer) drawFullscreenTempMessage(m, COLOR_ACCENT2);
    else { updateTrackDisplay(); showTempMsg(m, COLOR_ACCENT2); }
}

void executeAction(int clicks) {
    if (clicks == 1) {
        uint8_t st = audioplayer.checkPlayStatus();
        if (st == AUDIO_PLAYER_STATUS_PLAYING) {
            audioplayer.pauseAudio();
            pausedTime = millis();
            isPlaying  = false;
            if (isFullscreenVisualizer)
                drawFullscreenTempMessage("PAUSED", COLOR_ACCENT3);
            else { updateStatusDisplay(); showTempMsg("PAUSED", COLOR_ACCENT3); }
        } else {
            audioplayer.playAudio();
            if (st == AUDIO_PLAYER_STATUS_STOPPED) {
                trackStartTime  = millis();
                totalPausedTime = 0;
            } else {
                totalPausedTime += (millis() - pausedTime);
            }
            isPlaying = true;
            if (isFullscreenVisualizer)
                drawFullscreenTempMessage("PLAYING", COLOR_GREEN);
            else { updateStatusDisplay(); showTempMsg("PLAYING", COLOR_GREEN); }
        }
    } else if (clicks == 2) {
        audioplayer.nextAudio(); delay(100); audioplayer.playAudio();
        isPlaying       = true;
        trackStartTime  = millis();
        totalPausedTime = 0;
        if (isFullscreenVisualizer)
            drawFullscreenTempMessage("NEXT >>", COLOR_ACCENT1);
        else { updateStatusDisplay(); showTempMsg("NEXT >>", COLOR_ACCENT1); }
    } else if (clicks == 3) {
        audioplayer.previousAudio(); delay(100); audioplayer.playAudio();
        isPlaying       = true;
        trackStartTime  = millis();
        totalPausedTime = 0;
        if (isFullscreenVisualizer)
            drawFullscreenTempMessage("<< PREV", COLOR_ACCENT1);
        else { updateStatusDisplay(); showTempMsg("<< PREV", COLOR_ACCENT1); }
    }
}

void checkAutoNext() {
    static unsigned long lc = 0;
    if (millis() - lc < 500) return;
    lc = millis();
    uint8_t cs = audioplayer.checkPlayStatus();

    if (lastPlayStatus == AUDIO_PLAYER_STATUS_PLAYING &&
        cs == AUDIO_PLAYER_STATUS_STOPPED) {
        if (isLoopEnabled) {
            delay(100); audioplayer.playAudio(); delay(100);
            trackStartTime  = millis();
            totalPausedTime = 0;
            isPlaying       = true;
            if (!isFullscreenVisualizer) showTempMsg("LOOP", COLOR_ACCENT3);
            else drawFullscreenTempMessage("LOOP", COLOR_ACCENT3);
        } else if (isShuffleEnabled) {
            uint16_t r = random(1, totalTracks + 1);
            audioplayer.selectAudioNum(r); delay(200);
            audioplayer.playAudio(); delay(100);
            trackStartTime  = millis();
            totalPausedTime = 0;
            isPlaying       = true;
            if (!isFullscreenVisualizer) showTempMsg("SHUFFLE", COLOR_ACCENT1);
            else drawFullscreenTempMessage("SHUFFLE", COLOR_ACCENT1);
        } else {
            audioplayer.nextAudio(); delay(200);
            audioplayer.playAudio(); delay(100);
            trackStartTime  = millis();
            totalPausedTime = 0;
            isPlaying       = true;
            if (!isFullscreenVisualizer) showTempMsg("AUTO NEXT", 0xFFFF);
            else drawFullscreenTempMessage("AUTO NEXT", 0xFFFF);
        }
    }

    uint16_t rt = audioplayer.getCurrentAudioNumber();
    if (rt < 1 || rt > totalTracks) return;

    if (rt != lastDisplayedTrack) {
        currentTrack       = rt;
        lastDisplayedTrack = rt;
        trackStartTime     = millis();
        totalPausedTime    = 0;
        if (!isFullscreenVisualizer && !isInfoScreen) updateTrackDisplay();
    }

    if (lastPlayStatus != cs) {
        if (cs == AUDIO_PLAYER_STATUS_PLAYING) {
            if (lastPlayStatus == AUDIO_PLAYER_STATUS_PAUSED)
                totalPausedTime += (millis() - pausedTime);
            isPlaying = true;
        } else if (cs == AUDIO_PLAYER_STATUS_PAUSED) {
            pausedTime = millis();
            isPlaying  = false;
        } else {
            isPlaying = false;
        }
        if (!isFullscreenVisualizer && !isInfoScreen) updateStatusDisplay();
    }
    lastPlayStatus = cs;
}

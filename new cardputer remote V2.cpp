#include "Cardputer_Remote.h"
#include "MyOS.h"
#include "MainMenuV2.h"

// ================================================================
// BEGIN / LOOP / DRAW / EXIT
// ================================================================

void Cardputer_Remote::Begin()
{
    showTopBar = false;

    if (!spriteCreated)
    {
        mainOS->sprite.createSprite(SCREEN_W, SCREEN_H);
        spriteCreated = true;
    }

    // FIX: IRsend pointer ile başlat
    createIRSend(sendPin);

    irRecv.enableIRIn();

    sdAvailable = false;
    initSD();

    if (sdAvailable)
        scanSDFiles();

    animTimer   = millis();
    needsRedraw = true;
    drawScreen();
}

void Cardputer_Remote::Loop()
{
    if (mainOS->NewKey.ifKeyJustPress('`'))
    {
        if (currentState == STATE_MAIN_MENU)
        {
            mainOS->ChangeMenu(new MainMenuV2(mainOS));
            return;
        }
    }

    handleIRReceive();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())
    {
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        char key = 0;

        if (!status.word.empty())
            key = status.word[0];

        if (!status.hid_keys.empty())
        {
            switch (status.hid_keys[0])
            {
            case 0x28: key = '\n'; break;
            case 0x2A: key = 127;  break;
            case 0x29: key = '`';  break;
            case 0x4F: key = '/';  break;
            case 0x50: key = ',';  break;
            case 0x51: key = '.';  break;
            case 0x52: key = ';';  break;
            default:   break;
            }
        }

        if (key != 0)
        {
            switch (currentState)
            {
            case STATE_MAIN_MENU:    handleMainMenu(key);    break;
            case STATE_CAPTURE:      handleCapture(key);     break;
            case STATE_SAVE_PROMPT:  handleSavePrompt(key);  break;
            case STATE_CAPTURE_SAVE: handleSaveDialog(key);  break;
            case STATE_VIEW_CODE:    handleViewCode(key);    break;
            case STATE_SD_BROWSER:   handleSDBrowser(key);   break;
            case STATE_SEND_CONFIRM: handleSendConfirm(key); break;
            case STATE_SETTINGS:     handleSettings(key);    break;
            case STATE_REMOTE_PAD:   handleRemotePad(key);   break;
            }
            needsRedraw = true;
        }
    }

    if (currentState == STATE_MAIN_MENU ||
        (currentState == STATE_CAPTURE && !signalCaptured) ||
        currentState == STATE_CAPTURE_SAVE)
        needsRedraw = true;

    if (statusMsg.length() > 0 && millis() - statusTime < 2500)
        needsRedraw = true;

    if (needsRedraw)
    {
        drawScreen();
        needsRedraw = false;
    }

    delay(16);
}

void Cardputer_Remote::Draw()   {}

void Cardputer_Remote::OnExit()
{
    capturing      = false;
    signalCaptured = false;
    irRecv.pause();

    // FIX: IRsend pointer temizle
    if (irSendPtr)
    {
        delete irSendPtr;
        irSendPtr = nullptr;
    }
}

// ================================================================
// IR SEND POINTER YÖNETİMİ
// ================================================================

// FIX: Yeni bir IRsend nesnesi oluşturur (pointer)
void Cardputer_Remote::createIRSend(uint8_t pin)
{
    if (irSendPtr)
    {
        delete irSendPtr;
        irSendPtr = nullptr;
    }
    irSendPtr = new IRsend(pin);
    if (irSendPtr)
    {
        irSendPtr->begin();
        delay(30);
    }
}

// FIX: Mevcut pin ile yeniden başlat
void Cardputer_Remote::reinitIRSend()
{
    createIRSend(sendPin);
}

// ================================================================
// YARDIMCI - STATİK
// ================================================================

String Cardputer_Remote::trimStr(String s)
{
    while (s.length() > 0 &&
           (s[0] == ' ' || s[0] == '\r' || s[0] == '\t' || s[0] == '\n'))
        s = s.substring(1);
    while (s.length() > 0)
    {
        char c = s[s.length() - 1];
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n')
            s = s.substring(0, s.length() - 1);
        else
            break;
    }
    return s;
}

uint32_t Cardputer_Remote::parseHexBytes(String s)
{
    s.replace(" ", "");
    s.toUpperCase();
    uint32_t val = 0;
    int len = s.length();
    for (int byteIdx = 0; byteIdx < 4 && (byteIdx * 2 + 1) < len; byteIdx++)
    {
        char hi = s[byteIdx * 2];
        char lo = s[byteIdx * 2 + 1];
        uint8_t bval = 0;
        if      (hi >= '0' && hi <= '9') bval = (hi - '0') << 4;
        else if (hi >= 'A' && hi <= 'F') bval = (hi - 'A' + 10) << 4;
        if      (lo >= '0' && lo <= '9') bval |= (lo - '0');
        else if (lo >= 'A' && lo <= 'F') bval |= (lo - 'A' + 10);
        val |= ((uint32_t)bval << (byteIdx * 8));
    }
    return val;
}

void Cardputer_Remote::setStatus(String msg)
{
    statusMsg   = msg;
    statusTime  = millis();
    needsRedraw = true;
}

String Cardputer_Remote::protoName(decode_type_t p)
{
    switch (p)
    {
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
    default:         return "UNKNOWN";
    }
}

decode_type_t Cardputer_Remote::strToProto(String s)
{
    s.trim();
    s.toUpperCase();
    if (s == "NEC")        return NEC;
    if (s == "SONY")       return SONY;
    if (s == "RC5")        return RC5;
    if (s == "RC6")        return RC6;
    if (s == "SAMSUNG")    return SAMSUNG;
    if (s == "LG")         return LG;
    if (s == "PANASONIC")  return PANASONIC;
    if (s == "JVC")        return JVC;
    if (s == "SHARP")      return SHARP;
    if (s == "DENON")      return DENON;
    if (s == "PIONEER")    return PIONEER;
    if (s == "MITSUBISHI") return MITSUBISHI;
    if (s == "SAMSUNG36")  return SAMSUNG;
    if (s == "NEC1")       return NEC;
    if (s == "NEC2")       return NEC;
    return UNKNOWN;
}

// ================================================================
// SD KART
// ================================================================

bool Cardputer_Remote::initSD()
{
    sdAvailable = mainOS->haveSDcard;
    if (sdAvailable)
    {
        if (!SD.exists("/IR_Signals"))
            SD.mkdir("/IR_Signals");
    }
    return sdAvailable;
}

bool Cardputer_Remote::saveSignalToSD(IRSignal &sig)
{
    if (!sdAvailable) return false;
    if (sig.name.length() == 0) return false;

    String fn = "/IR_Signals/" + sig.name;
    fn.replace(" ", "_");
    if (!fn.endsWith(".ir")) fn += ".ir";
    sig.filename = fn;

    if (SD.exists(fn)) SD.remove(fn);

    File f = SD.open(fn, FILE_WRITE);
    if (!f) return false;

    f.println("Filetype: IR signals file");
    f.println("Version: 1");

    for (size_t ci = 0; ci < sig.commands.size(); ci++)
    {
        IRCommand &cmd = sig.commands[ci];
        f.println("#");
        f.print("name: ");
        f.println(cmd.label.length() > 0 ? cmd.label : sig.name);
        f.print("type: ");
        f.println(cmd.type);

        if (cmd.type == "parsed")
        {
            f.print("protocol: ");
            f.println(cmd.protocol);

            char buf[36];
            sprintf(buf, "%02X %02X %02X %02X",
                    (uint8_t)(cmd.address & 0xFF),
                    (uint8_t)((cmd.address >> 8)  & 0xFF),
                    (uint8_t)((cmd.address >> 16) & 0xFF),
                    (uint8_t)((cmd.address >> 24) & 0xFF));
            f.print("address: "); f.println(buf);

            sprintf(buf, "%02X %02X %02X %02X",
                    (uint8_t)(cmd.command & 0xFF),
                    (uint8_t)((cmd.command >> 8)  & 0xFF),
                    (uint8_t)((cmd.command >> 16) & 0xFF),
                    (uint8_t)((cmd.command >> 24) & 0xFF));
            f.print("command: "); f.println(buf);
        }
        else
        {
            f.print("frequency: ");  f.println(cmd.frequency);
            f.print("duty_cycle: "); f.println(cmd.dutyCycle, 6);
            f.print("data:");
            for (size_t di = 0; di < cmd.rawData.size(); di++)
            {
                f.print(" ");
                f.print(cmd.rawData[di]);
            }
            f.println();
        }
    }

    f.close();
    return true;
}

bool Cardputer_Remote::loadSignalFromSD(const String &filename, IRSignal &sig)
{
    if (!sdAvailable) return false;

    File f = SD.open(filename, FILE_READ);
    if (!f) return false;

    sig.clear();
    sig.filename = filename;

    String base = filename;
    int ls = base.lastIndexOf('/');
    if (ls >= 0) base = base.substring(ls + 1);
    if (base.length() > 3 &&
        (base.endsWith(".ir") || base.endsWith(".IR")))
        base = base.substring(0, base.length() - 3);
    base.replace("_", " ");
    sig.name = base;

    IRCommand currentCmd;
    bool inBlock = false;

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line = trimStr(line);
        if (line.length() == 0) continue;

        if (line.startsWith("Filetype:")) continue;
        if (line.startsWith("Version:"))  continue;
        if (line.startsWith("# "))        continue;

        if (line == "#")
        {
            if (inBlock && currentCmd.label.length() > 0)
                sig.commands.push_back(currentCmd);

            currentCmd             = IRCommand();
            currentCmd.frequency   = 38000;
            currentCmd.dutyCycle   = 0.33f;
            inBlock = true;
            continue;
        }

        int colon = line.indexOf(':');
        if (colon < 0) continue;

        String key = trimStr(line.substring(0, colon));
        String val = trimStr(line.substring(colon + 1));
        key.toLowerCase();

        if (key == "name")
        {
            currentCmd.label = val;
        }
        else if (key == "type")
        {
            val.toLowerCase();
            currentCmd.type = val;
        }
        else if (key == "protocol")
        {
            currentCmd.protocol = val;
            currentCmd.protocol.trim();
        }
        else if (key == "address")
        {
            currentCmd.address = parseHexBytes(val);
        }
        else if (key == "command")
        {
            currentCmd.command = parseHexBytes(val);
        }
        else if (key == "frequency")
        {
            currentCmd.frequency = (uint32_t)val.toInt();
            if (currentCmd.frequency == 0) currentCmd.frequency = 38000;
        }
        else if (key == "duty_cycle")
        {
            currentCmd.dutyCycle = val.toFloat();
            if (currentCmd.dutyCycle <= 0.0f) currentCmd.dutyCycle = 0.33f;
        }
        else if (key == "data")
        {
            currentCmd.rawData.clear();
            int start = 0;
            int dlen  = (int)val.length();
            int count = 0;
            while (start < dlen && count < MAX_RAW_LEN)
            {
                while (start < dlen && val[start] == ' ') start++;
                if (start >= dlen) break;

                int sp = val.indexOf(' ', start);
                String token;
                if (sp < 0) { token = val.substring(start); start = dlen; }
                else         { token = val.substring(start, sp); start = sp + 1; }

                token.trim();
                if (token.length() > 0)
                {
                    uint16_t v = (uint16_t)token.toInt();
                    if (v > 0) { currentCmd.rawData.push_back(v); count++; }
                }
            }
        }
    }

    if (inBlock && currentCmd.label.length() > 0)
        sig.commands.push_back(currentCmd);

    f.close();

    if (sig.commands.empty()) return false;

    sig.isMulti = (sig.commands.size() > 1);

    IRCommand &first = sig.commands[0];
    sig.protocol  = first.protocol;
    sig.address   = first.address;
    sig.command   = first.command;
    sig.frequency = first.frequency;
    sig.isRaw     = (first.type == "raw");
    sig.rawData   = first.rawData;

    return true;
}

void Cardputer_Remote::scanSDRecursive(const String &dirPath, int depth)
{
    if (depth > 4) return;
    if ((int)sdFiles.size() >= 200) return;

    File dir = SD.open(dirPath.length() > 0 ? dirPath : "/");
    if (!dir) return;
    if (!dir.isDirectory()) { dir.close(); return; }

    while (true)
    {
        if ((int)sdFiles.size() >= 200) break;

        File entry = dir.openNextFile();
        if (!entry) break;

        String entryName = String(entry.name());
        if (entryName.startsWith(".")) { entry.close(); continue; }

        String fullPath;
        if (dirPath.length() == 0 || dirPath == "/")
            fullPath = "/" + entryName;
        else
            fullPath = dirPath + "/" + entryName;

        while (fullPath.indexOf("//") >= 0)
            fullPath.replace("//", "/");

        if (entry.isDirectory())
        {
            entry.close();
            scanSDRecursive(fullPath, depth + 1);
        }
        else
        {
            String lower = entryName;
            lower.toLowerCase();

            if (lower.endsWith(".ir"))
            {
                SDFileInfo info;
                info.path    = fullPath;
                info.isMulti = false;
                info.cmdCount= 1;

                String dn = entryName;
                if (dn.length() > 3) dn = dn.substring(0, dn.length() - 3);
                dn.replace("_", " ");

                if (dirPath.length() > 1)
                {
                    int lsi = dirPath.lastIndexOf('/');
                    String folder = (lsi >= 0 && lsi < (int)dirPath.length() - 1)
                                    ? dirPath.substring(lsi + 1)
                                    : dirPath;
                    if (folder.length() > 0 && folder != "/")
                        info.displayName = "[" + folder + "] " + dn;
                    else
                        info.displayName = dn;
                }
                else
                {
                    info.displayName = dn;
                }

                File peek = SD.open(fullPath, FILE_READ);
                if (peek)
                {
                    int hashCount = 0;
                    while (peek.available())
                    {
                        String ln = peek.readStringUntil('\n');
                        ln.trim();
                        if (ln == "#") hashCount++;
                    }
                    peek.close();
                    info.cmdCount = hashCount;
                    info.isMulti  = (hashCount > 1);
                }

                sdFiles.push_back(info);
            }
            entry.close();
        }
    }

    dir.close();
}

void Cardputer_Remote::scanSDFiles()
{
    sdFiles.clear();
    if (!sdAvailable) return;
    scanSDRecursive("/", 0);
}

bool Cardputer_Remote::deleteFromSD(const String &fn)
{
    if (!sdAvailable) return false;
    return SD.remove(fn);
}

// ================================================================
// IR GÖNDER  — FIX: Pointer kontrolü, doğru kHz hesabı
// ================================================================

bool Cardputer_Remote::sendIRCommand(IRCommand &cmd)
{
    // FIX: Pointer null kontrolü
    if (!irSendPtr)
    {
        createIRSend(sendPin);
        if (!irSendPtr) return false;
    }

    if (cmd.type == "raw")
    {
        if (cmd.rawData.empty()) return false;

        // FIX: IRremoteESP8266 sendRaw() frekansı kHz olarak ister
        // cmd.frequency Hz cinsinden gelir (örn: 38000)
        uint16_t freqKHz = (cmd.frequency >= 1000)
                           ? (uint16_t)(cmd.frequency / 1000)
                           : (uint16_t)cmd.frequency;
        if (freqKHz == 0) freqKHz = 38;

        irSendPtr->sendRaw(cmd.rawData.data(),
                           (uint16_t)cmd.rawData.size(),
                           freqKHz);
        return true;
    }

    // parsed — protokol eşleştir
    decode_type_t proto = strToProto(cmd.protocol);

    // FIX: Flipper format little-endian: address[0] = LSB
    uint8_t addrByte = (uint8_t)(cmd.address & 0xFF);
    uint8_t cmdByte  = (uint8_t)(cmd.command  & 0xFF);

    switch (proto)
    {
    case NEC:
    {
        // NEC 32-bit: addr(8) + ~addr(8) + cmd(8) + ~cmd(8)  [MSB first]
        uint32_t val = ((uint32_t)addrByte         << 24) |
                       ((uint32_t)(~addrByte & 0xFF) << 16) |
                       ((uint32_t)cmdByte           <<  8) |
                       ((uint32_t)(~cmdByte  & 0xFF));
        irSendPtr->sendNEC(val, 32);
        return true;
    }
    case SAMSUNG:
    {
        // Samsung 32-bit: addr(8) + addr(8) + cmd(8) + ~cmd(8)
        uint32_t val = ((uint32_t)addrByte         << 24) |
                       ((uint32_t)addrByte          << 16) |
                       ((uint32_t)cmdByte           <<  8) |
                       ((uint32_t)(~cmdByte & 0xFF));
        irSendPtr->sendSAMSUNG(val, 32);
        return true;
    }
    case SONY:
        // Sony 12-bit: cmd[6:0] + addr[4:0]
        irSendPtr->sendSony(((uint32_t)(cmdByte  & 0x7F)) |
                            (((uint32_t)(addrByte & 0x1F)) << 7), 12);
        return true;

    case RC5:
        irSendPtr->sendRC5(cmdByte, 12);
        return true;

    case RC6:
        irSendPtr->sendRC6(cmdByte, 20);
        return true;

    case LG:
    {
        uint32_t val = ((uint32_t)addrByte << 16) |
                       ((uint32_t)cmdByte  <<  8) |
                       (uint32_t)(~cmdByte & 0xFF);
        irSendPtr->sendLG(val, 28);
        return true;
    }
    case PANASONIC:
        irSendPtr->sendPanasonic(addrByte, cmdByte);
        return true;

    case JVC:
        irSendPtr->sendJVC(((uint32_t)addrByte << 8) | cmdByte, 16, 0);
        return true;

    case SHARP:
        irSendPtr->sendSharpRaw(((uint32_t)addrByte << 8) | cmdByte, 15);
        return true;

    case DENON:
        irSendPtr->sendDenon(((uint32_t)addrByte << 8) | cmdByte, 15);
        return true;

    case PIONEER:
        // Pioneer NEC tabanlı
        irSendPtr->sendNEC(((uint32_t)addrByte << 8) | cmdByte, 32);
        return true;

    default:
        // UNKNOWN: raw data varsa kullan
        if (!cmd.rawData.empty())
        {
            irSendPtr->sendRaw(cmd.rawData.data(),
                               (uint16_t)cmd.rawData.size(), 38);
            return true;
        }
        // Hiçbir şey yoksa NEC olarak dene
        {
            uint32_t val = ((uint32_t)addrByte         << 24) |
                           ((uint32_t)(~addrByte & 0xFF) << 16) |
                           ((uint32_t)cmdByte           <<  8) |
                           ((uint32_t)(~cmdByte  & 0xFF));
            irSendPtr->sendNEC(val, 32);
            return true;
        }
    }
}

bool Cardputer_Remote::sendIR(IRSignal &sig)
{
    if (sig.commands.empty()) return false;
    return sendIRCommand(sig.commands[0]);
}

// ================================================================
// DRAW HELPERS
// ================================================================

void Cardputer_Remote::drawTopBar(const String &title)
{
    mainOS->sprite.fillRect(0, 0, SCREEN_W, 22, 0x111111);
    mainOS->sprite.drawFastHLine(0, 22, SCREEN_W, 0x333333);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);
    mainOS->sprite.setTextColor(0x4488FF);
    mainOS->sprite.setCursor(4, 7);
    mainOS->sprite.print("IR");
    mainOS->sprite.setTextColor(COLOR_TEXT);

    int tlen = title.length();
    int tx   = (SCREEN_W - tlen * 6) / 2;
    if (tx < 20) tx = 20;
    mainOS->sprite.setCursor(tx, 7);
    mainOS->sprite.print(title);

    // SD + aktif pin göstergesi
    mainOS->sprite.fillCircle(SCREEN_W - 8, 11, 4,
                              sdAvailable ? 0x004400 : 0x440000);
    mainOS->sprite.fillCircle(SCREEN_W - 8, 11, 2,
                              sdAvailable ? COLOR_SUCCESS : COLOR_ACCENT);

    // FIX: Aktif pin bilgisini top bar'da göster
    mainOS->sprite.setTextColor(0x444444);
    mainOS->sprite.setCursor(SCREEN_W - 42, 7);
    mainOS->sprite.print(usePin44 ? "G44" : "G2 ");
}

void Cardputer_Remote::drawToast()
{
    if (statusMsg.length() == 0) return;
    unsigned long elapsed = millis() - statusTime;
    if (elapsed > 2500) { statusMsg = ""; return; }

    int charW = 6;
    int w = min((int)statusMsg.length() * charW + 16, SCREEN_W - 10);
    int x = (SCREEN_W - w) / 2;
    int y = SCREEN_H - 28;

    mainOS->sprite.fillRoundRect(x, y, w, 16, 4, 0x220011);
    mainOS->sprite.drawRoundRect(x, y, w, 16, 4, 0x884466);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);
    mainOS->sprite.setTextColor(COLOR_TEXT);
    mainOS->sprite.setCursor(x + 8, y + 4);

    String msg = statusMsg;
    int maxChars = (w - 16) / charW;
    if ((int)msg.length() > maxChars) msg = msg.substring(0, maxChars);
    mainOS->sprite.print(msg);
}

void Cardputer_Remote::drawBottomBar(const String &hint)
{
    mainOS->sprite.fillRect(0, SCREEN_H - 13, SCREEN_W, 13, 0x0A0A0A);
    mainOS->sprite.drawFastHLine(0, SCREEN_H - 13, SCREEN_W, 0x222222);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);
    mainOS->sprite.setTextColor(0x555555);
    mainOS->sprite.setCursor(3, SCREEN_H - 10);
    mainOS->sprite.print(hint);
}

// ================================================================
// MAIN MENU
// ================================================================

void Cardputer_Remote::drawMainMenu()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("MAIN MENU");

    for (int i = 0; i < MAIN_MENU_COUNT; i++)
    {
        int y    = MENU_START_Y + i * ITEM_H;
        bool sel = (i == menuIndex);
        uint32_t col = menuColors[i];

        if (sel)
        {
            mainOS->sprite.fillRoundRect(4, y, SCREEN_W-8, ITEM_H-3, 4, col);
            mainOS->sprite.fillRect(4, y, 4, ITEM_H-3, TFT_WHITE);
            mainOS->sprite.setTextColor(TFT_WHITE);
        }
        else
        {
            mainOS->sprite.fillRoundRect(4, y, SCREEN_W-8, ITEM_H-3, 4, 0x111111);
            mainOS->sprite.drawRoundRect(4, y, SCREEN_W-8, ITEM_H-3, 4, 0x222222);
            mainOS->sprite.fillRect(4, y, 3, ITEM_H-3, col);
            mainOS->sprite.setTextColor(0x888888);
        }

        mainOS->sprite.setFont(&fonts::FreeSansBold9pt7b);
        mainOS->sprite.setTextSize(1);
        mainOS->sprite.setCursor(14, y + 5);
        mainOS->sprite.print(mainMenuItems[i]);

        if (sel)
        {
            mainOS->sprite.setFont(&fonts::Font0);
            mainOS->sprite.setTextColor(TFT_WHITE);
            mainOS->sprite.setCursor(SCREEN_W - 14, y + 9);
            mainOS->sprite.print(">");
        }
    }

    drawBottomBar("Arrow/;.: Nav  Enter: Select  `: Exit");
}

// ================================================================
// CAPTURE
// ================================================================

void Cardputer_Remote::drawCaptureScreen()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("CAPTURE IR");
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    if (!signalCaptured)
    {
        uint32_t t = (millis() - animTimer) % 1280;
        int r = 8 + (t / 80) % 16;
        mainOS->sprite.drawCircle(30, 78, r, 0x001866);
        mainOS->sprite.fillCircle(30, 78, 7, 0x0033BB);
        mainOS->sprite.fillCircle(30, 78, 3, TFT_WHITE);

        mainOS->sprite.setTextColor(COLOR_WARNING);
        mainOS->sprite.setCursor(50, 42);
        mainOS->sprite.print("Waiting for IR...");
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(50, 58);
        mainOS->sprite.print("Point remote at");
        mainOS->sprite.setCursor(50, 70);
        mainOS->sprite.print("device and press");
        mainOS->sprite.setCursor(50, 82);
        mainOS->sprite.print("a button.");

        drawBottomBar("` : Back");
        needsRedraw = true;
    }
    else
    {
        if (capturedSignal.commands.empty()) return;
        IRCommand &cmd = capturedSignal.commands[0];

        mainOS->sprite.fillCircle(14, 42, 7, COLOR_SUCCESS);
        mainOS->sprite.setTextColor(COLOR_BG);
        mainOS->sprite.setCursor(9, 38);
        mainOS->sprite.print("OK");

        mainOS->sprite.fillRoundRect(26, 28, SCREEN_W-30, 70, 5, 0x080808);
        mainOS->sprite.drawRoundRect(26, 28, SCREEN_W-30, 70, 5, COLOR_SUCCESS);

        String protoStr = cmd.protocol.length() > 0 ? cmd.protocol : "RAW";
        mainOS->sprite.fillRoundRect(30, 31, (int)protoStr.length()*6+8, 12, 3, 0x002288);
        mainOS->sprite.setTextColor(TFT_WHITE);
        mainOS->sprite.setCursor(34, 34);
        mainOS->sprite.print(protoStr);

        if (cmd.type == "parsed")
        {
            char buf[16];
            mainOS->sprite.setTextColor(0x555555);
            mainOS->sprite.setCursor(30, 50);
            mainOS->sprite.print("Addr: ");
            mainOS->sprite.setTextColor(COLOR_SUCCESS);
            sprintf(buf, "%02X %02X", cmd.address & 0xFF, (cmd.address>>8) & 0xFF);
            mainOS->sprite.print(buf);

            mainOS->sprite.setTextColor(0x555555);
            mainOS->sprite.setCursor(30, 62);
            mainOS->sprite.print("Cmd:  ");
            mainOS->sprite.setTextColor(COLOR_WARNING);
            sprintf(buf, "%02X %02X", cmd.command & 0xFF, (cmd.command>>8) & 0xFF);
            mainOS->sprite.print(buf);
        }
        else
        {
            mainOS->sprite.setTextColor(0x555555);
            mainOS->sprite.setCursor(30, 50);
            mainOS->sprite.print("RAW ");
            mainOS->sprite.setTextColor(0xAAAAAA);
            mainOS->sprite.print((int)cmd.rawData.size());
            mainOS->sprite.print(" pulses");

            mainOS->sprite.setTextColor(0x555555);
            mainOS->sprite.setCursor(30, 62);
            mainOS->sprite.print("Freq: ");
            mainOS->sprite.setTextColor(COLOR_WARNING);
            mainOS->sprite.print(cmd.frequency / 1000);
            mainOS->sprite.print(" kHz");
        }

        mainOS->sprite.setTextColor(0x333333);
        mainOS->sprite.setCursor(30, 74);
        mainOS->sprite.print("Type: ");
        mainOS->sprite.setTextColor(0x6688AA);
        mainOS->sprite.print(cmd.type);

        mainOS->sprite.fillRoundRect(  4, 102, 54, 14, 3, 0x003300);
        mainOS->sprite.drawRoundRect(  4, 102, 54, 14, 3, COLOR_SUCCESS);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(8, 105);
        mainOS->sprite.print("S: Save");

        mainOS->sprite.fillRoundRect( 62, 102, 54, 14, 3, 0x000033);
        mainOS->sprite.drawRoundRect( 62, 102, 54, 14, 3, 0x4444FF);
        mainOS->sprite.setTextColor(0x4444FF);
        mainOS->sprite.setCursor(66, 105);
        mainOS->sprite.print("T: Test");

        mainOS->sprite.fillRoundRect(120, 102, 54, 14, 3, 0x221100);
        mainOS->sprite.drawRoundRect(120, 102, 54, 14, 3, COLOR_WARNING);
        mainOS->sprite.setTextColor(COLOR_WARNING);
        mainOS->sprite.setCursor(124, 105);
        mainOS->sprite.print("V: View");

        mainOS->sprite.fillRoundRect(178, 102, 54, 14, 3, 0x220000);
        mainOS->sprite.drawRoundRect(178, 102, 54, 14, 3, COLOR_ACCENT);
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(182, 105);
        mainOS->sprite.print("R: Redo");

        drawBottomBar("S:Save T:Test V:View R:Redo `:Back");
    }
}

// ================================================================
// SAVE PROMPT
// ================================================================

void Cardputer_Remote::drawSavePrompt()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("SAVE SIGNAL?");

    mainOS->sprite.fillRoundRect(10, 32, SCREEN_W-20, 68, 6, 0x0A0A0A);
    mainOS->sprite.drawRoundRect(10, 32, SCREEN_W-20, 68, 6, COLOR_WARNING);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    mainOS->sprite.setTextColor(COLOR_TEXT);
    mainOS->sprite.setCursor(18, 42);
    mainOS->sprite.print("Save before exiting?");

    if (!capturedSignal.commands.empty())
    {
        IRCommand &cmd = capturedSignal.commands[0];
        mainOS->sprite.setTextColor(0x555555);
        mainOS->sprite.setCursor(18, 56);
        mainOS->sprite.print("Proto: ");
        mainOS->sprite.setTextColor(0x4488FF);
        mainOS->sprite.print(cmd.protocol.length() > 0 ? cmd.protocol : "RAW");

        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(18, 68);
        if (cmd.type == "parsed")
        {
            char buf[20];
            sprintf(buf, "A:%02X C:%02X", cmd.address & 0xFF, cmd.command & 0xFF);
            mainOS->sprite.setTextColor(0x666666);
            mainOS->sprite.print(buf);
        }
        else
        {
            mainOS->sprite.setTextColor(0x666666);
            mainOS->sprite.print("RAW ");
            mainOS->sprite.print((int)cmd.rawData.size());
            mainOS->sprite.print(" pulses");
        }
    }

    mainOS->sprite.fillRoundRect( 18, 82,  84, 16, 4, 0x003300);
    mainOS->sprite.drawRoundRect( 18, 82,  84, 16, 4, COLOR_SUCCESS);
    mainOS->sprite.setTextColor(COLOR_SUCCESS);
    mainOS->sprite.setCursor(28, 86);
    mainOS->sprite.print("Y - Yes, Save");

    mainOS->sprite.fillRoundRect(118, 82, 100, 16, 4, 0x220000);
    mainOS->sprite.drawRoundRect(118, 82, 100, 16, 4, COLOR_ACCENT);
    mainOS->sprite.setTextColor(COLOR_ACCENT);
    mainOS->sprite.setCursor(128, 86);
    mainOS->sprite.print("N - Discard");
}

// ================================================================
// SAVE DIALOG
// ================================================================

void Cardputer_Remote::drawSaveDialog()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("NAME SIGNAL");

    mainOS->sprite.fillRoundRect(6, 26, SCREEN_W-12, 90, 6, 0x080808);
    mainOS->sprite.drawRoundRect(6, 26, SCREEN_W-12, 90, 6, 0x0055FF);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    mainOS->sprite.setTextColor(0x777777);
    mainOS->sprite.setCursor(14, 34);
    mainOS->sprite.print("Enter signal name:");

    mainOS->sprite.fillRoundRect(14, 46, SCREEN_W-28, 18, 3, 0x050505);
    mainOS->sprite.drawRoundRect(14, 46, SCREEN_W-28, 18, 3, 0x0055FF);
    mainOS->sprite.setTextColor(TFT_WHITE);
    mainOS->sprite.setCursor(18, 50);

    String disp = inputText;
    int maxDisplay = 24;
    if ((int)disp.length() > maxDisplay)
        disp = disp.substring(disp.length() - maxDisplay);
    mainOS->sprite.print(disp);

    if ((millis() / 400) % 2 == 0)
    {
        int cx = 18 + (int)disp.length() * 6;
        if (cx < SCREEN_W - 18)
            mainOS->sprite.drawFastVLine(cx, 48, 12, 0x0088FF);
    }

    if (!capturedSignal.commands.empty())
    {
        IRCommand &cmd = capturedSignal.commands[0];
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(14, 72);
        mainOS->sprite.print("Proto: ");
        mainOS->sprite.setTextColor(0x4488FF);
        mainOS->sprite.print(cmd.protocol.length() > 0 ? cmd.protocol : "RAW");
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.print("  Type: ");
        mainOS->sprite.setTextColor(0x4488FF);
        mainOS->sprite.print(cmd.type);
    }

    mainOS->sprite.setTextColor(0x333333);
    mainOS->sprite.setCursor(14, 84);
    mainOS->sprite.print("/IR_Signals/");
    mainOS->sprite.setTextColor(0x555555);
    String fn = (inputText.length() > 0) ? inputText : "?";
    fn.replace(" ", "_");
    mainOS->sprite.print(fn + ".ir");

    mainOS->sprite.fillRoundRect( 14, 100,  80, 14, 3, 0x003300);
    mainOS->sprite.drawRoundRect( 14, 100,  80, 14, 3, COLOR_SUCCESS);
    mainOS->sprite.setTextColor(COLOR_SUCCESS);
    mainOS->sprite.setCursor(20, 103);
    mainOS->sprite.print("Enter: Save");

    mainOS->sprite.fillRoundRect(102, 100, 124, 14, 3, 0x220000);
    mainOS->sprite.drawRoundRect(102, 100, 124, 14, 3, COLOR_ACCENT);
    mainOS->sprite.setTextColor(COLOR_ACCENT);
    mainOS->sprite.setCursor(108, 103);
    mainOS->sprite.print("`: Cancel");

    needsRedraw = true;
}

// ================================================================
// VIEW CODE
// ================================================================

void Cardputer_Remote::drawViewCodeScreen()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("IR CODE DETAIL");
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    if (selectedSignal.commands.empty())
    {
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(60, 65);
        mainOS->sprite.print("No data to show");
        drawBottomBar("` : Back");
        return;
    }

    IRCommand &cmd = selectedSignal.commands[0];
    String protoStr = cmd.protocol.length() > 0 ? cmd.protocol : "RAW";

    mainOS->sprite.fillRoundRect(2, 26, 90, 26, 4, 0x090909);
    mainOS->sprite.drawRoundRect(2, 26, 90, 26, 4, 0x0033AA);
    mainOS->sprite.setTextColor(0x334466);
    mainOS->sprite.setCursor(6, 29);
    mainOS->sprite.print("Protocol");
    mainOS->sprite.setTextColor(0x4488FF);
    mainOS->sprite.setCursor(6, 40);
    mainOS->sprite.print(protoStr);

    mainOS->sprite.fillRoundRect(96, 26, 60, 26, 4, 0x090909);
    mainOS->sprite.drawRoundRect(96, 26, 60, 26, 4, 0x003300);
    mainOS->sprite.setTextColor(0x224422);
    mainOS->sprite.setCursor(100, 29);
    mainOS->sprite.print("Type");
    mainOS->sprite.setTextColor(COLOR_SUCCESS);
    mainOS->sprite.setCursor(100, 40);
    mainOS->sprite.print(cmd.type);

    mainOS->sprite.fillRoundRect(160, 26, 78, 26, 4, 0x090909);
    mainOS->sprite.drawRoundRect(160, 26, 78, 26, 4, 0x332200);
    mainOS->sprite.setTextColor(0x443322);
    mainOS->sprite.setCursor(164, 29);
    mainOS->sprite.print("Freq");
    mainOS->sprite.setTextColor(COLOR_WARNING);
    mainOS->sprite.setCursor(164, 40);
    mainOS->sprite.print(String(cmd.frequency / 1000) + " kHz");

    if (cmd.type == "parsed")
    {
        char buf[36];
        mainOS->sprite.fillRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 60);
        mainOS->sprite.print("Address: ");
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        sprintf(buf, "%02X %02X %02X %02X",
                (uint8_t)(cmd.address & 0xFF),
                (uint8_t)((cmd.address >> 8)  & 0xFF),
                (uint8_t)((cmd.address >> 16) & 0xFF),
                (uint8_t)((cmd.address >> 24) & 0xFF));
        mainOS->sprite.print(buf);

        mainOS->sprite.fillRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 80);
        mainOS->sprite.print("Command: ");
        mainOS->sprite.setTextColor(0xCCCCCC);
        sprintf(buf, "%02X %02X %02X %02X",
                (uint8_t)(cmd.command & 0xFF),
                (uint8_t)((cmd.command >> 8)  & 0xFF),
                (uint8_t)((cmd.command >> 16) & 0xFF),
                (uint8_t)((cmd.command >> 24) & 0xFF));
        mainOS->sprite.print(buf);
    }
    else
    {
        mainOS->sprite.fillRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 60);
        mainOS->sprite.print("Pulses: ");
        mainOS->sprite.setTextColor(COLOR_WARNING);
        mainOS->sprite.print((int)cmd.rawData.size());

        mainOS->sprite.fillRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 80);
        mainOS->sprite.print("Data: ");
        mainOS->sprite.setTextColor(0x666666);
        String preview = "";
        int showN = min((int)cmd.rawData.size(), 5);
        for (int i = 0; i < showN; i++)
        {
            if (i > 0) preview += " ";
            preview += String(cmd.rawData[i]);
        }
        if ((int)cmd.rawData.size() > 5) preview += "...";
        mainOS->sprite.print(preview);
    }

    mainOS->sprite.setTextColor(0x333333);
    mainOS->sprite.setCursor(6, 100);
    mainOS->sprite.print("File: ");
    mainOS->sprite.setTextColor(0x555555);
    String fn = selectedSignal.filename;
    int ls2 = fn.lastIndexOf('/');
    if (ls2 >= 0) fn = fn.substring(ls2 + 1);
    if ((int)fn.length() > 22) fn = fn.substring(0, 22) + "..";
    mainOS->sprite.print(fn);

    drawBottomBar("Enter/S: Send    `: Back");
}

// ================================================================
// SD BROWSER
// ================================================================

void Cardputer_Remote::drawSDBrowserScreen()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("SD BROWSER");
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    if (!sdAvailable)
    {
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(SCREEN_W/2 - 54, 52);
        mainOS->sprite.print("SD Card not found!");
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(SCREEN_W/2 - 60, 66);
        mainOS->sprite.print("Insert card & restart");
        drawBottomBar("R: Refresh  ` : Back");
        return;
    }

    if (sdFiles.empty())
    {
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(SCREEN_W/2 - 54, 52);
        mainOS->sprite.print("No .ir files on SD");
        mainOS->sprite.setCursor(SCREEN_W/2 - 54, 66);
        mainOS->sprite.print("(all folders scanned)");
        drawBottomBar("R: Refresh   ` : Back");
        return;
    }

    const int visN   = 4;
    const int itemH  = 24;
    const int startY = 26;

    if (sdFileIndex < 0) sdFileIndex = 0;
    if (sdFileIndex >= (int)sdFiles.size()) sdFileIndex = (int)sdFiles.size() - 1;
    if (sdFileIndex < scrollOffset) scrollOffset = sdFileIndex;
    if (sdFileIndex >= scrollOffset + visN) scrollOffset = sdFileIndex - visN + 1;

    for (int i = 0; i < visN; i++)
    {
        int idx = i + scrollOffset;
        if (idx >= (int)sdFiles.size()) break;

        int  y   = startY + i * itemH;
        bool sel = (idx == sdFileIndex);
        bool isM = sdFiles[idx].isMulti;

        uint32_t selBg  = isM ? 0x1A1000 : 0x001800;
        uint32_t selBrd = isM ? COLOR_WARNING : COLOR_SUCCESS;
        uint32_t selBar = isM ? COLOR_WARNING : COLOR_SUCCESS;

        if (sel)
        {
            mainOS->sprite.fillRoundRect(2, y, SCREEN_W-4, itemH-2, 3, selBg);
            mainOS->sprite.drawRoundRect(2, y, SCREEN_W-4, itemH-2, 3, selBrd);
            mainOS->sprite.fillRect(2, y, 3, itemH-2, selBar);
        }
        else
        {
            mainOS->sprite.fillRoundRect(2, y, SCREEN_W-4, itemH-2, 3, 0x080808);
            mainOS->sprite.drawRoundRect(2, y, SCREEN_W-4, itemH-2, 3, 0x1A1A1A);
        }

        mainOS->sprite.setTextColor(sel ? selBar : (isM ? 0x443300 : 0x336633));
        mainOS->sprite.setCursor(6, y + 4);
        mainOS->sprite.print(isM ? "[M]" : ".ir");

        String dn = sdFiles[idx].displayName;
        if ((int)dn.length() > 23) dn = dn.substring(0, 23) + "..";
        mainOS->sprite.setTextColor(sel ? TFT_WHITE : 0x777777);
        mainOS->sprite.setCursor(32, y + 4);
        mainOS->sprite.print(dn);

        String sub = isM
            ? (String(sdFiles[idx].cmdCount) + " cmds")
            : (String(idx + 1) + "/" + String(sdFiles.size()));
        mainOS->sprite.setTextColor(sel ? 0x557755 : 0x2A2A2A);
        mainOS->sprite.setCursor(SCREEN_W - (int)sub.length() * 6 - 6, y + 14);
        mainOS->sprite.print(sub);
    }

    if ((int)sdFiles.size() > visN)
    {
        int sbH = visN * itemH;
        int sbX = SCREEN_W - 3;
        int sbY = startY;
        mainOS->sprite.fillRect(sbX, sbY, 2, sbH, 0x111111);
        int th = sbH * visN / (int)sdFiles.size();
        if (th < 4) th = 4;
        int ty = sbY + sbH * scrollOffset / (int)sdFiles.size();
        mainOS->sprite.fillRect(sbX, ty, 2, th, COLOR_SUCCESS);
    }

    drawBottomBar("Arrow:Nav  Ent:Open  V:Detail  R:Scan  `:Back");
}

// ================================================================
// REMOTE PAD
// ================================================================

void Cardputer_Remote::drawRemotePad()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    String title = selectedSignal.name;
    if ((int)title.length() > 16) title = title.substring(0, 16) + "..";
    drawTopBar(title);

    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    int cmdCount = (int)selectedSignal.commands.size();
    if (cmdCount == 0)
    {
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(60, 65);
        mainOS->sprite.print("No commands found!");
        drawBottomBar("` : Back");
        return;
    }

    int cols = 3;
    if (cmdCount == 1) cols = 1;
    else if (cmdCount <= 2) cols = 2;
    else if (cmdCount >= 9) cols = 4;

    int rows  = (cmdCount + cols - 1) / cols;
    int areaY = 25;
    int areaH = SCREEN_H - areaY - 15;
    int areaW = SCREEN_W - 4;

    int btnW = areaW / cols;
    int btnH = (rows > 0) ? min(areaH / rows, 30) : 30;

    for (int i = 0; i < cmdCount; i++)
    {
        int col = i % cols;
        int row = i / cols;
        int bx  = 2 + col * btnW;
        int by  = areaY + row * btnH;
        int bw  = btnW - 2;
        int bh  = btnH - 2;

        bool sel = (i == remotePadIdx);

        if (sel)
        {
            mainOS->sprite.fillRoundRect(bx, by, bw, bh, 4, 0x1A1A00);
            mainOS->sprite.drawRoundRect(bx, by, bw, bh, 4, COLOR_WARNING);
            mainOS->sprite.fillRect(bx, by, 3, bh, COLOR_WARNING);
        }
        else
        {
            mainOS->sprite.fillRoundRect(bx, by, bw, bh, 4, 0x0A0A0A);
            mainOS->sprite.drawRoundRect(bx, by, bw, bh, 4, 0x222222);
        }

        String lbl = selectedSignal.commands[i].label;
        if ((int)lbl.length() > 7) lbl = lbl.substring(0, 7);
        int tx = bx + (bw - (int)lbl.length() * 6) / 2;
        int ty = by + (bh - 8) / 2;
        mainOS->sprite.setTextColor(sel ? COLOR_WARNING : 0x777777);
        mainOS->sprite.setCursor(tx, ty);
        mainOS->sprite.print(lbl);
    }

    if (remotePadIdx >= 0 && remotePadIdx < cmdCount)
    {
        IRCommand &sc = selectedSignal.commands[remotePadIdx];
        String protoStr = sc.protocol.length() > 0 ? sc.protocol : "RAW";
        String info = sc.label + " [" + protoStr + "]";
        if (sc.type == "parsed")
        {
            char buf[20];
            sprintf(buf, " A:%02X C:%02X",
                    (uint8_t)(sc.address & 0xFF),
                    (uint8_t)(sc.command & 0xFF));
            info += String(buf);
        }
        if ((int)info.length() > 34) info = info.substring(0, 34);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(4, SCREEN_H - 24);
        mainOS->sprite.print(info);
    }

    drawBottomBar("Arrow:Nav  Enter:Send  `:Back");
}

// ================================================================
// SEND CONFIRM
// ================================================================

void Cardputer_Remote::drawSendConfirm(bool sending)
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("TRANSMIT");

    mainOS->sprite.fillRoundRect(6, 26, SCREEN_W-12, 80, 6, 0x080808);
    mainOS->sprite.drawRoundRect(6, 26, SCREEN_W-12, 80, 6,
                                 sending ? COLOR_SUCCESS : 0x0044FF);

    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    if (sending)
    {
        mainOS->sprite.setTextSize(2);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(80, 52);
        mainOS->sprite.print(">>>");
        mainOS->sprite.setTextSize(1);
        mainOS->sprite.setTextColor(0x777777);
        mainOS->sprite.setCursor(SCREEN_W/2 - 40, 82);
        mainOS->sprite.print("Transmitting...");
    }
    else
    {
        mainOS->sprite.setTextColor(0x555555);
        mainOS->sprite.setCursor(14, 34);
        mainOS->sprite.print("Ready to send:");

        mainOS->sprite.setTextColor(TFT_WHITE);
        mainOS->sprite.setCursor(14, 46);
        String nm = selectedSignal.name;
        if ((int)nm.length() > 26) nm = nm.substring(0, 26) + "..";
        mainOS->sprite.print(nm);

        if (!selectedSignal.commands.empty())
        {
            IRCommand &cmd = selectedSignal.commands[0];
            String pt = cmd.protocol.length() > 0 ? cmd.protocol : "RAW";
            int pw = (int)pt.length() * 6 + 8;
            mainOS->sprite.fillRoundRect(14, 58, pw, 12, 3, 0x001A44);
            mainOS->sprite.setTextColor(0x4488FF);
            mainOS->sprite.setCursor(18, 61);
            mainOS->sprite.print(pt);

            mainOS->sprite.setCursor(14, 74);
            if (cmd.type == "parsed")
            {
                char buf[24];
                sprintf(buf, "A:%02X C:%02X",
                        (uint8_t)(cmd.address & 0xFF),
                        (uint8_t)(cmd.command & 0xFF));
                mainOS->sprite.setTextColor(0x555555);
                mainOS->sprite.print(buf);
            }
            else
            {
                mainOS->sprite.setTextColor(0x555555);
                mainOS->sprite.print("RAW ");
                mainOS->sprite.print((int)cmd.rawData.size());
                mainOS->sprite.print(" pulses");
            }
        }

        mainOS->sprite.fillRoundRect( 14, 92,  88, 14, 4, 0x003300);
        mainOS->sprite.drawRoundRect( 14, 92,  88, 14, 4, COLOR_SUCCESS);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(22, 95);
        mainOS->sprite.print("Enter: SEND");

        mainOS->sprite.fillRoundRect(114, 92, 110, 14, 4, 0x1A0000);
        mainOS->sprite.drawRoundRect(114, 92, 110, 14, 4, COLOR_ACCENT);
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(120, 95);
        mainOS->sprite.print("`: Cancel");
    }
}

// ================================================================
// SETTINGS
// ================================================================

void Cardputer_Remote::drawSettingsScreen()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("SETTINGS");
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    mainOS->sprite.fillRoundRect(4, 26, SCREEN_W-8, 36, 5, 0x0A0A0A);
    mainOS->sprite.drawRoundRect(4, 26, SCREEN_W-8, 36, 5, 0x333333);
    mainOS->sprite.setTextColor(0x446644);
    mainOS->sprite.setCursor(10, 32);
    mainOS->sprite.print("Recv: PIN G1  (fixed)");
    mainOS->sprite.setTextColor(0x444488);
    mainOS->sprite.setCursor(10, 44);
    mainOS->sprite.print("Format: Flipper Zero .ir");

    mainOS->sprite.drawFastHLine(4, 66, SCREEN_W-8, 0x222222);
    mainOS->sprite.setTextColor(0x886600);
    mainOS->sprite.setCursor(10, 72);
    mainOS->sprite.print("Select IR Sender Pin:");

    // ---- G44 kutusu ----
    bool sel44 = (settingsIndex == 0);
    mainOS->sprite.fillRoundRect(4, 82, 110, 34, 5, sel44 ? 0x2A1A00 : 0x0A0A0A);
    mainOS->sprite.drawRoundRect(4, 82, 110, 34, 5, sel44 ? COLOR_WARNING : 0x222222);
    if (sel44) mainOS->sprite.fillRect(4, 82, 4, 34, COLOR_WARNING);

    mainOS->sprite.setTextColor(sel44 ? COLOR_WARNING : 0x555555);
    mainOS->sprite.setCursor(12, 88);
    mainOS->sprite.print("PIN G44");

    mainOS->sprite.setTextColor(sel44 ? 0x888855 : 0x333333);
    mainOS->sprite.setCursor(12, 100);
    mainOS->sprite.print("Default sender");

    if (usePin44)
    {
        mainOS->sprite.fillRoundRect(60, 83, 50, 11, 3, 0x003300);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(63, 85);
        mainOS->sprite.print("[ACTIVE]");
    }

    // ---- G2 kutusu ----
    bool sel2 = (settingsIndex == 1);
    mainOS->sprite.fillRoundRect(120, 82, 110, 34, 5, sel2 ? 0x001A0A : 0x0A0A0A);
    mainOS->sprite.drawRoundRect(120, 82, 110, 34, 5, sel2 ? COLOR_SUCCESS : 0x222222);
    if (sel2) mainOS->sprite.fillRect(120, 82, 4, 34, COLOR_SUCCESS);

    mainOS->sprite.setTextColor(sel2 ? COLOR_SUCCESS : 0x555555);
    mainOS->sprite.setCursor(128, 88);
    mainOS->sprite.print("PIN G2");

    mainOS->sprite.setTextColor(sel2 ? 0x336655 : 0x333333);
    mainOS->sprite.setCursor(128, 100);
    mainOS->sprite.print("Alt sender");

    if (!usePin44)
    {
        mainOS->sprite.fillRoundRect(172, 83, 50, 11, 3, 0x003300);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(175, 85);
        mainOS->sprite.print("[ACTIVE]");
    }

    drawBottomBar(",./ :Sel  Enter:Apply  `:Back");
}

// ================================================================
// MASTER DRAW
// ================================================================

void Cardputer_Remote::drawScreen()
{
    switch (currentState)
    {
    case STATE_MAIN_MENU:    drawMainMenu();         break;
    case STATE_CAPTURE:      drawCaptureScreen();    break;
    case STATE_SAVE_PROMPT:  drawSavePrompt();       break;
    case STATE_CAPTURE_SAVE: drawSaveDialog();       break;
    case STATE_VIEW_CODE:    drawViewCodeScreen();   break;
    case STATE_SD_BROWSER:   drawSDBrowserScreen();  break;
    case STATE_SEND_CONFIRM: drawSendConfirm(false); break;
    case STATE_SETTINGS:     drawSettingsScreen();   break;
    case STATE_REMOTE_PAD:   drawRemotePad();        break;
    default:                 drawMainMenu();         break;
    }
    drawToast();
    mainOS->sprite.pushSprite(0, 0);
}

// ================================================================
// IR RECEIVE
// ================================================================

void Cardputer_Remote::handleIRReceive()
{
    if (!capturing || signalCaptured) return;
    if (!irRecv.decode(&irResults))   return;

    uint32_t val = (uint32_t)irResults.value;

    if (val == 0xFFFFFFFF || val == 0x00000000 ||
        millis() - lastCapture < 300)
    {
        irRecv.resume();
        return;
    }

    IRCommand cmd;
    cmd.label     = "BTN";
    cmd.frequency = 38000;
    cmd.dutyCycle = 0.33f;

    bool isRaw = (irResults.decode_type == UNKNOWN);

    if (isRaw)
    {
        cmd.type     = "raw";
        cmd.protocol = "";
        cmd.address  = 0;
        cmd.command  = 0;

        int copyLen = min((int)irResults.rawlen - 1, MAX_RAW_LEN);
        cmd.rawData.reserve(copyLen);
        for (int i = 1; i <= copyLen; i++)
        {
            uint16_t pulse = (uint16_t)(irResults.rawbuf[i] * RAWTICK);
            if (pulse > 0) cmd.rawData.push_back(pulse);
        }
    }
    else
    {
        cmd.type     = "parsed";
        cmd.protocol = protoName(irResults.decode_type);

        switch (irResults.decode_type)
        {
        case NEC:
        case SAMSUNG:
            cmd.address = (val >> 24) & 0xFF;
            cmd.command = (val >> 8) & 0xFF;
            break;
        case SONY:
            cmd.command = val & 0x7F;
            cmd.address = (val >> 7) & 0x1F;
            break;
        default:
            cmd.address = val & 0xFF;
            cmd.command = (val >> 8) & 0xFF;
            break;
        }
    }

    capturedSignal.clear();
    capturedSignal.name     = "IR_" + String(millis() % 100000);
    capturedSignal.isRaw    = isRaw;
    capturedSignal.isMulti  = false;
    capturedSignal.commands.push_back(cmd);
    capturedSignal.protocol = cmd.protocol;
    capturedSignal.address  = cmd.address;
    capturedSignal.command  = cmd.command;
    capturedSignal.frequency= cmd.frequency;
    capturedSignal.rawData  = cmd.rawData;

    signalCaptured = true;
    lastCapture    = millis();
    capturing      = false;
    irRecv.pause();
    needsRedraw    = true;
}

// ================================================================
// KEY HANDLERS
// ================================================================

void Cardputer_Remote::handleMainMenu(char key)
{
    if (key == ';')
        menuIndex = (menuIndex - 1 + MAIN_MENU_COUNT) % MAIN_MENU_COUNT;
    else if (key == '.')
        menuIndex = (menuIndex + 1) % MAIN_MENU_COUNT;
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        scrollOffset = 0;
        switch (menuIndex)
        {
        case 0:
            currentState   = STATE_CAPTURE;
            signalCaptured = false;
            capturing      = true;
            irRecv.enableIRIn();
            animTimer = millis();
            break;
        case 1:
            if (!sdAvailable) initSD();
            scanSDFiles();
            currentState = STATE_SD_BROWSER;
            sdFileIndex  = 0;
            scrollOffset = 0;
            break;
        case 2:
            currentState  = STATE_SETTINGS;
            settingsIndex = 0;
            break;
        }
    }
}

void Cardputer_Remote::handleCapture(char key)
{
    if (key == '`' || key == 27)
    {
        if (signalCaptured)
            currentState = STATE_SAVE_PROMPT;
        else
        {
            capturing = false;
            irRecv.pause();
            currentState = STATE_MAIN_MENU;
        }
        return;
    }

    if (!signalCaptured) return;

    if (key == 's' || key == 'S')
    {
        if (sdAvailable) { currentState = STATE_CAPTURE_SAVE; inputText = ""; }
        else setStatus("SD not available!");
    }
    else if (key == 't' || key == 'T')
    {
        bool ok = sendIR(capturedSignal);
        setStatus(ok ? "Signal sent!" : "Send failed!");
    }
    else if (key == 'v' || key == 'V')
    {
        selectedSignal = capturedSignal;
        previousState  = STATE_CAPTURE;
        currentState   = STATE_VIEW_CODE;
    }
    else if (key == 'r' || key == 'R')
    {
        signalCaptured = false;
        capturing      = true;
        capturedSignal.clear();
        irRecv.enableIRIn();
        animTimer = millis();
    }
}

void Cardputer_Remote::handleSavePrompt(char key)
{
    if (key == 'y' || key == 'Y')
    {
        currentState = STATE_CAPTURE_SAVE;
        inputText    = "";
    }
    else if (key == 'n' || key == 'N' || key == '`' || key == 27)
    {
        capturing      = false;
        irRecv.pause();
        signalCaptured = false;
        capturedSignal.clear();
        currentState   = STATE_MAIN_MENU;
    }
}

void Cardputer_Remote::handleSaveDialog(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = STATE_CAPTURE;
        return;
    }

    if (key == '\n' || key == '\r')
    {
        if (inputText.length() == 0)
        {
            setStatus("Name cannot be empty!");
            return;
        }
        capturedSignal.name = inputText;
        if (!capturedSignal.commands.empty())
            capturedSignal.commands[0].label = inputText;

        if (saveSignalToSD(capturedSignal))
        {
            scanSDFiles();
            setStatus("Saved: " + inputText + ".ir");
            currentState   = STATE_MAIN_MENU;
            signalCaptured = false;
            capturing      = false;
            inputText      = "";
            capturedSignal.clear();
        }
        else
        {
            setStatus("Save failed!");
        }
        return;
    }

    if (key == 8 || key == 127)
    {
        if (inputText.length() > 0)
            inputText.remove(inputText.length() - 1);
    }
    else if (key >= 32 && key <= 126 && (int)inputText.length() < 20)
    {
        if (key != '/' && key != '\\' && key != ':' &&
            key != '*' && key != '?' && key != '"' &&
            key != '<' && key != '>' && key != '|')
        {
            inputText += key;
        }
    }
}

void Cardputer_Remote::handleViewCode(char key)
{
    if (key == '`' || key == 27)
        currentState = previousState;
    else if (key == 's' || key == 'S' || key == '\n' || key == '\r')
    {
        bool ok = sendIR(selectedSignal);
        setStatus(ok ? "Signal sent!" : "Send failed!");
    }
}

void Cardputer_Remote::handleSDBrowser(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = STATE_MAIN_MENU;
        return;
    }

    if (sdFiles.empty())
    {
        if (key == 'r' || key == 'R')
        {
            scanSDFiles();
            sdFileIndex = 0; scrollOffset = 0;
            setStatus(String((int)sdFiles.size()) + " files found");
        }
        return;
    }

    if (key == ';')
    {
        if (sdFileIndex > 0) sdFileIndex--;
    }
    else if (key == '.')
    {
        if (sdFileIndex < (int)sdFiles.size() - 1) sdFileIndex++;
    }
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        IRSignal sig;
        if (loadSignalFromSD(sdFiles[sdFileIndex].path, sig))
        {
            selectedSignal = sig;
            previousState  = STATE_SD_BROWSER;

            if (sig.isMulti)
            {
                remotePadIdx = 0;
                currentState = STATE_REMOTE_PAD;
            }
            else
            {
                currentState = STATE_SEND_CONFIRM;
            }
        }
        else
        {
            setStatus("Load failed: " + sdFiles[sdFileIndex].path);
        }
    }
    else if (key == 'v' || key == 'V')
    {
        IRSignal sig;
        if (loadSignalFromSD(sdFiles[sdFileIndex].path, sig))
        {
            selectedSignal = sig;
            previousState  = STATE_SD_BROWSER;
            currentState   = STATE_VIEW_CODE;
        }
        else
        {
            setStatus("Load failed!");
        }
    }
    else if (key == 'r' || key == 'R')
    {
        scanSDFiles();
        sdFileIndex  = 0;
        scrollOffset = 0;
        setStatus(String((int)sdFiles.size()) + " files found");
    }
}

void Cardputer_Remote::handleSendConfirm(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = previousState;
        return;
    }

    if (key == '\n' || key == '\r' || key == ' ' ||
        key == 's' || key == 'S')
    {
        mainOS->sprite.fillScreen(COLOR_BG);
        drawSendConfirm(true);
        mainOS->sprite.pushSprite(0, 0);

        bool ok = sendIR(selectedSignal);
        delay(300);

        setStatus(ok ? "Sent OK!" : "Send failed!");
        currentState = previousState;
    }
}

void Cardputer_Remote::handleSettings(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = STATE_MAIN_MENU;
        return;
    }

    // FIX: Sol/Sağ ok ve virgül/nokta ile seçim
    if (key == ';' || key == ',')
        settingsIndex = 0;   // G44
    else if (key == '.' || key == '/')
        settingsIndex = 1;   // G2
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        if (settingsIndex == 0)
        {
            // G44 seç
            if (!usePin44)
            {
                usePin44 = true;
                sendPin  = IR_SEND_PIN_G44;   // 44
                // FIX: Yeni IRsend nesnesi oluştur
                createIRSend(sendPin);
                setStatus("Pin set to G44 (pin 44)");
            }
            else
            {
                setStatus("G44 already active");
            }
        }
        else if (settingsIndex == 1)
        {
            // G2 seç
            if (usePin44)
            {
                usePin44 = false;
                sendPin  = IR_SEND_PIN_G2;    // 2
                // FIX: Yeni IRsend nesnesi oluştur
                createIRSend(sendPin);
                setStatus("Pin set to G2 (pin 2)");
            }
            else
            {
                setStatus("G2 already active");
            }
        }
    }
}

void Cardputer_Remote::handleRemotePad(char key)
{
    int cmdCount = (int)selectedSignal.commands.size();
    if (cmdCount == 0)
    {
        currentState = previousState;
        return;
    }

    int cols = 3;
    if (cmdCount == 1) cols = 1;
    else if (cmdCount <= 2) cols = 2;
    else if (cmdCount >= 9) cols = 4;

    if (key == '`' || key == 27)
    {
        currentState = previousState;
    }
    else if (key == ',')
    {
        if (remotePadIdx % cols > 0) remotePadIdx--;
    }
    else if (key == '/')
    {
        if (remotePadIdx % cols < cols - 1 && remotePadIdx + 1 < cmdCount)
            remotePadIdx++;
    }
    else if (key == ';')
    {
        if (remotePadIdx - cols >= 0) remotePadIdx -= cols;
    }
    else if (key == '.')
    {
        if (remotePadIdx + cols < cmdCount) remotePadIdx += cols;
    }
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        if (remotePadIdx < 0 || remotePadIdx >= cmdCount) return;

        String lbl = selectedSignal.commands[remotePadIdx].label;

        // Gönder animasyonu
        mainOS->sprite.fillScreen(COLOR_BG);
        drawTopBar(selectedSignal.name.length() > 16
                   ? selectedSignal.name.substring(0, 16)
                   : selectedSignal.name);
        mainOS->sprite.setFont(&fonts::Font0);
        mainOS->sprite.setTextSize(2);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(80, 52);
        mainOS->sprite.print(">>>");
        mainOS->sprite.setTextSize(1);
        mainOS->sprite.setTextColor(0x888888);
        mainOS->sprite.setCursor(50, 82);
        String sendTxt = "Sending: " + lbl;
        if ((int)sendTxt.length() > 30) sendTxt = sendTxt.substring(0, 30);
        mainOS->sprite.print(sendTxt);
        mainOS->sprite.pushSprite(0, 0);

        bool ok = sendIRCommand(selectedSignal.commands[remotePadIdx]);
        delay(150);

        setStatus(ok ? (lbl + " sent!") : "Send failed!");
        needsRedraw = true;
    }
}

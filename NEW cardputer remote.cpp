#include "Cardputer_Remote.h"
#include "MyOS.h"
#include "MainMenuV2.h"

// ================================================================
// BEGIN / LOOP / DRAW / EXIT
// ================================================================

void Cardputer_Remote::Begin()
{
    showTopBar = false;
    mainOS->sprite.createSprite(SCREEN_W, SCREEN_H);
    irRecv.enableIRIn();
    irSend.begin();
    initSD();
    if (sdAvailable) scanSDFiles();
    animTimer = millis();
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
            default: break;
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
        currentState == STATE_CAPTURE_SAVE ||
        currentState == STATE_SEND_CONFIRM)
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
void Cardputer_Remote::OnExit() {}

// ================================================================
// YARDIMCI
// ================================================================

void Cardputer_Remote::setStatus(String msg)
{
    statusMsg  = msg;
    statusTime = millis();
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
    default:         return "RAW";
    }
}

decode_type_t Cardputer_Remote::strToProto(String s)
{
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
    return UNKNOWN;
}

// ================================================================
// SD KART  –  FLIPPER IR FORMAT
// ================================================================

bool Cardputer_Remote::initSD()
{
    sdAvailable = mainOS->haveSDcard;
    if (sdAvailable && !SD.exists("/IR_Signals"))
        SD.mkdir("/IR_Signals");
    return sdAvailable;
}

// ----------------------------------------------------------------
//  KAYDET  –  Flipper Zero .ir formatı
// ----------------------------------------------------------------
//
//  Filetype: IR signals file
//  Version: 1
//  #
//  name: VOL_UP
//  type: parsed
//  protocol: NEC
//  address: 04 00 00 00
//  command: 08 00 00 00
//  #
//  name: BTN_RAW
//  type: raw
//  frequency: 38000
//  duty_cycle: 0.330000
//  data: 9000 4500 560 560 ...
//
// ----------------------------------------------------------------
bool Cardputer_Remote::saveSignalToSD(IRSignal &sig)
{
    if (!sdAvailable) return false;

    String fn = "/IR_Signals/" + sig.name + ".ir";
    fn.replace(" ", "_");
    sig.filename = fn;

    if (SD.exists(fn)) SD.remove(fn);
    File f = SD.open(fn, FILE_WRITE);
    if (!f) return false;

    // Başlık
    f.println("Filetype: IR signals file");
    f.println("Version: 1");

    for (auto &cmd : sig.commands)
    {
        f.println("#");
        f.print("name: ");   f.println(cmd.label);
        f.print("type: ");   f.println(cmd.type);

        if (cmd.type == "parsed")
        {
            f.print("protocol: "); f.println(cmd.protocol);

            // address: "04 00 00 00" formatı
            char buf[32];
            uint8_t a0 =  cmd.address        & 0xFF;
            uint8_t a1 = (cmd.address >>  8) & 0xFF;
            uint8_t a2 = (cmd.address >> 16) & 0xFF;
            uint8_t a3 = (cmd.address >> 24) & 0xFF;
            sprintf(buf, "%02X %02X %02X %02X", a0, a1, a2, a3);
            f.print("address: "); f.println(buf);

            uint8_t c0 =  cmd.command        & 0xFF;
            uint8_t c1 = (cmd.command >>  8) & 0xFF;
            uint8_t c2 = (cmd.command >> 16) & 0xFF;
            uint8_t c3 = (cmd.command >> 24) & 0xFF;
            sprintf(buf, "%02X %02X %02X %02X", c0, c1, c2, c3);
            f.print("command: "); f.println(buf);
        }
        else // raw
        {
            f.print("frequency: ");  f.println(cmd.frequency);
            f.print("duty_cycle: "); f.println(cmd.dutyCycle, 6);
            f.print("data:");
            for (auto &v : cmd.rawData)
            {
                f.print(" ");
                f.print(v);
            }
            f.println();
        }
    }

    f.close();
    return true;
}

// ----------------------------------------------------------------
//  OKU  –  Flipper Zero .ir formatı
// ----------------------------------------------------------------
bool Cardputer_Remote::loadSignalFromSD(String filename, IRSignal &sig)
{
    File f = SD.open(filename, FILE_READ);
    if (!f) return false;

    sig.filename = filename;
    sig.commands.clear();
    sig.isMulti  = false;
    sig.isRaw    = false;
    sig.frequency= 38000;

    // Dosya adından isim üret
    String base = filename;
    int ls = base.lastIndexOf('/');
    if (ls >= 0) base = base.substring(ls + 1);
    if (base.endsWith(".ir") || base.endsWith(".IR"))
        base = base.substring(0, base.length() - 3);
    base.replace("_", " ");
    sig.name = base;

    IRCommand currentCmd;
    bool inBlock = false;

    auto trimStr = [](String s) -> String {
        while (s.length() > 0 && (s[0] == ' ' || s[0] == '\r' || s[0] == '\t'))
            s = s.substring(1);
        while (s.length() > 0 && (s[s.length()-1] == ' ' ||
                                   s[s.length()-1] == '\r' ||
                                   s[s.length()-1] == '\n' ||
                                   s[s.length()-1] == '\t'))
            s = s.substring(0, s.length()-1);
        return s;
    };

    // Hex string "04 00 00 00" → uint32
    auto parseHexBytes = [](String s) -> uint32_t {
        s.replace(" ", "");
        s.toUpperCase();
        uint32_t val = 0;
        for (char c : s)
        {
            val <<= 4;
            if (c >= '0' && c <= '9') val |= (c - '0');
            else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
        }
        return val;
    };

    while (f.available())
    {
        String line = f.readStringUntil('\n');
        line = trimStr(line);

        if (line.length() == 0) continue;

        // Yorum / başlık satırları
        if (line.startsWith("Filetype:")) continue;
        if (line.startsWith("Version:"))  continue;

        // Blok ayırıcı
        if (line == "#")
        {
            if (inBlock && currentCmd.label.length() > 0)
            {
                sig.commands.push_back(currentCmd);
            }
            currentCmd = IRCommand();
            currentCmd.frequency = 38000;
            currentCmd.dutyCycle = 0.33f;
            inBlock = true;
            continue;
        }

        // Key: Value ayrıştır
        int colon = line.indexOf(':');
        if (colon < 0) continue;

        String key = trimStr(line.substring(0, colon));
        String val = trimStr(line.substring(colon + 1));

        key.toLowerCase();

        if      (key == "name")       currentCmd.label    = val;
        else if (key == "type")       { val.toLowerCase(); currentCmd.type = val; }
        else if (key == "protocol")   currentCmd.protocol = val;
        else if (key == "address")    currentCmd.address  = parseHexBytes(val);
        else if (key == "command")    currentCmd.command  = parseHexBytes(val);
        else if (key == "frequency")  currentCmd.frequency= (uint32_t)val.toInt();
        else if (key == "duty_cycle") currentCmd.dutyCycle= val.toFloat();
        else if (key == "data")
        {
            currentCmd.rawData.clear();
            // val: "9000 4500 560 560 ..."
            String d = val;
            d.trim();
            int start = 0;
            while (start < (int)d.length())
            {
                int sp = d.indexOf(' ', start);
                String token;
                if (sp < 0) { token = d.substring(start); start = d.length(); }
                else        { token = d.substring(start, sp); start = sp + 1; }
                token.trim();
                if (token.length() > 0)
                    currentCmd.rawData.push_back((uint16_t)token.toInt());
            }
        }
    }

    // Son bloğu ekle
    if (inBlock && currentCmd.label.length() > 0)
        sig.commands.push_back(currentCmd);

    f.close();

    if (sig.commands.empty()) return false;

    sig.isMulti = (sig.commands.size() > 1);

    // İlk komuttan hızlı erişim alanlarını doldur
    IRCommand &first = sig.commands[0];
    sig.protocol  = first.protocol;
    sig.address   = first.address;
    sig.command   = first.command;
    sig.frequency = first.frequency;
    sig.isRaw     = (first.type == "raw");
    sig.rawData   = first.rawData;

    return true;
}

// ----------------------------------------------------------------
//  RECURSIVE TARAMA
// ----------------------------------------------------------------
void Cardputer_Remote::scanSDRecursive(File dir, const String &basePath)
{
    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry) break;

        String entryName = String(entry.name());

        if (entry.isDirectory())
        {
            if (entryName.startsWith(".")) { entry.close(); continue; }
            String subPath = basePath + "/" + entryName;
            File subDir = SD.open(subPath);
            if (subDir) { scanSDRecursive(subDir, subPath); subDir.close(); }
        }
        else
        {
            String lower = entryName;
            lower.toLowerCase();
            if (lower.endsWith(".ir"))
            {
                String fullPath = basePath + "/" + entryName;

                SDFileInfo info;
                info.path     = fullPath;
                info.isMulti  = false;
                info.cmdCount = 1;

                // Görünen ad
                String dn = entryName;
                if (dn.length() > 3) dn = dn.substring(0, dn.length() - 3);
                dn.replace("_", " ");

                // Klasör prefix
                if (basePath.length() > 0 && basePath != "/")
                {
                    int lsi = basePath.lastIndexOf('/');
                    String folder = (lsi >= 0)
                                    ? basePath.substring(lsi + 1)
                                    : basePath;
                    info.displayName = "[" + folder + "] " + dn;
                }
                else info.displayName = dn;

                // Hızlı çoklu kontrol: '#' sayısına bak
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
        }
        entry.close();
    }
}

void Cardputer_Remote::scanSDFiles()
{
    sdFiles.clear();
    if (!sdAvailable) return;
    File root = SD.open("/");
    if (!root) return;
    scanSDRecursive(root, "");
    root.close();
}

bool Cardputer_Remote::deleteFromSD(String fn)
{
    if (!sdAvailable) return false;
    return SD.remove(fn);
}

// ================================================================
// IR GÖNDER
// ================================================================

void Cardputer_Remote::reinitIRSend()
{
    irSend.~IRsend();
    new (&irSend) IRsend(sendPin);
    irSend.begin();
    delay(10);
}

bool Cardputer_Remote::sendIRCommand(IRCommand &cmd)
{
    irSend.begin();
    delay(10);

    if (cmd.type == "raw")
    {
        if (cmd.rawData.empty()) return false;
        irSend.sendRaw(cmd.rawData.data(), cmd.rawData.size(),
                       cmd.frequency > 0 ? cmd.frequency / 1000 : 38);
        return true;
    }

    // parsed
    decode_type_t proto = strToProto(cmd.protocol);

    // NEC ve benzeri: address + command birleştir → value
    // Flipper format: address low byte + command low byte
    uint8_t addr = cmd.address & 0xFF;
    uint8_t cmdB = cmd.command & 0xFF;

    switch (proto)
    {
    case NEC:
    {
        // NEC: 32bit  addrByte + ~addrByte + cmdByte + ~cmdByte
        uint32_t val = ((uint32_t)addr << 24) |
                       ((uint32_t)(~addr & 0xFF) << 16) |
                       ((uint32_t)cmdB  <<  8) |
                       ((uint32_t)(~cmdB & 0xFF));
        irSend.sendNEC(val, 32);
        break;
    }
    case SAMSUNG:
    {
        uint32_t val = ((uint32_t)addr << 24) |
                       ((uint32_t)addr << 16) |
                       ((uint32_t)cmdB  <<  8) |
                       ((uint32_t)(~cmdB & 0xFF));
        irSend.sendSAMSUNG(val, 32);
        break;
    }
    case SONY:
        irSend.sendSony(cmdB, 12);
        break;
    case RC5:
        irSend.sendRC5(cmdB, 12);
        break;
    case RC6:
        irSend.sendRC6(cmdB, 20);
        break;
    case LG:
    {
        uint32_t val = ((uint32_t)addr << 16) | ((uint32_t)cmdB << 8);
        irSend.sendLG(val, 28);
        break;
    }
    case PANASONIC:
        irSend.sendPanasonic(addr, cmdB);
        break;
    case JVC:
        irSend.sendJVC(((uint32_t)addr << 8) | cmdB, 16, 0);
        break;
    case SHARP:
        irSend.sendSharpRaw(((uint32_t)addr << 8) | cmdB, 15);
        break;
    case DENON:
        irSend.sendDenon(((uint32_t)addr << 8) | cmdB, 15);
        break;
    default:
        if (!cmd.rawData.empty())
        {
            irSend.sendRaw(cmd.rawData.data(), cmd.rawData.size(), 38);
            return true;
        }
        return false;
    }
    return true;
}

bool Cardputer_Remote::sendIR(IRSignal &sig)
{
    if (sig.commands.empty()) return false;
    return sendIRCommand(sig.commands[0]);
}

// ================================================================
// DRAW HELPERS
// ================================================================

void Cardputer_Remote::drawTopBar(String title)
{
    mainOS->sprite.fillRect(0, 0, SCREEN_W, 22, 0x111111);
    mainOS->sprite.drawFastHLine(0, 22, SCREEN_W, 0x333333);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);
    mainOS->sprite.setTextColor(0x4488FF);
    mainOS->sprite.setCursor(4, 7);
    mainOS->sprite.print("IR");
    mainOS->sprite.setTextColor(COLOR_TEXT);
    int tx = (SCREEN_W - (int)title.length() * 6) / 2;
    mainOS->sprite.setCursor(tx, 7);
    mainOS->sprite.print(title);
    mainOS->sprite.fillCircle(SCREEN_W - 8, 11, 4, sdAvailable ? 0x004400 : 0x440000);
    mainOS->sprite.fillCircle(SCREEN_W - 8, 11, 2, sdAvailable ? COLOR_SUCCESS : COLOR_ACCENT);
}

void Cardputer_Remote::drawToast()
{
    if (statusMsg.length() == 0) return;
    if (millis() - statusTime > 2500) { statusMsg = ""; return; }
    int w = min((int)statusMsg.length() * 6 + 16, SCREEN_W - 20);
    int x = (SCREEN_W - w) / 2, y = SCREEN_H - 28;
    mainOS->sprite.fillRoundRect(x, y, w, 16, 4, 0x220011);
    mainOS->sprite.drawRoundRect(x, y, w, 16, 4, 0x884466);
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);
    mainOS->sprite.setTextColor(COLOR_TEXT);
    mainOS->sprite.setCursor(x + 8, y + 4);
    mainOS->sprite.print(statusMsg);
}

void Cardputer_Remote::drawBottomBar(String hint)
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
        int y = MENU_START_Y + i * ITEM_H;
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
            mainOS->sprite.setTextSize(1);
            mainOS->sprite.setTextColor(TFT_WHITE);
            mainOS->sprite.setCursor(SCREEN_W - 14, y + 9);
            mainOS->sprite.print(">");
        }
    }
    drawBottomBar(";/.: Navigate  Enter: Select  `: Exit");
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
        uint32_t t = millis() - animTimer;
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
        drawBottomBar("`/ESC: Back");
        needsRedraw = true;
    }
    else
    {
        // Yakalanan sinyal bilgisi
        IRCommand &cmd = capturedSignal.commands[0];

        mainOS->sprite.fillCircle(14, 42, 7, COLOR_SUCCESS);
        mainOS->sprite.setTextColor(COLOR_BG);
        mainOS->sprite.setCursor(9, 38);
        mainOS->sprite.print("OK");

        mainOS->sprite.fillRoundRect(26, 28, SCREEN_W-30, 70, 5, 0x080808);
        mainOS->sprite.drawRoundRect(26, 28, SCREEN_W-30, 70, 5, COLOR_SUCCESS);

        // Protokol etiketi
        mainOS->sprite.fillRoundRect(30, 31, cmd.protocol.length()*6+8, 12, 3, 0x002288);
        mainOS->sprite.setTextColor(TFT_WHITE);
        mainOS->sprite.setCursor(34, 34);
        mainOS->sprite.print(cmd.protocol);

        if (cmd.type == "parsed")
        {
            mainOS->sprite.setTextColor(0x555555);
            mainOS->sprite.setCursor(30, 50);
            mainOS->sprite.print("Addr: ");
            mainOS->sprite.setTextColor(COLOR_SUCCESS);
            char buf[12];
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
            mainOS->sprite.print(cmd.rawData.size());
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

        // Alt butonlar
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

        drawBottomBar("`/ESC: Back (save prompt)");
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
        mainOS->sprite.print(cmd.protocol);

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
            mainOS->sprite.print(cmd.rawData.size());
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
    mainOS->sprite.print("N - No, Discard");
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
    if (disp.length() > 24) disp = disp.substring(disp.length() - 24);
    mainOS->sprite.print(disp);
    if ((millis() / 400) % 2 == 0)
    {
        int cx = 18 + disp.length() * 6;
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
        mainOS->sprite.print(cmd.protocol);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.print("  Type: ");
        mainOS->sprite.setTextColor(0x4488FF);
        mainOS->sprite.print(cmd.type);
    }

    mainOS->sprite.setTextColor(0x333333);
    mainOS->sprite.setCursor(14, 84);
    mainOS->sprite.print("File: /IR_Signals/");
    mainOS->sprite.setTextColor(0x555555);
    String fn = inputText.length() > 0 ? inputText : "?";
    fn.replace(" ", "_");
    mainOS->sprite.print(fn + ".ir");

    mainOS->sprite.fillRoundRect( 14, 100,  80, 14, 3, 0x003300);
    mainOS->sprite.drawRoundRect( 14, 100,  80, 14, 3, COLOR_SUCCESS);
    mainOS->sprite.setTextColor(COLOR_SUCCESS);
    mainOS->sprite.setCursor(20, 103);
    mainOS->sprite.print("ENTER: Save");

    mainOS->sprite.fillRoundRect(102, 100, 124, 14, 3, 0x220000);
    mainOS->sprite.drawRoundRect(102, 100, 124, 14, 3, COLOR_ACCENT);
    mainOS->sprite.setTextColor(COLOR_ACCENT);
    mainOS->sprite.setCursor(108, 103);
    mainOS->sprite.print("`/ESC: Cancel");

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

    // Protokol kutusu
    mainOS->sprite.fillRoundRect(2, 26, 90, 26, 4, 0x090909);
    mainOS->sprite.drawRoundRect(2, 26, 90, 26, 4, 0x0033AA);
    mainOS->sprite.setTextColor(0x334466);
    mainOS->sprite.setCursor(6, 29);
    mainOS->sprite.print("Protocol");
    mainOS->sprite.setTextColor(0x4488FF);
    mainOS->sprite.setCursor(6, 40);
    mainOS->sprite.print(cmd.protocol.length() > 0 ? cmd.protocol : "RAW");

    // Type kutusu
    mainOS->sprite.fillRoundRect(96, 26, 60, 26, 4, 0x090909);
    mainOS->sprite.drawRoundRect(96, 26, 60, 26, 4, 0x003300);
    mainOS->sprite.setTextColor(0x224422);
    mainOS->sprite.setCursor(100, 29);
    mainOS->sprite.print("Type");
    mainOS->sprite.setTextColor(COLOR_SUCCESS);
    mainOS->sprite.setCursor(100, 40);
    mainOS->sprite.print(cmd.type);

    // Freq kutusu
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
        // Address
        mainOS->sprite.fillRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 60);
        mainOS->sprite.print("Address: ");
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        char buf[32];
        sprintf(buf, "%02X %02X %02X %02X",
                cmd.address & 0xFF, (cmd.address>>8)&0xFF,
                (cmd.address>>16)&0xFF, (cmd.address>>24)&0xFF);
        mainOS->sprite.print(buf);

        // Command
        mainOS->sprite.fillRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 80);
        mainOS->sprite.print("Command: ");
        mainOS->sprite.setTextColor(0xCCCCCC);
        sprintf(buf, "%02X %02X %02X %02X",
                cmd.command & 0xFF, (cmd.command>>8)&0xFF,
                (cmd.command>>16)&0xFF, (cmd.command>>24)&0xFF);
        mainOS->sprite.print(buf);
    }
    else
    {
        // RAW bilgi
        mainOS->sprite.fillRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 56, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 60);
        mainOS->sprite.print("Pulses: ");
        mainOS->sprite.setTextColor(COLOR_WARNING);
        mainOS->sprite.print(cmd.rawData.size());

        // İlk birkaç raw değer
        mainOS->sprite.fillRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x080808);
        mainOS->sprite.drawRoundRect(2, 76, SCREEN_W-4, 16, 3, 0x222222);
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(6, 80);
        mainOS->sprite.print("Data: ");
        mainOS->sprite.setTextColor(0x666666);
        String preview = "";
        for (int i = 0; i < min((int)cmd.rawData.size(), 5); i++)
        {
            preview += String(cmd.rawData[i]);
            if (i < 4 && i < (int)cmd.rawData.size()-1) preview += " ";
        }
        if ((int)cmd.rawData.size() > 5) preview += "...";
        mainOS->sprite.print(preview);
    }

    // Dosya adı
    mainOS->sprite.setTextColor(0x333333);
    mainOS->sprite.setCursor(6, 100);
    mainOS->sprite.print("File: ");
    mainOS->sprite.setTextColor(0x555555);
    String fn = selectedSignal.filename;
    int ls = fn.lastIndexOf('/');
    if (ls >= 0) fn = fn.substring(ls + 1);
    if (fn.length() > 22) fn = fn.substring(0, 22) + "..";
    mainOS->sprite.print(fn);

    drawBottomBar("Enter/S: Send    `: Back");
}

// ================================================================
// SD BROWSER
// ================================================================

void Cardputer_Remote::drawSDBrowserScreen()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    drawTopBar("SD BROWSER (.ir)");
    mainOS->sprite.setFont(&fonts::Font0);
    mainOS->sprite.setTextSize(1);

    if (!sdAvailable)
    {
        mainOS->sprite.setTextColor(COLOR_ACCENT);
        mainOS->sprite.setCursor(SCREEN_W/2-48, 52);
        mainOS->sprite.print("SD Card not found!");
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(SCREEN_W/2-54, 66);
        mainOS->sprite.print("Insert card & restart");
        drawBottomBar("` : Back");
        return;
    }

    if (sdFiles.empty())
    {
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(SCREEN_W/2-54, 52);
        mainOS->sprite.print("No .ir files on SD");
        mainOS->sprite.setCursor(SCREEN_W/2-52, 66);
        mainOS->sprite.print("(all folders searched)");
        drawBottomBar("R: Refresh   ` : Back");
        return;
    }

    int visN = 4, itemH = 24, startY = 26;
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

        // İkon
        mainOS->sprite.setTextColor(sel ? selBar : (isM ? 0x443300 : 0x336633));
        mainOS->sprite.setCursor(6, y + 4);
        mainOS->sprite.print(isM ? "[M]" : ".ir");

        // Görünen ad
        String dn = sdFiles[idx].displayName;
        if (dn.length() > 23) dn = dn.substring(0, 23) + "..";
        mainOS->sprite.setTextColor(sel ? TFT_WHITE : 0x777777);
        mainOS->sprite.setCursor(32, y + 4);
        mainOS->sprite.print(dn);

        // Alt bilgi
        String sub = isM
                     ? (String(sdFiles[idx].cmdCount) + " cmds")
                     : (String(idx+1) + "/" + String(sdFiles.size()));
        mainOS->sprite.setTextColor(sel ? 0x557755 : 0x2A2A2A);
        mainOS->sprite.setCursor(SCREEN_W - sub.length()*6 - 6, y + 14);
        mainOS->sprite.print(sub);
    }

    // Scrollbar
    if ((int)sdFiles.size() > visN)
    {
        int sbH = visN * itemH, sbX = SCREEN_W-3, sbY = startY;
        mainOS->sprite.fillRect(sbX, sbY, 2, sbH, 0x111111);
        int th = sbH * visN / sdFiles.size();
        int ty = sbY + sbH * scrollOffset / sdFiles.size();
        mainOS->sprite.fillRect(sbX, ty, 2, th, COLOR_SUCCESS);
    }

    drawBottomBar(";/.: Nav  Ent:Open  V:Detail  R:Refresh  `:Back");
}

// ================================================================
// REMOTE PAD  (çoklu komut kumanda ekranı)
// ================================================================

void Cardputer_Remote::drawRemotePad()
{
    mainOS->sprite.fillScreen(COLOR_BG);
    String title = selectedSignal.name;
    if (title.length() > 16) title = title.substring(0, 16) + "..";
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

    // Kullanılabilir alan
    int areaX = 2, areaY = 24;
    int areaW = SCREEN_W - 4;
    int areaH = SCREEN_H - 24 - 15;   // 96 px

    // Sütun sayısı
    int cols = 3;
    if (cmdCount <= 2)  cols = cmdCount;
    if (cmdCount >= 9)  cols = 4;
    if (cmdCount == 1)  cols = 1;

    int rows    = (cmdCount + cols - 1) / cols;
    int btnW    = areaW / cols;
    int btnH    = min(areaH / max(rows, 1), 30);

    for (int i = 0; i < cmdCount; i++)
    {
        int col = i % cols;
        int row = i / cols;
        int bx  = areaX + col * btnW;
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
        if (lbl.length() > 7) lbl = lbl.substring(0, 7);
        int tx = bx + (bw - (int)lbl.length() * 6) / 2;
        int ty = by + (bh - 8) / 2;
        mainOS->sprite.setTextColor(sel ? COLOR_WARNING : 0x777777);
        mainOS->sprite.setCursor(tx, ty);
        mainOS->sprite.print(lbl);
    }

    // Seçili buton detayı
    if (remotePadIdx < cmdCount)
    {
        IRCommand &sc = selectedSignal.commands[remotePadIdx];
        mainOS->sprite.setTextColor(0x444444);
        mainOS->sprite.setCursor(4, SCREEN_H - 24);
        String info = sc.label + " [" + (sc.protocol.length()>0 ? sc.protocol : "RAW") + "]";
        if (sc.type == "parsed")
        {
            char buf[20];
            sprintf(buf, " A:%02X C:%02X", sc.address & 0xFF, sc.command & 0xFF);
            info += String(buf);
        }
        if (info.length() > 34) info = info.substring(0, 34);
        mainOS->sprite.print(info);
    }

    drawBottomBar(";/.,/: Nav  Enter:Send  `:Back");
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
        mainOS->sprite.setCursor(SCREEN_W/2-36, 82);
        mainOS->sprite.print("Transmitting...");
        needsRedraw = true;
    }
    else
    {
        mainOS->sprite.setTextColor(0x555555);
        mainOS->sprite.setCursor(14, 34);
        mainOS->sprite.print("Ready to send:");

        mainOS->sprite.setTextColor(TFT_WHITE);
        mainOS->sprite.setCursor(14, 46);
        String nm = selectedSignal.name;
        if (nm.length() > 26) nm = nm.substring(0, 26) + "..";
        mainOS->sprite.print(nm);

        if (!selectedSignal.commands.empty())
        {
            IRCommand &cmd = selectedSignal.commands[0];
            String pt = cmd.protocol.length() > 0 ? cmd.protocol : "RAW";
            int pw = pt.length() * 6 + 8;
            mainOS->sprite.fillRoundRect(14, 58, pw, 12, 3, 0x001A44);
            mainOS->sprite.setTextColor(0x4488FF);
            mainOS->sprite.setCursor(18, 61);
            mainOS->sprite.print(pt);

            mainOS->sprite.setTextColor(0x333333);
            mainOS->sprite.setCursor(14, 74);
            if (cmd.type == "parsed")
            {
                char buf[24];
                sprintf(buf, "A:%02X C:%02X", cmd.address & 0xFF, cmd.command & 0xFF);
                mainOS->sprite.setTextColor(0x555555);
                mainOS->sprite.print(buf);
            }
            else
            {
                mainOS->sprite.setTextColor(0x555555);
                mainOS->sprite.print("RAW ");
                mainOS->sprite.print(cmd.rawData.size());
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
        mainOS->sprite.print("`/ESC: Cancel");
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
    mainOS->sprite.print("Receiver : PIN G1  (fixed)");
    mainOS->sprite.setTextColor(0x444488);
    mainOS->sprite.setCursor(10, 44);
    mainOS->sprite.print("Format   : Flipper Zero .ir");

    mainOS->sprite.drawFastHLine(4, 66, SCREEN_W-8, 0x222222);
    mainOS->sprite.setTextColor(0x886600);
    mainOS->sprite.setCursor(10, 72);
    mainOS->sprite.print("Select IR Sender Pin:");

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

    drawBottomBar(",/.: Select  Enter: Apply  `: Back");
}

// ================================================================
// MASTER DRAW
// ================================================================

void Cardputer_Remote::drawScreen()
{
    switch (currentState)
    {
    case STATE_MAIN_MENU:    drawMainMenu();        break;
    case STATE_CAPTURE:      drawCaptureScreen();   break;
    case STATE_SAVE_PROMPT:  drawSavePrompt();      break;
    case STATE_CAPTURE_SAVE: drawSaveDialog();      break;
    case STATE_VIEW_CODE:    drawViewCodeScreen();  break;
    case STATE_SD_BROWSER:   drawSDBrowserScreen(); break;
    case STATE_SEND_CONFIRM: drawSendConfirm(false);break;
    case STATE_SETTINGS:     drawSettingsScreen();  break;
    case STATE_REMOTE_PAD:   drawRemotePad();       break;
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

    if (irResults.value != 0xFFFFFFFF &&
        irResults.value != 0x0 &&
        millis() - lastCapture > 300)
    {
        // IRCommand oluştur
        IRCommand cmd;
        cmd.label    = "BTN";
        cmd.frequency= 38000;
        cmd.dutyCycle= 0.33f;

        bool isRaw = (irResults.decode_type == UNKNOWN);

        if (isRaw)
        {
            cmd.type     = "raw";
            cmd.protocol = "";
            cmd.address  = 0;
            cmd.command  = 0;
            for (int i = 1; i < irResults.rawlen && i < 512; i++)
                cmd.rawData.push_back((uint16_t)(irResults.rawbuf[i] * RAWTICK));
        }
        else
        {
            cmd.type     = "parsed";
            cmd.protocol = protoName(irResults.decode_type);
            // Flipper format: address = low byte, command = next byte
            // IRremote value genellikle: [addr][~addr][cmd][~cmd]  (NEC için)
            uint32_t val = (uint32_t)irResults.value;
            cmd.address  = val & 0xFF;
            cmd.command  = (val >> 8) & 0xFF;
        }

        capturedSignal.name     = "IR_" + String(millis() % 100000);
        capturedSignal.filename = "";
        capturedSignal.isMulti  = false;
        capturedSignal.isRaw    = isRaw;
        capturedSignal.commands.clear();
        capturedSignal.commands.push_back(cmd);

        // Hızlı erişim alanları
        capturedSignal.protocol = cmd.protocol;
        capturedSignal.address  = cmd.address;
        capturedSignal.command  = cmd.command;
        capturedSignal.frequency= cmd.frequency;
        capturedSignal.rawData  = cmd.rawData;

        signalCaptured = true;
        lastCapture    = millis();
        capturing      = false;
        irRecv.pause();
        needsRedraw = true;
    }
    irRecv.resume();
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
            irRecv.resume();
            animTimer = millis();
            break;
        case 1:
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
        if (signalCaptured) currentState = STATE_SAVE_PROMPT;
        else { capturing = false; irRecv.pause(); currentState = STATE_MAIN_MENU; }
    }
    else if (signalCaptured)
    {
        if (key == 's' || key == 'S')
        {
            if (sdAvailable) { currentState = STATE_CAPTURE_SAVE; inputText = ""; }
            else setStatus("SD Card not available!");
        }
        else if (key == 't' || key == 'T')
        {
            setStatus(sendIR(capturedSignal) ? "Signal sent!" : "Send failed!");
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
            irRecv.resume();
            animTimer = millis();
        }
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
        capturing = false; irRecv.pause();
        signalCaptured = false;
        currentState = STATE_MAIN_MENU;
    }
}

void Cardputer_Remote::handleSaveDialog(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = STATE_CAPTURE;
    }
    else if (key == '\n' || key == '\r')
    {
        if (inputText.length() == 0) { setStatus("Name cannot be empty!"); return; }

        // İsmi ve label'ı güncelle
        capturedSignal.name = inputText;
        if (!capturedSignal.commands.empty())
            capturedSignal.commands[0].label = inputText;

        if (saveSignalToSD(capturedSignal))
        {
            // sdFiles listesini güncelle
            scanSDFiles();
            setStatus("Saved: " + inputText + ".ir");
            currentState   = STATE_MAIN_MENU;
            signalCaptured = false;
            capturing      = false;
            inputText      = "";
        }
        else setStatus("Save failed!");
    }
    else if (key == 8 || key == 127)
    {
        if (inputText.length() > 0) inputText.remove(inputText.length() - 1);
    }
    else if (key >= 32 && key <= 126 && inputText.length() < 20)
    {
        inputText += key;
    }
}

void Cardputer_Remote::handleViewCode(char key)
{
    if (key == '`' || key == 27)
        currentState = previousState;
    else if (key == 's' || key == 'S' || key == '\n' || key == '\r')
        setStatus(sendIR(selectedSignal) ? "Signal sent!" : "Send failed!");
}

void Cardputer_Remote::handleSDBrowser(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = STATE_MAIN_MENU;
    }
    else if (key == ';' && sdFileIndex > 0)
    {
        sdFileIndex--;
    }
    else if (key == '.' && sdFileIndex < (int)sdFiles.size()-1)
    {
        sdFileIndex++;
    }
    else if ((key == '\n' || key == '\r' || key == ' ') && !sdFiles.empty())
    {
        IRSignal sig;
        if (loadSignalFromSD(sdFiles[sdFileIndex].path, sig))
        {
            selectedSignal = sig;
            previousState  = STATE_SD_BROWSER;
            if (sig.isMulti && sig.commands.size() > 1)
            {
                remotePadIdx = 0;
                currentState = STATE_REMOTE_PAD;
            }
            else currentState = STATE_SEND_CONFIRM;
        }
        else setStatus("Failed to load!");
    }
    else if ((key == 'v' || key == 'V') && !sdFiles.empty())
    {
        IRSignal sig;
        if (loadSignalFromSD(sdFiles[sdFileIndex].path, sig))
        {
            selectedSignal = sig;
            previousState  = STATE_SD_BROWSER;
            currentState   = STATE_VIEW_CODE;
        }
        else setStatus("Failed to load!");
    }
    else if (key == 'r' || key == 'R')
    {
        scanSDFiles();
        sdFileIndex  = 0;
        scrollOffset = 0;
        setStatus(String(sdFiles.size()) + " files found");
    }
}

void Cardputer_Remote::handleSendConfirm(char key)
{
    if (key == '`' || key == 27)
    {
        currentState = previousState;
    }
    else if (key == '\n' || key == '\r' || key == ' ' || key == 's' || key == 'S')
    {
        mainOS->sprite.fillScreen(COLOR_BG);
        drawTopBar("TRANSMIT");
        drawSendConfirm(true);
        drawToast();
        mainOS->sprite.pushSprite(0, 0);
        bool ok = sendIR(selectedSignal);
        delay(400);
        setStatus(ok ? "Sent successfully!" : "Send failed!");
        currentState = previousState;
    }
}

void Cardputer_Remote::handleSettings(char key)
{
    if (key == '`' || key == 27)
        currentState = STATE_MAIN_MENU;
    else if (key == ';' || key == ',')
        settingsIndex = 0;
    else if (key == '.' || key == '/')
        settingsIndex = 1;
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        if (settingsIndex == 0 && !usePin44)
        {
            usePin44 = true; sendPin = 44;
            reinitIRSend();
            setStatus("Sender pin set to G44");
        }
        else if (settingsIndex == 1 && usePin44)
        {
            usePin44 = false; sendPin = 2;
            reinitIRSend();
            setStatus("Sender pin set to G2");
        }
        else setStatus(usePin44 ? "G44 already active" : "G2 already active");
    }
}

void Cardputer_Remote::handleRemotePad(char key)
{
    int cmdCount = (int)selectedSignal.commands.size();
    if (cmdCount == 0) { currentState = previousState; return; }

    int cols = 3;
    if (cmdCount <= 2) cols = cmdCount;
    if (cmdCount >= 9) cols = 4;
    if (cmdCount == 1) cols = 1;

    if (key == '`' || key == 27)
        currentState = previousState;
    else if (key == ',')   // Sol
    {
        if (remotePadIdx % cols > 0) remotePadIdx--;
    }
    else if (key == '/')   // Sağ
    {
        if (remotePadIdx % cols < cols-1 && remotePadIdx+1 < cmdCount)
            remotePadIdx++;
    }
    else if (key == ';')   // Yukarı
    {
        if (remotePadIdx - cols >= 0) remotePadIdx -= cols;
    }
    else if (key == '.')   // Aşağı
    {
        if (remotePadIdx + cols < cmdCount) remotePadIdx += cols;
    }
    else if (key == '\n' || key == '\r' || key == ' ')
    {
        // Gönder animasyonu
        mainOS->sprite.fillScreen(COLOR_BG);
        drawTopBar("TRANSMIT");
        mainOS->sprite.setFont(&fonts::Font0);
        mainOS->sprite.setTextSize(2);
        mainOS->sprite.setTextColor(COLOR_SUCCESS);
        mainOS->sprite.setCursor(80, 52);
        mainOS->sprite.print(">>>");
        mainOS->sprite.setTextSize(1);
        mainOS->sprite.setTextColor(0x888888);
        mainOS->sprite.setCursor(50, 82);
        String lbl = selectedSignal.commands[remotePadIdx].label;
        mainOS->sprite.print("Sending: " + lbl);
        mainOS->sprite.pushSprite(0, 0);

        bool ok = sendIRCommand(selectedSignal.commands[remotePadIdx]);
        delay(200);
        setStatus(ok ? (lbl + " sent!") : "Send failed!");
        needsRedraw = true;
    }
}

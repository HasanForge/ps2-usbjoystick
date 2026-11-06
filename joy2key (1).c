/*
 * joy2key.c
 *
 * Polls a USB/PS2 joystick via the Windows legacy Joystick API (winmm)
 * and simulates keyboard key presses for its buttons and main axes.
 *
 * Bindings come from joy2key.ini, read from the same folder as the exe.
 * If that file doesn't exist, a default one is written out on first run
 * so you can rebind keys by editing a text file -- no rebuild needed.
 *
 * Build (32-bit, matches an old Win7 32-bit box):
 *   i686-w64-mingw32-gcc joy2key.c -o joy2key.exe -lwinmm -static
 */

#include <windows.h>
#include <joystickapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define NUM_BUTTONS 12
#define INI_NAME "joy2key.ini"

static WORD buttonVk[NUM_BUTTONS];
static WORD axisLeftVk  = VK_LEFT;
static WORD axisRightVk = VK_RIGHT;
static WORD axisUpVk    = VK_UP;
static WORD axisDownVk  = VK_DOWN;
static int  axisLow  = 20000;
static int  axisHigh = 45000;

typedef struct { const char *name; WORD vk; } VkName;
static VkName vkNames[] = {
    {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"RETURN", VK_RETURN},
    {"ESC", VK_ESCAPE}, {"ESCAPE", VK_ESCAPE}, {"TAB", VK_TAB},
    {"BACKSPACE", VK_BACK}, {"SHIFT", VK_SHIFT}, {"LSHIFT", VK_LSHIFT},
    {"RSHIFT", VK_RSHIFT}, {"CTRL", VK_CONTROL}, {"LCTRL", VK_LCONTROL},
    {"LCONTROL", VK_LCONTROL}, {"RCTRL", VK_RCONTROL}, {"RCONTROL", VK_RCONTROL},
    {"ALT", VK_MENU}, {"LALT", VK_LMENU}, {"RALT", VK_RMENU},
    {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
    {"NONE", 0}, {"UNBOUND", 0},
};
#define NUM_VKNAMES (sizeof(vkNames) / sizeof(vkNames[0]))

static WORD vkFromName(const char *name) {
    for (size_t i = 0; i < NUM_VKNAMES; i++)
        if (_stricmp(name, vkNames[i].name) == 0) return vkNames[i].vk;
    if (strlen(name) == 1) {
        char c = (char)toupper((unsigned char)name[0]);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return (WORD)c;
    }
    return 0;
}

static void writeDefaultIni(void) {
    FILE *f = fopen(INI_NAME, "w");
    if (!f) return;
    fprintf(f,
        "# joy2key bindings. Edit the key name after each '=', save, rerun joy2key.exe.\n"
        "# Key names: SPACE ENTER ESC TAB SHIFT LSHIFT RSHIFT CTRL LCTRL RCTRL\n"
        "#            ALT LALT RALT UP DOWN LEFT RIGHT, any single letter/digit, NONE\n"
        "button1=SPACE\n"
        "button2=LCTRL\n"
        "button3=Z\n"
        "button4=X\n"
        "button5=NONE\n"
        "button6=NONE\n"
        "button7=NONE\n"
        "button8=NONE\n"
        "button9=NONE\n"
        "button10=NONE\n"
        "button11=NONE\n"
        "button12=NONE\n"
        "axis_left=LEFT\n"
        "axis_right=RIGHT\n"
        "axis_up=UP\n"
        "axis_down=DOWN\n"
        "axis_low=20000\n"
        "axis_high=45000\n"
    );
    fclose(f);
}

static void loadIni(void) {
    buttonVk[0] = VK_SPACE; buttonVk[1] = VK_LCONTROL;
    buttonVk[2] = 'Z';      buttonVk[3] = 'X';
    for (int i = 4; i < NUM_BUTTONS; i++) buttonVk[i] = 0;

    FILE *f = fopen(INI_NAME, "r");
    if (!f) {
        writeDefaultIni();
        f = fopen(INI_NAME, "r");
        if (!f) return;
    }

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0' || *p == '\r' || *p == '\n') continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        char *end = key + strlen(key) - 1;
        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';

        int n;
        if (sscanf(key, "button%d", &n) == 1 && n >= 1 && n <= NUM_BUTTONS) {
            buttonVk[n - 1] = vkFromName(val);
        } else if (_stricmp(key, "axis_left") == 0) {
            axisLeftVk = vkFromName(val);
        } else if (_stricmp(key, "axis_right") == 0) {
            axisRightVk = vkFromName(val);
        } else if (_stricmp(key, "axis_up") == 0) {
            axisUpVk = vkFromName(val);
        } else if (_stricmp(key, "axis_down") == 0) {
            axisDownVk = vkFromName(val);
        } else if (_stricmp(key, "axis_low") == 0) {
            axisLow = atoi(val);
        } else if (_stricmp(key, "axis_high") == 0) {
            axisHigh = atoi(val);
        }
    }
    fclose(f);
}

static void sendKey(WORD vk, int down) {
    if (!vk) return;
    keybd_event((BYTE)vk, 0, down ? 0 : KEYEVENTF_KEYUP, 0);
}

int main(void) {
    loadIni();

    JOYINFOEX ji;
    ji.dwSize = sizeof(JOYINFOEX);
    ji.dwFlags = JOY_RETURNALL;

    if (joyGetNumDevs() == 0) {
        printf("No joystick driver present on this system.\n");
        return 1;
    }
    if (joyGetPosEx(JOYSTICKID1, &ji) != JOYERR_NOERROR) {
        printf("No joystick found at JOYSTICKID1.\n");
        printf("Check it shows up and responds in Control Panel's Game Controllers first.\n");
        return 1;
    }

    printf("joy2key running with bindings from %s.\n", INI_NAME);
    printf("Close this console window to quit.\n");

    DWORD prevButtons = 0;
    int prevLeft = 0, prevRight = 0, prevUp = 0, prevDown = 0;

    for (;;) {
        if (joyGetPosEx(JOYSTICKID1, &ji) == JOYERR_NOERROR) {
            for (int i = 0; i < NUM_BUTTONS; i++) {
                DWORD mask = 1u << i;
                int now = (ji.dwButtons & mask) != 0;
                int was = (prevButtons & mask) != 0;
                if (now && !was) sendKey(buttonVk[i], 1);
                if (!now && was) sendKey(buttonVk[i], 0);
            }
            prevButtons = ji.dwButtons;

            int left  = (int)ji.dwXpos < axisLow;
            int right = (int)ji.dwXpos > axisHigh;
            if (left  && !prevLeft)   sendKey(axisLeftVk, 1);
            if (!left &&  prevLeft)   sendKey(axisLeftVk, 0);
            if (right && !prevRight)  sendKey(axisRightVk, 1);
            if (!right &&  prevRight) sendKey(axisRightVk, 0);
            prevLeft = left; prevRight = right;

            int up   = (int)ji.dwYpos < axisLow;
            int down = (int)ji.dwYpos > axisHigh;
            if (up   && !prevUp)    sendKey(axisUpVk, 1);
            if (!up  &&  prevUp)    sendKey(axisUpVk, 0);
            if (down && !prevDown)  sendKey(axisDownVk, 1);
            if (!down &&  prevDown) sendKey(axisDownVk, 0);
            prevUp = up; prevDown = down;
        }
        Sleep(10);
    }

    return 0;
}

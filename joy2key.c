/*
 * joy2key.c
 *
 * Polls a USB/PS2 joystick via the Windows legacy Joystick API (winmm)
 * and simulates keyboard key presses for its buttons and main axes.
 *
 * Build (32-bit, matches an old Win7 32-bit box):
 *   i686-w64-mingw32-gcc joy2key.c -o joy2key.exe -lwinmm -static
 *
 * Run: just launch joy2key.exe, then focus whatever window/game should
 * receive the simulated key presses. Close the console window to quit.
 */

#include <windows.h>
#include <joystickapi.h>
#include <stdio.h>
#include <stddef.h>

typedef struct { DWORD joyButtonMask; WORD vk; } ButtonMap;

/* ---- Edit this table to change which joystick button fires which key ---- */
static ButtonMap buttonMap[] = {
    { JOY_BUTTON1, VK_SPACE    },  /* button 1 -> Space      */
    { JOY_BUTTON2, VK_LCONTROL },  /* button 2 -> Left Ctrl  */
    { JOY_BUTTON3, 'Z'         },  /* button 3 -> Z          */
    { JOY_BUTTON4, 'X'         },  /* button 4 -> X          */
};
#define NUM_BUTTONS (sizeof(buttonMap) / sizeof(buttonMap[0]))

/* Axis thresholds: raw range is 0-65535, midpoint ~32767.
   Tune these if your stick's rest position isn't centered. */
#define AXIS_LOW  20000
#define AXIS_HIGH 45000

static void sendKey(WORD vk, int down) {
    keybd_event((BYTE)vk, 0, down ? 0 : KEYEVENTF_KEYUP, 0);
}

int main(void) {
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

    printf("joy2key running. Close this console window to quit.\n");

    DWORD prevButtons = 0;
    int prevLeft = 0, prevRight = 0, prevUp = 0, prevDown = 0;

    for (;;) {
        if (joyGetPosEx(JOYSTICKID1, &ji) == JOYERR_NOERROR) {
            /* Buttons */
            for (size_t i = 0; i < NUM_BUTTONS; i++) {
                DWORD mask = buttonMap[i].joyButtonMask;
                int now = (ji.dwButtons & mask) != 0;
                int was = (prevButtons & mask) != 0;
                if (now && !was) sendKey(buttonMap[i].vk, 1);
                if (!now && was) sendKey(buttonMap[i].vk, 0);
            }
            prevButtons = ji.dwButtons;

            /* X axis -> Left/Right arrows */
            int left  = ji.dwXpos < AXIS_LOW;
            int right = ji.dwXpos > AXIS_HIGH;
            if (left  && !prevLeft)  sendKey(VK_LEFT, 1);
            if (!left &&  prevLeft)  sendKey(VK_LEFT, 0);
            if (right && !prevRight) sendKey(VK_RIGHT, 1);
            if (!right &&  prevRight) sendKey(VK_RIGHT, 0);
            prevLeft = left; prevRight = right;

            /* Y axis -> Up/Down arrows */
            int up   = ji.dwYpos < AXIS_LOW;
            int down = ji.dwYpos > AXIS_HIGH;
            if (up   && !prevUp)   sendKey(VK_UP, 1);
            if (!up  &&  prevUp)   sendKey(VK_UP, 0);
            if (down && !prevDown) sendKey(VK_DOWN, 1);
            if (!down &&  prevDown) sendKey(VK_DOWN, 0);
            prevUp = up; prevDown = down;
        }
        Sleep(10); /* ~100 polls/sec */
    }

    return 0;
}

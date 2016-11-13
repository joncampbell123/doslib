
#ifndef TARGET_OS2
# error This is OS-2 code, not DOS or Windows
#endif

#define INCL_GPIPRIMITIVES
#define INCL_WINHELP
#define INCL_WIN
#define INCL_DDF

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include <os2.h>

#define ID_PRIMWIN 100

MRESULT EXPENTRY HelloWndProc(HWND,USHORT,MPARAM,MPARAM);

HWND HelloWnd=0;
HWND HelloWndClient=0;

int main(int argc,char **argv) {
    ULONG flFrameFlags = FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST | FCF_SIZEBORDER | FCF_SHELLPOSITION | FCF_MINBUTTON | FCF_MAXBUTTON;
    QMSG qmsg;
    HAB hab;
    HMQ hmq;

    hab = WinInitialize(0);
    if (hab == (HAB)0) return 1;

    hmq = WinCreateMsgQueue(hab,0);
    if (hmq == (HMQ)0) return 1;

    WinRegisterClass(hab,"HelloWnd",(PFNWP)HelloWndProc,CS_SIZEREDRAW,0);

    HelloWnd = WinCreateStdWindow(HWND_DESKTOP,WS_VISIBLE,&flFrameFlags,"HelloWnd","Hello world",0L,0/*NULLHANDLE*/,ID_PRIMWIN,&HelloWndClient);
    if (HelloWnd == (HWND)0 || HelloWndClient == (HWND)0) return 1;

    while (WinGetMsg(hab,&qmsg,0/*NULLHANDLE*/,0,0))
        WinDispatchMsg(hab,&qmsg);

    WinDestroyWindow(HelloWnd);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    return 0;
}

MRESULT EXPENTRY HelloWndProc(HWND hwnd,USHORT msg,MPARAM mp1,MPARAM mp2) {
    switch (msg) {
        case WM_PAINT: {
            static char *message = "Hello world! I'm running under OS/2!";
            POINTL pt;
            HPS hps;

            /* where to put it. NTS: OS/2 window coordinates are done Cartesian style where Y increases upward on the screen */
            pt.x = 40;
            pt.y = 40;

            hps = WinBeginPaint(hwnd,0/*NULLHANDLE*/,NULL);
            GpiErase(hps);
            GpiCharStringAt(hps,&pt,strlen(message),message);
            WinEndPaint(hps);
        } return 0;
    }

    return WinDefWindowProc(hwnd,msg,mp1,mp2);
}


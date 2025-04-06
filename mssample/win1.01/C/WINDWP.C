/*/
 *  windwp.c
 *  DefWindowProc
 *  Windows Beta Release
 *  Copyright (c) Microsoft 1985
/*/

/*% MOVEABLE DS - SL */

#define LSTRING
#define NOCLIPBOARD
#define NOMENUS
#define NOCLIPBOARD
#define NOCTLMGR
#define NOSYSSTRUCTS
#include "user.h"

#define SYS_ALTERNATE 0x2000

extern SCRN scrn;
extern HWND hwndKeyCvt;
extern BOOL fMessageBox;

extern HWND hwndActive;
extern HWND hwndActivePrev;

BOOL far DefSetText(HWND, LPSTR);
void FAR EndScroll(HWND, BOOL);
void far PaintIconicWindow();
void far HideIconName();
void far ShowIconName();
HWND far FarDeliberateIconName(POINT);
void FAR FillWindow();
int FAR InWnd();

long far DefWindowProc(hwnd, message, wParam, lParam)
register HWND hwnd;
unsigned message;
register WORD wParam;
LONG lParam;
{
    HDC hdc;
    int cch;
    PAINTSTRUCT ps;
    RECT rc;
    HBRUSH hbr;
    extern BOOL fMenu;
    void far HideCaret2();
    void far ShowCaret2();
    int * far InitPwSB(HWND);
    extern CARET currentCaret;
    extern SYSCOLORS sysColors;
    extern SYSCLROBJECTS sysClrObjects;
    extern int FAR VKConvert();
    extern HWND hwndDragging;
    extern HWND hwndCapture;
    extern HWND hwndFocus;

    if (!CheckHwnd(hwnd))
        return((DWORD)FALSE);

    switch (message) {
    case WM_CANCELMODE:
        EndScroll(hwnd, TRUE);
        /* if the capture is still set, just release at this point.
         * Can put other End* functions in later.
         */
        if (hwnd == hwndCapture)
            ReleaseCapture();
        break;

    case WM_NCCREATE:
        if (TestWF(hwnd, (WFHSCROLL | WFVSCROLL))) {
            if (InitPwSB(hwnd) == NULL)
                return((long)FALSE);
        }
        return((long)DefSetText(hwnd, ((LPCREATESTRUCT)lParam)->lpszName));
        break;

    case WM_NCDESTROY:
        if (hwnd->spWndName)
            FreeP((HANDLE)(hwnd->spWndName));
        if (hwnd->wndPwScroll)
            FreeP(hwnd->wndPwScroll);
        break;

    case WM_NCCALCSIZE:
        CompClient2(hwnd, (LPRECT)lParam);
        break;

    case WM_SETTEXT:
        DefSetText(hwnd, (LPSTR)lParam);
        if ((TestWF(hwnd, WFBORDERMASK) == (BYTE)LOBYTE(WFCAPTION)) && IsWindowVisible(hwnd)) {
            hdc = (HDC)GetWindowDC(hwnd);
            if (TestWF(hwnd, WFBORDERMASK) != 0) {
                GetWndRect(hwnd, (LPRECT)&rc);
                DrawFrame(hdc, (LPRECT)&rc, 0);
            }
            DrawCaption(hwnd, hdc);
            ReleaseADc(hdc);
        }
        break;

    case WM_GETTEXT:
        cch = 0;
        if (hwnd->spWndName && wParam != 0) {
            cch = min(lstrlen((LPSTR)hwnd->spWndName), wParam - 1);
            LCopyStruct((LPSTR)hwnd->spWndName, (LPSTR)lParam, cch);
            ((LPSTR)lParam)[cch] = 0;
        }
        return((long)cch);
        break;

    case WM_GETTEXTLENGTH:
        if (hwnd->spWndName)
            return(lstrlen((LPSTR)hwnd->spWndName));
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;

    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
        WINInput(hwnd, message, wParam, lParam);
        break;

    case WM_NCHITTEST:
        return((long)InWnd(hwnd, lParam));
        break;

    case WM_PAINT:
        BeginPaint(hwnd, (LPPAINTSTRUCT)&ps);
        EndPaint(hwnd, (LPPAINTSTRUCT)&ps);
        break;

    case WM_NCPAINT:
        DrawWindowFrame(hwnd);
        break;

    case WM_ERASEBKGND:
        if ((hbr = hwnd->spWndCls->hBackBrush) != NULL) {
            if ((unsigned)hbr <= COLOR_CAPTIONTEXT + 1)
                hbr = *(((HBRUSH *)&sysClrObjects) + ((unsigned)hbr) - 1);
            /* DC Org is already set */
            UnrealizeObject(hbr);
            FillWindow(hwnd, hwnd, (HDC)wParam, hbr);
            return((LONG)TRUE);
        }
        break;

    case WM_QUERYOPEN:
    case WM_QUERYENDSESSION:
        return((long)TRUE);
        break;

    case WM_SYSCOMMAND:
        SysCommand(hwnd, wParam, lParam);
        break;

    case WM_SYSKEYDOWN:
        if (hwndDragging == NULL) {
            /* Special case stuff */
            /* if syskey is down */
            if (HIWORD(lParam) & SYS_ALTERNATE) {
                if (hwndActive != NULL) {
                    switch (wParam) {
                    case VK_RETURN:
                        SendMessage(hwndActive, WM_SYSCOMMAND, SC_ZOOM, 0L);
                        break;
                    case VK_TAB:
                        SendMessage(hwndActive, WM_SYSCOMMAND,
                                (GetKeyState(VK_SHIFT) < 0 ? SC_NEXTWINDOW : SC_PREVWINDOW),
                                0L);
                        break;
                    }
                }
            }
        }
        break;

    case WM_SYSCHAR:
        /* If syskey is down and we have a char */
        if ((HIWORD(lParam) & SYS_ALTERNATE) && wParam) {
                SendMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, (DWORD)wParam);
        }
        break;

    case WM_NCACTIVATE:
        if (TestWF(hwnd, WFTOPLEVEL)) {
            if (wParam) {
                SetWF(hwnd, WFFRAMEON);
            } else {
                ClrWF(hwnd, WFFRAMEON);
            }
            if (TestWF(hwnd, WFVISIBLE) && wParam != 2) {
                if (IsIconic(hwnd)) {
                    PaintIconicWindow(hwnd, FALSE);
                } else {
                    hdc = (HDC)GetWindowDC(hwnd);
                    DrawCaption(hwnd, hdc);
                    ReleaseADc(hdc);
                }
            }
        }
        return((long)TRUE);
        break;

    case WM_ACTIVATE:
        if (wParam && !HIWORD(lParam)) {
            SetFocus(hwnd);
        }
        break;

    case WM_SETREDRAW:
        SetRedraw(hwnd, wParam);
        break;

    case WM_SHOWWINDOW:
        /* Non null show window descriptor = implied popup hide or show */
        if (LOWORD(lParam) != 0 && TestwndPopup(hwnd)) {
            /* if NOT(showing, invisible, and not set as hidden) AND
            /* NOT(hiding and not visible) */
            if (!(wParam != 0 && !TestWF(hwnd, WFVISIBLE) && !TestWF(hwnd, WFHIDDENPOPUP)) &&
                    !(wParam == 0 && !TestWF(hwnd, WFVISIBLE))) {
                if (wParam) {
                    /* if showing, clear the hidden popup flag */
                    ClrWF(hwnd, WFHIDDENPOPUP);
                } else {
                    /* if hiding, set the hidden popup flag */
                    SetWF(hwnd, WFHIDDENPOPUP);
                }
                ShowWindow(hwnd, (wParam ? SHOW_OPENNOACTIVATE : HIDE_WINDOW));
            }
        }
        break;

    case WM_CTLCOLOR:
        if (HIWORD(lParam) != CTLCOLOR_SCROLLBAR) {
            SetBkColor((HDC)wParam, sysColors.clrWindow);
            SetTextColor((HDC)wParam, sysColors.clrWindowText);
            hbr = sysClrObjects.hbrWindow;
        } else {
            SetBkColor((HDC)wParam, sysColors.clrCaptionText);
            SetTextColor((HDC)wParam, sysColors.clrCaption);
            hbr = sysClrObjects.hbrScrollbar;
        }
        /* This brush should not be selected anywhere at this moment */
        UnrealizeObject(hbr);
        SetBrushOrg((HDC)wParam, hwnd->clientRect.left, hwnd->clientRect.top);
        return((DWORD)hbr);
        break;

    case WM_MOVECONVERTWINDOW:
        if ((hwndKeyCvt != NULL) && !fMessageBox)
            MoveWindow(hwndKeyCvt, 0, scrn.screenHeight - oemInfo.kanjiSCnt ,
                                  scrn.screenWidth, oemInfo.kanjiSCnt , FALSE);
        break;
    }
    return(0L);
}

BOOL far DefSetText(hwnd, lpsz)
register HWND hwnd;
LPSTR lpsz;
{
    char *sz;
    register int cch;

    if (hwnd->spWndName != NULL)
        hwnd->spWndName = FreeP(hwnd->spWndName);
    if (lpsz != NULL) {
        cch = lstrlen((LPSTR)lpsz) + 1;
        /* Map HIWORD of lpsz to DS handle (in case DS moves) */
        *(((HANDLE far *)&lpsz) + 1) = (HANDLE)LOWORD(GlobalHandle(HIWORD(lpsz)));
        if ((sz = (char *)AllocP(cch)) == NULL)
            return(FALSE);      /* out of memory */
        hwnd->spWndName = sz;
        /* Map HIWORD of lpsz from DS Handle to DS pointer */
        *(((HANDLE far *)&lpsz) + 1) = (HANDLE)HIWORD(GlobalHandle(HIWORD(lpsz)));
        LCopyStruct(lpsz, (LPSTR)(hwnd->spWndName), cch);
    }
    return(TRUE);
}


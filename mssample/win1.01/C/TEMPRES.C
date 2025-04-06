/*  TempRES.c - Resident portion of application
    Template Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "template.h"

extern HANDLE  hAccelTable;
extern HWND    hWndTemplate;

/******* The code below is for demo purposes only *******/
extern BOOL    fDoFlash;
extern FARPROC lpprocTimerFlash;

FAR PASCAL TimerAsync(hWnd, message, id, time)
HWND       hWnd;
unsigned   message;
WORD       id;
DWORD      time;
{
    /* this is in the resident segment to prevent a disk i/o when invoked */
    KillTimer(hWnd, IDTASYNC);
    if (GetFocus() == hWnd)
        ShowAsyncMessage(hWnd);
    else {
        fDoFlash = TRUE;
        SetTimer(hWnd, IDTFLASH, CMSECFLASH, lpprocTimerFlash);
        FlashWindow(hWnd, TRUE);
        }
    return(FALSE);
}


FAR PASCAL TimerFlash(hWnd, message, id, time)
HWND       hWnd;
unsigned   message;
WORD       id;
DWORD      time;
{
    /* this is in the resident segment since it is called frequently */
    FlashWindow(hWnd, TRUE);
}
/******* The code above is for demo purposes only *******/


long FAR PASCAL TemplateWndProc(hWnd, message, wParam, lParam)
HWND       hWnd;
unsigned   message;
WORD       wParam;
LONG       lParam;
{
    /* Procedures which make up the window class. */
    PAINTSTRUCT  ps;

    switch (message)
    {
        case WM_CREATE:
            /* Window's being created; lParam contains lpParam field
            ** passed to CreateWindow */
            TemplateCreate(hWnd,lParam);
            break;

        case WM_DESTROY:
            /* Window's being destroyed. */
            PostQuitMessage(0);     /* Cause application to be terminated */
            break;

        case WM_MOVE:
            /* Window has been moved.  lParam contains the new position */
            TemplateMove(hWnd, MAKEPOINT(lParam).x, MAKEPOINT(lParam).y);
            break;

        case WM_SIZE:
            /* Window's size is changing.  lParam contains the width
            ** and height, in the low and high words, respectively.
            ** wParam contains SIZENORMAL for "normal" size changes,
            ** SIZEICONIC when the window is being made iconic, and
            ** SIZEFULLSCREEN when the window is being made full screen. */
            TemplateSize(hWnd, MAKEPOINT(lParam).x, MAKEPOINT(lParam).y,
                             wParam);
            break;

        case WM_ACTIVATE:
            /* Window is becoming active window if wParam is TRUE,
            ** inactive if wParam is FALSE.  LOWORD(lParam) is other window
            ** changing state.  HIWORD(lParam) is TRUE if hWnd is iconic.
            ** If not iconic then set focus to receive keyboard input */
            if (wParam && !HIWORD(lParam))
                SetFocus(hWnd);
            break;

        case WM_SETFOCUS:
            /* The window is getting the focus.  wParam contains the window
            ** handle of the window that previously had the focus. */
            TemplateGetFocus(hWnd, (HWND)wParam);
            break;

        case WM_KILLFOCUS:
            /* The window is losing the focus.  wParam contains the window
            ** handle of the window about to get the focus, or NULL. */
            TemplateLoseFocus(hWnd, (HWND)wParam);
            break;

        case WM_PAINT:
            /* Time for the window to draw itself. */
            BeginPaint(hWnd, (LPPAINTSTRUCT)&ps);
            TemplatePaint(hWnd, (LPPAINTSTRUCT)&ps);
            EndPaint(hWnd, (LPPAINTSTRUCT)&ps);
            break;

        case WM_CLOSE:
            /* The system menu command Close has been invoked.  To
            ** continue closing down, invoke DestroyWindow else
            ** do nothing */
            TemplateClose(hWnd);
            break;

        case WM_QUERYENDSESSION:
            /* The DOS command End Session has been invoked.  To
            ** continue return TRUE else return FALSE; */
            return((long)TemplateQueryEndSession(hWnd));
            break;

        case WM_ENDSESSION:
            /* The DOS command End Session has been invoked and
            ** all applications have returned TRUE to WM_QUERYENDSESSION */
            TemplateEndSession(hWnd, wParam);
            break;

        case WM_ERASEBKGND:
            /* Since the hbrBackground is set to NULL in registering the class
            ** the application is responsible for erasing and painting the
            ** background color on demand */
            TemplateEraseBkgnd(hWnd, (HDC)wParam);
            break;

        case WM_KEYUP:
        case WM_KEYDOWN:
            /* Virtual key input.  wParam is the virtual key.  Most
            ** applications will NOT process virtual keys but will
            ** instead wait for the ASCII to arrive via WM_CHAR generated
            ** by TranslateMessage.  If HIWORD(lParam) < 0 then key is
            ** down, but this is redundant info since message indicated
            ** whether there is a down or up transition */
            TemplateKeyInput(hWnd, message, wParam);
            break;

        case WM_CHAR:
            /* Character input.  wParam contains the character code, and
            ** the low word of lParam contains the auto-repeat count. */
            TemplateCharInput(hWnd, wParam, LOWORD(lParam));
            break;

        case WM_COMMAND:
            /* A menu item has been selected, or a control is notifying
            ** its parent.  wParam is the menu item value (for menus),
            ** or control ID (for controls).  For controls, the low word
            ** of lParam has the window handle of the control, and the hi
            ** word has the notification code.  For menus, lParam contains
            ** 0L. */
            TemplateCommand(hWnd, wParam, (HWND)LOWORD(lParam),
                            HIWORD(lParam));
            break;

        case WM_SYSCOMMAND:
            /* N.B. TemplateSysCommand has a DefWindowProc call in it */
            return ((long)TemplateSysCommand(hWnd, message, wParam, lParam));

        case WM_TIMER:
            /* Timer message.  wParam contains the timer ID value */
            TemplateTimer(hWnd, wParam);
            break;

        case WM_VSCROLL:
            /* Vertical scroll bar input.  wParam contains the
            ** scroll code.  For the thumb movement codes, the low
            ** word of lParam contain the new scroll position.
            ** Possible values for wParam are: SB_LINEUP, SB_LINEDOWN,
            ** SB_PAGEUP, SB_PAGEDOWN, SB_THUMBPOSITION, SB_THUMBTRACK */
            TemplateVertScroll(hWnd, wParam, LOWORD(lParam));
            break;

        case WM_HSCROLL:
            /* Horizontal scroll bar input.  Parameters same as for
            ** WM_HSCROLL.  UP and DOWN should be interpreted as LEFT
            ** and RIGHT, respectively. */
            TemplateHorzScroll(hWnd, wParam, (int)lParam);
            break;

        /* For each of following mouse window messages, wParam contains
        ** bits indicating whether or not various virtual keys are down,
        ** and lParam is a POINT containing the mouse coordinates.   The
        ** keydown bits of wParam are:  MK_LBUTTON (set if Left Button is
        ** down); MK_RBUTTON (set if Right Button is down); MK_SHIFT (set
        ** if Shift Key is down); MK_CONTROL (set if Control Key is down). */

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
            TemplateMouse(hWnd, message, wParam, MAKEPOINT(lParam));
            break;

        case WM_RENDERFORMAT:
        case WM_RENDERALLFORMATS:
        case WM_DESTROYCLIPBOARD:
        case WM_DRAWCLIPBOARD:
            /* Data interchange request. */
            break;

        default:

            /* Everything else comes here.  This call MUST exist
            ** in your window proc.  */

            return((long)DefWindowProc(hWnd, message, wParam, lParam));
            break;
    }

    /* A window proc should always return something */
    return(0L);
}


int FAR PASCAL WinMain(hInstance, hPrevInstance, lpszCmdLine, cmdShow)
HANDLE hInstance, hPrevInstance;
LPSTR  lpszCmdLine;
int    cmdShow;
{
    MSG   msg;

    if (!TemplateInitApp(hInstance, hPrevInstance, lpszCmdLine, cmdShow))
        return FALSE;

    /* WM_QUIT message return FALSE and terminate loop */
    while (GetMessage((LPMSG)&msg, NULL, 0, 0)) {
        if (TranslateAccelerator(hWndTemplate, hAccelTable, (LPMSG)&msg) == 0) {
            TranslateMessage((LPMSG)&msg);
            DispatchMessage((LPMSG)&msg);
            }
        }
    return msg.wParam;
}

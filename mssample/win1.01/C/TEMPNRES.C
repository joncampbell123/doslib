/*  TempNRES.c - NonResident portion of application
    Template Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

/*****************************************************************************
** This is a template for a WINDOWS application program.
**
** It illustrates the building of a class record, the registering of
** that class record with WINDOWS, and the creation of window instances
** of that class.  See TEMPLATE.DEF for the associated module definition
** file
**
** This is a MIDDLE model application, it has three code segments _RES, _INIT
** and _NRES and one data segment _DATA.
**
**  For MIDDLE model:
**      Declare WinMain as FAR
**      Use compiler switches  -AM -Gsw -Zpe -NT [segment name]
**      Link with library mlibw
**  For SMALL model:
**      Declare WinMain as NEAR
**      Use compiler switches  -AS -Gsw -Zpe
**      Link with library slibw
**
**  Also, for MIDDLE model, procedures that are accessed from another segment
**  must be declared FAR (see template.h).
**
**  Code that is NOT part of the template but is merely for demo purposes is
**  marked as being for demo purposes and should be replaced/removed when this
**  template is converted to a real application.
**
*****************************************************************************/

#include "windows.h"
#include "template.h"

/*****************************************************************************
** The following variables are initialized once when the application
** is first loaded and then copied into each succeeding instance, using the
** GetInstanceData procedure
*****************************************************************************/

HBRUSH  hbrWhite;
HBRUSH  hbrBlack;
HANDLE  hAccelTable;  /* handle to accelerator table */

HANDLE  hInstTemplate;
HWND    hWndTemplate;

FARPROC lpprocTemplateAboutDlg;

/************************************************************************
** The following following are variables and definitions that are part of
** the template only for demo purposes and will be replaced when the
** template is converted to an application
************************************************************************/

#define RGBBLACK 0L
#define CCHSTR 15
#define CSTR    7
#define POSMIN  0
#define POSMAX  100
#define LINEINC 5
#define PAGEINC 25

int     fDisplay = IDMCOLOR;
int     cchContent;
int     fRadioButton = IDDRBRIGHT;
int     fRadioButtonTemp;
int     iSel = 2;
int     posV = 50;
int     posH = 50;
char    szContent[31];
char    szStr[CCHSTR+1];
char    szDefault[10];
char    szAppName[10];
BOOL    bCheckBox = TRUE;
BOOL    fDoAsync = FALSE;
BOOL    fDoFlash = FALSE;
FARPROC lpprocTemplateDlg;
FARPROC lpprocTimerAsync;
FARPROC lpprocTimerFlash;

/*********** End of demo purpose variables and definitions ************/

/***********************************************************************
**  Procedures in alphabetical order
************************************************************************/


/******* The code below is for demo purposes only *******/
ShowAsyncMessage(hWnd)
HWND hWnd;
{
    char    szOkAsync[CCHMAXSTRING], szAsync[CCHMAXSTRING];

    /* have an outstanding message for user */
    fDoFlash = FALSE;
    KillTimer(hWnd, IDTASYNC);
    KillTimer(hWnd, IDTFLASH);
    FlashWindow(hWnd, FALSE);
    LoadString(hInstTemplate, IDSOKASYNC, (LPSTR)szOkAsync, CCHMAXSTRING);
    LoadString(hInstTemplate, IDSASYNC,   (LPSTR)szAsync  , CCHMAXSTRING);
    MessageBox(hWnd, (LPSTR)szOkAsync, (LPSTR)szAsync,
        MB_OK | MB_ICONEXCLAMATION);
    if (fDoAsync)
        /* restart timer to get another message */
        SetTimer(hWnd, IDTASYNC, CMSECASYNC, lpprocTimerAsync);
}
/******* The code above is for demo purposes only *******/


BOOL FAR PASCAL TemplateAboutDlg(hWndDlg, message, wParam, lParam)
HWND    hWndDlg;
unsigned    message;
WORD    wParam;
LONG    lParam;
{
    switch (message)
    {
    case WM_INITDIALOG:
        /* must return TRUE to this msg, and we do below */
        break;
    case WM_COMMAND:
        switch (wParam)
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(hWndDlg, TRUE);
            break;
        default:
            return FALSE;
        }
    default:
        return FALSE;
    }
    return TRUE;
}

TemplateCharInput(hWnd, ch, cRepeat)
HWND hWnd;
int ch;
int cRepeat;
{
    /* ignore cRepeat if you want to ignore auto-repeated keys */
    while (cRepeat-- != 0)
    {
    }
}

TemplateClose(hWnd)
HWND hWnd;
{
    /******* The code below is for demo purposes only *******/
    char    szOkClose[CCHMAXSTRING], szClose[CCHMAXSTRING];

    LoadString(hInstTemplate, IDSOKCLOSE, (LPSTR)szOkClose, CCHMAXSTRING);
    LoadString(hInstTemplate, IDSCLOSE,   (LPSTR)szClose  , CCHMAXSTRING);
    MessageBeep(MB_OKCANCEL | MB_ICONQUESTION);
    if (MessageBox(hWnd, (LPSTR)szOkClose, (LPSTR)szClose,
        MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
        DestroyWindow(hWnd);
    /******* The code above is for demo purposes only *******/
}

TemplateCommand(hWnd, id, hWndCtl, codeCtl)
HWND hWnd;
int id;
HWND hWndCtl;
int codeCtl;
{
    /******* The code below is for demo purposes only *******/
    HMENU hMenu;
    RECT  rect;
    HDC   hDC;

    switch (id)
    {
        case IDMFLASH:
            GetClientRect(hWnd, (LPRECT)&rect);
            hDC = GetDC(hWnd);
            InvertRect(hDC, (LPRECT)&rect);
            InvertRect(hDC, (LPRECT)&rect);
            ReleaseDC(hWnd, hDC);
            break;
        case IDMCOLOR:
        case IDMBLACK:
            if (fDisplay != id)
                {
                fDisplay = id;
                hMenu = GetMenu(hWnd);
                CheckMenuItem(hMenu, IDMCOLOR, (id == IDMCOLOR ?
                    MF_CHECKED : MF_UNCHECKED));
                CheckMenuItem(hMenu, IDMBLACK, (id != IDMCOLOR ?
                    MF_CHECKED : MF_UNCHECKED));
                /* this will cause repaint of entire window */
                InvalidateRect(hWnd, (LPRECT)NULL, TRUE);
                }
            break;
        case IDMSTART:
            /* start a timer, when it goes off:
            ** if window has focus then post message box
            ** else start flashing window/icon and wait until the
            ** window DOES have the focus before posting the message box,
            ** i.e, a template for an async message box */
            hMenu = GetMenu(hWnd);
            EnableMenuItem(hMenu, IDMSTART, MF_GRAYED );
            EnableMenuItem(hMenu, IDMEND  , MF_ENABLED);
            fDoAsync = TRUE;
            fDoFlash = FALSE;
            SetTimer(hWnd, IDTASYNC, CMSECASYNC, lpprocTimerAsync);
            break;
        case IDMEND:
            KillTimer(hWnd, IDTASYNC);
            KillTimer(hWnd, IDTFLASH);
            fDoAsync = fDoFlash = FALSE;
            hMenu = GetMenu(hWnd);
            EnableMenuItem(hMenu, IDMSTART, MF_ENABLED);
            EnableMenuItem(hMenu, IDMEND  , MF_GRAYED );
            break;
        case IDMDIALOG:
            DialogBox(hInstTemplate, MAKEINTRESOURCE(TEMPLATEDIALOG), hWnd,
                lpprocTemplateDlg);
    }
    /******* The code aabove is for demo purposes only *******/
}

TemplateCreate(hWnd, lParam)
HWND  hWnd;
LONG  lParam;
{
    HMENU   hMenu;
    char    szAbout[15];

    hMenu = GetSystemMenu(hWnd, FALSE);
    ChangeMenu(hMenu, 0, (LPSTR)NULL, -1, MF_APPEND | MF_SEPARATOR);
    LoadString(hInstTemplate, IDSABOUT, (LPSTR)szAbout, 15);
    ChangeMenu(hMenu, 0, (LPSTR)szAbout, IDMABOUT, MF_APPEND | MF_STRING);

    /******* The code below is for demo purposes only *******/
    hMenu = GetMenu(hWnd);
    CheckMenuItem(hMenu, IDMCOLOR,
        (fDisplay == IDMCOLOR ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, IDMBLACK,
        (fDisplay != IDMCOLOR ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(hMenu, IDMEND, MF_GRAYED);
    cchContent = LoadString(hInstTemplate, IDSCONTENT, (LPSTR)szContent, 30);
    szContent[cchContent] = '\0';
    LoadString(hInstTemplate, IDSDEFAULT, (LPSTR)szDefault, 8);
    {
    char *p = szDefault;
    char *q = szStr;
    while (*q++ = *p++)
        {};
    }
    /******* The code above is for demo purposes only *******/
}

/******* The code below is for demo purposes only *******/
BOOL FAR PASCAL TemplateDlg(hWndDlg, message, wParam, lParam)
HWND    hWndDlg;
unsigned    message;
WORD    wParam;
LONG    lParam;
{
    int     i;
    char    *p;
    char    *q;
    HWND    hwndSB;
    int     pos;
    char    szWaters[12];

    switch (message)
    {
    case WM_INITDIALOG:
init:
        SetDlgItemText(hWndDlg, IDDTEXT, (LPSTR)szStr);
        CheckDlgButton(hWndDlg, IDDCHECKBOX, bCheckBox);
        CheckRadioButton(hWndDlg, IDDRBLEFT, IDDRBCENTER,
            (fRadioButtonTemp = fRadioButton));
        for (i = 0; i < CSTR; i++) {
            LoadString(hInstTemplate, IDSSTR1+i, (LPSTR)szWaters, 12);
            SendDlgItemMessage(hWndDlg, IDDLISTBOX, LB_ADDSTRING, 0,
                (LONG)(LPSTR)szWaters);
            }
        SendDlgItemMessage(hWndDlg, IDDLISTBOX, LB_SETCURSEL, iSel, 0L);
        SetScrollRange(GetDlgItem(hWndDlg, IDDSBH), SB_CTL, POSMIN, POSMAX,
            TRUE);
        SetScrollRange(GetDlgItem(hWndDlg, IDDSBV), SB_CTL, POSMIN, POSMAX,
            TRUE);
        SetScrollPos(GetDlgItem(hWndDlg, IDDSBH), SB_CTL, posH, TRUE);
        SetScrollPos(GetDlgItem(hWndDlg, IDDSBV), SB_CTL, posV, TRUE);
        return TRUE;

    case WM_COMMAND:
        switch (wParam)
        {
        case IDDDEFAULTS:
            SetDlgItemText(hWndDlg, IDDTEXT, (LPSTR)szDefault);
            CheckDlgButton(hWndDlg, IDDCHECKBOX, TRUE);
            CheckRadioButton(hWndDlg, IDDRBLEFT, IDDRBCENTER,
                (fRadioButtonTemp = IDDRBRIGHT));
            SendDlgItemMessage(hWndDlg, IDDLISTBOX, LB_SETCURSEL, 2, 0L);
            SetScrollPos(GetDlgItem(hWndDlg, IDDSBH), SB_CTL, 50, TRUE);
            SetScrollPos(GetDlgItem(hWndDlg, IDDSBV), SB_CTL, 50, TRUE);
            break;

        case IDOK:
            bCheckBox = IsDlgButtonChecked(hWndDlg, IDDCHECKBOX);
            fRadioButton = fRadioButtonTemp;
            i = GetDlgItemText(hWndDlg, IDDTEXT, (LPSTR)szStr, CCHSTR);
            szStr[i] = '\0';
            iSel =  SendDlgItemMessage(hWndDlg, IDDLISTBOX, LB_GETCURSEL, 0, 0L);
            iSel = max(0, min(CSTR-1, iSel));
            posH = GetScrollPos(GetDlgItem(hWndDlg, IDDSBH), SB_CTL);
            posV = GetScrollPos(GetDlgItem(hWndDlg, IDDSBV), SB_CTL);
            /* fall through */

        case IDCANCEL:
            EndDialog(hWndDlg, TRUE);
            break;

        case IDDCHECKBOX:
            CheckDlgButton(hWndDlg, IDDCHECKBOX,
                !IsDlgButtonChecked(hWndDlg, IDDCHECKBOX));
            break;

        case IDDRBLEFT:
        case IDDRBRIGHT:
        case IDDRBCENTER:
            CheckRadioButton(hWndDlg, IDDRBLEFT, IDDRBCENTER,
                (fRadioButtonTemp = wParam));
            break;
        default:
            return FALSE;
        }
        break;  /* end of WM_COMMAND */

    /* scroll bar controls send messages here as WM_HSCROLL instead of
    ** WM_COMMAND */
    case WM_HSCROLL:
    case WM_VSCROLL:
        hwndSB = HIWORD(lParam);
        /* GetMenu returns the ID for the scroll bar control */
        switch (GetMenu(hwndSB))
        {
        case IDDSBH:
        case IDDSBV:
            pos = GetScrollPos(hwndSB, SB_CTL);
            switch (wParam)
            {
            case SB_LINEUP:
                pos -=  LINEINC;
                break;
            case SB_LINEDOWN:
                pos +=  LINEINC;
                break;
            case SB_PAGEUP:
                pos -= PAGEINC;
                break;
            case SB_PAGEDOWN:
                pos += PAGEINC;
                break;
            case SB_THUMBPOSITION:
                pos = LOWORD(lParam);
                break;
            case SB_TOP:
                pos = POSMIN;
                break;
            case SB_BOTTOM:
                pos = POSMAX;
                break;
            }   /* end of switch (wParam) */
            pos = max(POSMIN, min(POSMAX, pos));
            SetScrollPos(hwndSB, SB_CTL, pos, TRUE);
            break;

        default:
            return FALSE;
        }   /* end of switch (GetMenu(..)) */

    default:
        return FALSE;

    }  /* end switch (message) */

    return TRUE;
}
/******* The code above is for demo purposes only *******/

TemplateEndSession(hWnd, bEnding)
HWND hWnd;
BOOL bEnding;
{
    if (bEnding)
        {
        /* really ending */
        }
    else
        {
        /* someone replied FALSE to WM_QUERYENDSESSION */
        }
}

TemplateEraseBkgnd(hWnd, hDC)
HWND hWnd;
HDC  hDC;
{
    /******* The code below is for demo purposes only *******/
    HBRUSH  hbr, hbrOld;
    RECT    rect;

    GetClientRect(hWnd, (LPRECT)&rect);
    hbr = (fDisplay == IDMCOLOR ?
            CreateSolidBrush(GetSysColor(COLOR_WINDOW)) : hbrWhite);
    hbrOld = SelectObject(hDC, hbr);
    FillRect(hDC, (LPRECT)&rect, hbr);
    SelectObject(hDC, hbrOld);
    if (fDisplay == IDMCOLOR)
        DeleteObject(hbr);
    /******* The code above is for demo purposes only *******/
}

TemplateGetFocus(hWnd, hWndPrevFocus)
HWND hWnd;
HWND hWndPrevFocus;
{
    if (fDoFlash)
        ShowAsyncMessage(hWnd);
}

TemplateHorzScroll(hWnd, code, posNew)
HWND hWnd;
int code;
int posNew;
{
    switch (code)
    {
        case SB_LINEUP:     /* line left */
            break;
        case SB_LINEDOWN:   /* line right */
            break;
        case SB_PAGEUP:     /* page left */
            break;
        case SB_PAGEDOWN:   /* page right */
            break;
        case SB_THUMBPOSITION:
            /* position to posNew */
            break;
        case SB_THUMBTRACK:
            break;
    }
}

TemplateKeyInput(hWnd, message, ch)
HWND hWnd;
unsigned message;
int ch;
{
    switch (message)
    {
        case WM_KEYDOWN:
            break;
        case WM_KEYUP:
            break;
    }
}

TemplateLoseFocus(hWnd, hWndNewFocus)
HWND hWnd;
HWND hWndNewFocus;
{
}

TemplateMouse(hWnd, message, wParam, lParam)
HWND       hWnd;
unsigned   message;
WORD       wParam;
POINT      lParam;
{
    if (message == WM_MOUSEMOVE)
        {
        }
    if (message == WM_LBUTTONDOWN)
        {
        }
    if (message == WM_LBUTTONUP)
        {
        }
}

TemplateMove(hWnd, x, y)
HWND hWnd;
int x, y;
{
}

TemplatePaint(hWnd, lpps)
HWND hWnd;
LPPAINTSTRUCT lpps;
{
    /* Here the application paints its window. */

    /******* The code below is for demo purposes only *******/
    RECT    rect;
    HBRUSH  hbr, hbrOld;
    HPEN    hpen, hpenOld;
    int     cx, cy;
    long    rgbColor, rgbText;
    int     mode;

    rgbText = (fDisplay == IDMCOLOR ? GetSysColor(COLOR_WINDOWTEXT) : RGBBLACK);
    /* erase background if necessary */
    if (lpps->fErase)
        {
        hbr = (fDisplay == IDMCOLOR ?
                CreateSolidBrush(GetSysColor(COLOR_WINDOW)) : hbrWhite);
        hbrOld = SelectObject(lpps->hdc, hbr);
        FillRect(lpps->hdc, (LPRECT)&lpps->rcPaint, hbr);
        SelectObject(lpps->hdc, hbrOld);
        if (fDisplay == IDMCOLOR)
            DeleteObject(hbr);
        }

    mode = SetBkMode(lpps->hdc, TRANSPARENT);
    rgbColor = SetTextColor(lpps->hdc, rgbText);
    TextOut(lpps->hdc, 0, 0, (LPSTR)szContent, cchContent);
    SetTextColor(lpps->hdc, rgbColor);
    SetBkMode(lpps->hdc, mode);

    hpen = CreatePen(PS_SOLID, 0, rgbText);
    hpenOld = SelectObject(lpps->hdc, hpen);
    hbr     = SelectObject(lpps->hdc, GetStockObject(NULL_BRUSH));
    GetClientRect(hWnd, (LPRECT)&rect);
    cx = (rect.right  - rect.left) >> 2;
    cy = (rect.bottom - rect.top ) >> 2;

    /*  The vertical and horizontal lines on this rectangle are
    **  "one" pixel wide and on displays that do NOT have a 1:1
    **  aspect ratio, the lines will NOT be the same thickness */
    Rectangle(lpps->hdc, rect.left + cx, rect.top  + cy,
        rect.right  - cx, rect.bottom - cy);

    /*  Now we drawn a "rectangle" with uniformly thick lines */
    cx = cx >> 1;
    cy = cy >> 1;
    SelectObject(lpps->hdc, hpenOld);
    DeleteObject(hpen);
    hpen = CreatePen(PS_SOLID, 2 * GetSystemMetrics(SM_CYBORDER), rgbText);
    hpenOld = SelectObject(lpps->hdc, hpen);
    MoveTo(lpps->hdc, rect.left  + cx, rect.top    + cy);
    LineTo(lpps->hdc, rect.right - cx, rect.top    + cy);
    MoveTo(lpps->hdc, rect.left  + cx, rect.bottom - cy);
    LineTo(lpps->hdc, rect.right - cx, rect.bottom - cy);

    SelectObject(lpps->hdc, hpenOld);
    DeleteObject(hpen);
    hpen = CreatePen(PS_SOLID, 2 * GetSystemMetrics(SM_CXBORDER), rgbText);
    hpenOld = SelectObject(lpps->hdc, hpen);
    MoveTo(lpps->hdc, rect.left  + cx, rect.top    + cy);
    LineTo(lpps->hdc, rect.left  + cx, rect.bottom - cy);
    MoveTo(lpps->hdc, rect.right - cx, rect.top    + cy);
    LineTo(lpps->hdc, rect.right - cx, rect.bottom - cy);

    SelectObject(lpps->hdc, hpenOld);
    SelectObject(lpps->hdc, hbr);
    DeleteObject(hpen);
    /******* The code above is for demo purposes only *******/
}

TemplateQueryEndSession(hWnd)
HWND hWnd;
{
    /******* The code below is for demo purposes only *******/
    int id;
    char    szOkSave[CCHMAXSTRING], szSave[CCHMAXSTRING];

    LoadString(hInstTemplate, IDSOKSAVE, (LPSTR)szOkSave, CCHMAXSTRING);
    LoadString(hInstTemplate, IDSSAVE  , (LPSTR)szSave  , CCHMAXSTRING);
    MessageBeep(MB_OKCANCEL | MB_ICONQUESTION);
    id = MessageBox(hWnd, (LPSTR)szOkSave, (LPSTR)szSave,
        MB_YESNOCANCEL | MB_ICONQUESTION);
    switch (id)
    {
        case IDYES:
            /* save files etc and fall through */
        case IDNO:
            /* don't save but still permit End Eession to continue */
            return TRUE;
        case IDCANCEL:
            /* don't allow End Session to continue */
            return FALSE;
    }
    /******* The code above is for demo purposes only *******/
}

TemplateSize(hWnd, x, y, code)
HWND hWnd;
int x, y;
int code;
{
    switch (code)
    {
        case SIZEICONIC:
            break;
        case SIZEFULLSCREEN:
            break;
        case SIZENORMAL:
            break;
    }
}

long TemplateSysCommand(hWnd, message, wParam, lParam)
HWND       hWnd;
unsigned   message;
WORD       wParam;
LONG       lParam;
{
    switch (wParam) {
        case IDMABOUT:
            DialogBox(hInstTemplate, MAKEINTRESOURCE(ABOUTDIALOG), hWnd,
                lpprocTemplateAboutDlg);
            return TRUE;
        /* N.B MUST HAVE A CALL TO DefWindowProc HERE !!!!! */
        default:
            return(DefWindowProc(hWnd, message, wParam, lParam));
        }
    return (0L);
}

TemplateTimer(hWnd, id)
HWND hWnd;
WORD id;
{
    /* A timer event has occurred with ID id.  Process it here. */
}

TemplateVertScroll(hWnd, code, posNew)
HWND hWnd;
int code;
WORD posNew;
{
    switch (code)
    {
        case SB_LINEUP:
            break;
        case SB_LINEDOWN:
            break;
        case SB_PAGEUP:
            break;
        case SB_PAGEDOWN:
            break;
        case SB_THUMBPOSITION:
            /* position to posNew */
            break;
        case SB_THUMBTRACK:
            break;
    }
}

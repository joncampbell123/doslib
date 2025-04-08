/*  Type.c
    Virtual Typewriter Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "type.h"

char OneLine[80];
char TinyString[2] = {0, 0};
short Curpos = 0;
short CharWidth, CharHeight;
char szAppName[10];
char szAbout[10];
char szWindowTitle[30];
HANDLE hInst;
FARPROC lpprocAbout;

long FAR PASCAL TypeWndProc(HWND, unsigned, unsigned, LONG);
//int FAR PASCAL lstrlen( LPSTR );

BOOL FAR PASCAL About( hDlg, message, wParam, lParam )
HWND hDlg;
unsigned message;
unsigned wParam;
LONG lParam;
{
    (void)wParam;
    (void)lParam;
    if (message == WM_COMMAND) {
        EndDialog( hDlg, TRUE );
        return TRUE;
        }
    else if (message == WM_INITDIALOG)
        return TRUE;
    else return FALSE;
}


void TypeCreate( hWnd )
HWND hWnd;
{
    HDC hDC;
    TEXTMETRIC TM;
    int count;

    /* Get character width from textmetric of current font */
    hDC = GetDC( hWnd );
    GetTextMetrics( hDC, (LPTEXTMETRIC)&TM );
    CharWidth = TM.tmAveCharWidth;
    CharHeight= TM.tmHeight + TM.tmExternalLeading;
    ReleaseDC( hWnd, hDC );

    /* Initialize text buffer */
    for (count = 0; count <80; count++) OneLine[count] = 0;
}


void TypePaint( pps )
PAINTSTRUCT *pps;
{
    HBRUSH  hbr, hbrOld;
    HDC hDC = pps->hdc;

    if (pps->fErase) {

        /* Erase update rectangle with background color */

        hbr = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
        hbrOld = (HBRUSH)SelectObject( hDC, (HANDLE)hbr );
        FillRect( hDC, (LPRECT)&pps->rcPaint, hbr );
        SelectObject( hDC, (HANDLE)hbrOld );
        DeleteObject( (HANDLE)hbr );
        }

    /* You may want to calculate the screen content erased in the
     * update rectangle and only repaint that portion.  This app
     * always repaints the whole line.
     */
    SetBkMode( hDC, TRANSPARENT );
    SetTextColor( hDC, GetSysColor(COLOR_WINDOWTEXT) );
    TextOut( hDC, 0, 0, (LPSTR)OneLine, (short)lstrlen((LPSTR)OneLine) );
    SetBkMode( hDC, OPAQUE );
}


void TypeCharInput( hWnd, ch )
HWND hWnd;
char ch;
{
    HDC hDC;
    DWORD lOld, lNew;
    RECT rect;

    hDC = GetDC( hWnd );

    /* Do TextOut in current system text color */
    SetBkMode( hDC, TRANSPARENT );
    SetTextColor( hDC, GetSysColor(COLOR_WINDOWTEXT) );

    if ((ch > 31) && (ch < 127) && Curpos < 79) {
        /* Display character in current cursor position */
        TinyString[0] = OneLine[Curpos++] = ch;
        OneLine[Curpos] = 0;
        TextOut( hDC, CharWidth * (Curpos - 1), 0, TinyString, 1 );
        }
    else if (ch == BS && Curpos > 0) {
        /* Backspace */
        lOld = GetTextExtent( hDC, (LPSTR)OneLine, Curpos );
        OneLine[--Curpos] = 0;
        lNew = (Curpos > 0 ? GetTextExtent(hDC, (LPSTR)OneLine, Curpos) : 0L);
        rect.top    = 0;
        rect.bottom = (int)HIWORD(lOld);
        rect.left   = (int)LOWORD(lNew);
        rect.right  = (int)LOWORD(lOld);
        InvalidateRect( hWnd, (LPRECT)&rect, TRUE );
        }
    else if (ch == CR) {
        /* Erase the whole line */
        Curpos = 0;
        OneLine[0] = '\0';
        InvalidateRect( hWnd, (LPRECT) NULL, TRUE );
        }
    SetBkMode( hDC, OPAQUE );
    ReleaseDC( hWnd, hDC );
}


BOOL TypeInit( hInstance )
HANDLE hInstance;
{
    PWNDCLASS   pTypeClass;

    /* Load strings from resource */
    LoadString( hInstance, IDSNAME, (LPSTR)szAppName, 10 );
    LoadString( hInstance, IDSTITLE, (LPSTR)szWindowTitle, 30 );
    LoadString( hInstance, IDSABOUT, (LPSTR)szAbout, 10 );

    pTypeClass = (PWNDCLASS)LocalAlloc( LPTR, sizeof(WNDCLASS) );

    pTypeClass->hCursor       = LoadCursor( NULL, IDC_ARROW );
    pTypeClass->hIcon         = LoadIcon( hInstance, (LPSTR)szAppName );
    pTypeClass->lpszMenuName  = (LPSTR)NULL;
    pTypeClass->lpszClassName = (LPSTR)szAppName;
    pTypeClass->hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    pTypeClass->hInstance     = hInstance;
    pTypeClass->style         = CS_VREDRAW | CS_HREDRAW;
    pTypeClass->lpfnWndProc   = TypeWndProc;

    if (!RegisterClass( (LPWNDCLASS)pTypeClass ) )
        /* Initialization failed.
         * Windows will automatically deallocate all allocated memory.
         */
        return FALSE;

    LocalFree( (HANDLE)pTypeClass );
    return TRUE;        /* Initialization succeeded */
}


int PASCAL WinMain( hInstance, hPrevInstance, lpszCmdLine, cmdShow )
HANDLE hInstance, hPrevInstance;
LPSTR lpszCmdLine;
int cmdShow;
{
    MSG msg;
    HWND hWnd;
    HMENU hMenu;

    if (!hPrevInstance) {
        /* Call initialization procedure if this is the first instance */
        if (!TypeInit( hInstance ))
            return FALSE;
        }
    else {
        /* Copy data from previous instance */
        GetInstanceData( hPrevInstance, (PSTR)szAppName, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szAbout, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szWindowTitle, 30 );
        }

    /* Create a window instance of class "Type" */
    hWnd = CreateWindow((LPSTR)szAppName,
                        (LPSTR)szWindowTitle,
#if defined(CW_USEDEFAULT) && defined(WS_OVERLAPPEDWINDOW) && TARGET_WINDOWS >= 20 /* Windows 2.0 or higher */
                        /* this 1.01 sample was written before CW_USEDEFAULT was added in the Windows 2.0 SDK.
                         * Windows 1.0 and 2.0 correctly respond to the below case as assumed by this SDK sample.
                         * Windows 3.0 and higher respond to the 0s in the fields by showing a window at minimum size.
                         * CW_USEDEFAULT is required for the window to show at an appropriate size */
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT,    /*  x */
                        CW_USEDEFAULT,    /*  y */
                        CW_USEDEFAULT,    /* cx */
                        CW_USEDEFAULT,    /* cy */
#else
                        WS_TILEDWINDOW,
                        0,    /*  x - ignored for tiled windows */
                        0,    /*  y - ignored for tiled windows */
                        0,    /* cx - ignored for tiled windows */
                        0,    /* cy - ignored for tiled windows */
#endif
                        (HWND)NULL,        /* no parent */
                        (HMENU)NULL,       /* use class menu */
                        (HANDLE)hInstance, /* handle to window instance */
                        (LPSTR)NULL        /* no params to pass on */
                        );

    /* Save instance handle for DialogBox */
    hInst = hInstance;

    /* Bind callback function with module instance */
    lpprocAbout = MakeProcInstance( (FARPROC)About, hInstance );

    /* Insert "About..." into system menu */
    hMenu = GetSystemMenu( hWnd, FALSE );
    ChangeMenu( hMenu, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
    ChangeMenu( hMenu, 0, (LPSTR)szAbout, IDSABOUT, MF_APPEND | MF_STRING );

    /* Make window visible according to the way the app is activated */
    ShowWindow( hWnd, cmdShow );
    UpdateWindow( hWnd );

    /* Polling messages from event queue */
    while (GetMessage((LPMSG)&msg, NULL, 0, 0)) {
        TranslateMessage((LPMSG)&msg);
        DispatchMessage((LPMSG)&msg);
        }

    return (int)msg.wParam;
}


/* Procedures which make up the window class. */
long FAR PASCAL TypeWndProc( hWnd, message, wParam, lParam )
HWND hWnd;
unsigned message;
unsigned wParam;
LONG lParam;
{
    PAINTSTRUCT ps;
    HBRUSH hbr, hbrOld;
    RECT rect;

    switch (message)
    {
    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case IDSABOUT:
            DialogBox( hInst, MAKEINTRESOURCE(ABOUTBOX), hWnd, lpprocAbout );
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam );
            break;
        }
        break;

    case WM_CREATE:
        TypeCreate( hWnd );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    case WM_SETFOCUS:

        /* Create flashing caret. */

        CreateCaret( hWnd, (HBITMAP)NULL, 1, (int)CharHeight );
        SetCaretPos( (int)(CharWidth * Curpos), 0 );
        ShowCaret( hWnd );
        break;

    case WM_KILLFOCUS:
        HideCaret( hWnd );
        DestroyCaret();
        break;

    case WM_PAINT:
        BeginPaint( hWnd, (LPPAINTSTRUCT)&ps );
        TypePaint( (PAINTSTRUCT *)&ps );
        EndPaint( hWnd, (LPPAINTSTRUCT)&ps );
        break;

    case WM_ERASEBKGND:

        /* Erase window with current system background color */

        GetClientRect( hWnd, (LPRECT)&rect );
        hbr = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
        hbrOld = (HBRUSH)SelectObject( (HDC)wParam, (HANDLE)hbr );
        FillRect( (HDC)wParam, (LPRECT)&rect, hbr );
        SelectObject( (HDC)wParam, (HANDLE)hbrOld );
        DeleteObject( (HANDLE)hbr );
        break;

    case WM_CHAR:
        TypeCharInput( hWnd, (char)wParam );
        SetCaretPos( (int)(CharWidth * Curpos), 0 );
        break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
        break;
    }
    return(0L);
}

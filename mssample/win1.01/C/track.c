/*  Track.c
    Track Sample Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "track.h"
#define  abs(x) max(x, -(x))

HANDLE hInst;
FARPROC lpprocAbout;
char szAppName[10];
char szAbout[10];
char szWindowTitle[35];
HBRUSH hbrBlack;
RECT rcPaintOutline;
unsigned char bMouseDown = 0;
#define BMD_LEFT 1
#define BMD_RIGHT 2
WORD cCurrentShape = IDDSTAR;
HANDLE hAccelTable;
POINT pOrigin, pOld;
POINT StarPoints[]= {
    100, 75,
    0, 75,
    80, 0,
    50, 100,
    20, 0,
    100, 75
    };

long FAR PASCAL TrackWndProc(HWND, unsigned, unsigned, LONG);

BOOL FAR PASCAL About( hDlg, message, wParam, lParam )
HWND hDlg;
unsigned message;
WORD wParam;
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


void TrackSetupDC( hDC )
HDC hDC;
{
    SetMapMode( hDC, MM_ANISOTROPIC );
    SetWindowOrg( hDC, -10, 110 );
    SetWindowExt( hDC, 120, -120 );
    SetViewportOrg( hDC,
                    (short)min( rcPaintOutline.right, rcPaintOutline.left ),
                    (short)min( rcPaintOutline.bottom, rcPaintOutline.top ) );
    SetViewportExt( hDC,
                    (short)abs( rcPaintOutline.right-rcPaintOutline.left ),
                    (short)abs( rcPaintOutline.bottom-rcPaintOutline.top ) );
}


void DrawRect( hDC )
HDC hDC;
{
    Rectangle( hDC, 0, 0, 100, 100 );
}


void DrawEllipse( hDC )
HDC hDC;
{
    Ellipse( hDC, 0, 100, 100, 0 );
}


void DrawTriangle( hDC )
HDC hDC;
{
    MoveTo( hDC, 0, 0 );
    LineTo( hDC, 100, 0 );
    LineTo( hDC, 50, 100 );
    LineTo( hDC, 0, 0 );
}


void DrawStar( hDC )
HDC hDC;
{
    Polyline( hDC, StarPoints, 6 );
}


void TrackPaint( hWnd, pps )
HWND hWnd;
PAINTSTRUCT *pps;
{
    HDC hDC = pps->hdc;
    HBRUSH  hbr, hbrOld;
    HPEN    hpen, hpenOld;

    (void)hWnd;

    TrackSetupDC( hDC );

    if (pps->fErase) {
        hbr = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
        hbrOld = (HBRUSH)SelectObject( hDC, (HANDLE)hbr );
        FillRect( hDC, (LPRECT)&pps->rcPaint, hbr );
        SelectObject( hDC, (HANDLE)hbrOld );
        DeleteObject( (HANDLE)hbr );
        }
    hpen = CreatePen( PS_SOLID, 0, GetSysColor(COLOR_WINDOWTEXT) );
    hpenOld = (HPEN)SelectObject( hDC, (HANDLE)hpen );
    SelectObject( hDC, GetStockObject(NULL_BRUSH) );
    switch (cCurrentShape)
    {
    case IDDCLEAR:
        break;
    case IDDRECT:
        DrawRect( hDC );
        break;
    case IDDELLIPSE:
        DrawEllipse( hDC );
        break;
    case IDDTRIANGLE:
        DrawTriangle( hDC );
        break;
    case IDDSTAR:
        DrawStar( hDC );
        break;
    }
    SelectObject( hDC, (HANDLE)hpenOld );
    DeleteObject( (HANDLE)hpen );
}


void TrackCommand( hWnd, id )
HWND hWnd;
WORD id;
{
    HMENU    hMenu;

    hMenu = GetMenu (hWnd);
    CheckMenuItem( hMenu, cCurrentShape, MF_UNCHECKED );
    cCurrentShape = id;
    CheckMenuItem( hMenu, cCurrentShape, MF_CHECKED );
    InvalidateRect( hWnd, (LPRECT) NULL, (BOOL) TRUE );
}


void TrackMouse( hWnd, code, pt )
HWND hWnd;
unsigned code;
POINT pt;
{
    static HDC hDC;

    switch (code)
    {
    case WM_MOUSEMOVE:
        if (!bMouseDown)
            return;
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (bMouseDown)
            return;
        bMouseDown = (code == WM_RBUTTONDOWN) ? BMD_RIGHT : BMD_LEFT;
        SetCapture( hWnd );
        hDC = GetDC( hWnd );
        SelectObject( hDC, (HANDLE)hbrBlack );
        pOld.x = pOrigin.x = pt.x;
        pOld.y = pOrigin.y = pt.y;
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        if (bMouseDown != ((code == WM_RBUTTONUP) ? BMD_RIGHT : BMD_LEFT))
            return;
        bMouseDown = 0;
        ReleaseCapture();
        rcPaintOutline.left = pOrigin.x;
        rcPaintOutline.top = pOrigin.y;
        rcPaintOutline.right = pt.x;
        rcPaintOutline.bottom = pt.y;
        PatBlt( hDC,
                (short)pOrigin.x,
                (short)pOrigin.y,
                (short)(pt.x - pOrigin.x),
                (short)(pt.y - pOrigin.y),
                DSTINVERT );
        ReleaseDC( hWnd, hDC );
        InvalidateRect( hWnd, (LPRECT)NULL, (BOOL)TRUE );
        return;
        break;
    default:
        return;
    }
    PatBlt( hDC,
            (short)pOrigin.x,
            (short)pOld.y,
            (short)(pt.x - pOrigin.x),
            (short)(pt.y - pOld.y),
            DSTINVERT );
    PatBlt( hDC,
            (short)pOld.x,
            (short)pOrigin.y,
            (short)(pt.x - pOld.x),
            (short)(pOld.y - pOrigin.y),
            DSTINVERT );
    pOld.x = pt.x;
    pOld.y = pt.y;
}


int TrackInit( hInstance )
HANDLE hInstance;
{
    PWNDCLASS   pTrackClass;

    /* set up some default brushes */
    hbrBlack = (HBRUSH)GetStockObject( BLACK_BRUSH );

    /* Loading from string table */
    LoadString( hInstance, IDSNAME, (LPSTR)szAppName, 10 );
    LoadString( hInstance, IDSABOUT, (LPSTR)szAbout, 10 );
    LoadString( hInstance, IDSTITLE, (LPSTR)szWindowTitle, 35 );

    pTrackClass = (PWNDCLASS)LocalAlloc(LPTR, sizeof(WNDCLASS));

    pTrackClass->hCursor       = LoadCursor(NULL, IDC_ARROW);
    pTrackClass->hIcon         = LoadIcon(hInstance, (LPSTR)szAppName );
    pTrackClass->lpszMenuName  = (LPSTR)szAppName;
    pTrackClass->lpszClassName = (LPSTR)szAppName;
    pTrackClass->hbrBackground = NULL;
    pTrackClass->hInstance     = hInstance;
    pTrackClass->style         = CS_VREDRAW | CS_HREDRAW;
    pTrackClass->lpfnWndProc   = TrackWndProc;

    if (!RegisterClass((LPWNDCLASS)pTrackClass))
        return FALSE;

    LocalFree((HANDLE)pTrackClass);
    hAccelTable = LoadAccelerators(hInstance, (LPSTR)szAppName);
    return TRUE;
}


/* Procedure called every time a new instance of the application
 * is created
 */
int PASCAL WinMain( hInstance, hPrevInstance, lpszCmdLine, cmdShow )
HANDLE hInstance, hPrevInstance;
LPSTR lpszCmdLine;
int cmdShow;
{
    MSG   msg;
    HWND  hWnd;
    HMENU hMenu;

    (void)lpszCmdLine;

    if (!hPrevInstance) {
        if (!TrackInit(hInstance))
            return FALSE;
    }
    else {
        GetInstanceData( hPrevInstance, (PSTR)&hbrBlack, sizeof(hbrBlack) );
        GetInstanceData( hPrevInstance, (PSTR)&hAccelTable, sizeof(hAccelTable) );
        GetInstanceData( hPrevInstance, (PSTR)szAppName, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szAbout, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szWindowTitle, 35 );
    }

    hWnd = CreateWindow( (LPSTR)szAppName,
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
                         0,
                         0,
                         0,
                         0,
#endif
                         (HWND)NULL,
                         (HMENU)NULL,
                         (HANDLE)hInstance,
                         (LPSTR)NULL );

    hInst = hInstance;
    lpprocAbout = MakeProcInstance( (FARPROC)About, hInstance );
    hMenu = GetSystemMenu( hWnd, FALSE );
    ChangeMenu( hMenu, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
    ChangeMenu( hMenu, 0, (LPSTR)szAbout, IDSABOUT, MF_APPEND | MF_STRING );

    ShowWindow( hWnd, cmdShow );
    UpdateWindow( hWnd );

    while (GetMessage((LPMSG)&msg, NULL, 0, 0)) {
        if (TranslateAccelerator(hWnd, hAccelTable, (LPMSG)&msg) == 0) {
            TranslateMessage( (LPMSG)&msg );
            DispatchMessage( (LPMSG)&msg );
        }
    }

    return (int)msg.wParam;
}


/* Procedures which make up the window class. */
long FAR PASCAL TrackWndProc( hWnd, message, wParam, lParam )
HWND hWnd;
unsigned message;
unsigned wParam;
LONG lParam;
{
    PAINTSTRUCT ps;
    RECT rect;
    HBRUSH hbr, hbrOld;

    switch (message)
    {
    case WM_SYSCOMMAND:
        switch (wParam)
        {
        case IDSABOUT:
            DialogBox( hInst, MAKEINTRESOURCE(ABOUTBOX), hWnd, lpprocAbout );
            break;
        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
        }
        break;

    case WM_CREATE:
        CheckMenuItem( GetMenu(hWnd), cCurrentShape, MF_CHECKED );
        break;

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
        TrackMouse( hWnd, message, MAKEPOINT(lParam) );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    case WM_PAINT:
        BeginPaint( hWnd, (LPPAINTSTRUCT)&ps );
        TrackPaint( hWnd, (PAINTSTRUCT *)&ps );
        EndPaint( hWnd, (LPPAINTSTRUCT)&ps );
        break;

    case WM_ERASEBKGND:
        GetClientRect( hWnd, (LPRECT)&rect );
        hbr = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
        hbrOld = (HBRUSH)SelectObject( (HDC)wParam, (HANDLE)hbr );
        FillRect( (HDC)wParam, (LPRECT)&rect, hbr );
        SelectObject( (HDC)wParam, (HANDLE)hbrOld );
        DeleteObject( (HANDLE)hbr );
        break;

    case WM_COMMAND:
        TrackCommand( hWnd, wParam );
        break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
        break;
    }
    return(0L);
}

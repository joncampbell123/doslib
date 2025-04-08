/*  Shapes.c
    Shapes Sample Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "shapes.h"

HANDLE hInst;
HANDLE hAccelTable;
WORD cCurrentShape = IDDSTAR;
FARPROC lpprocAbout;

char szAppName[10];
char szAbout[10];
char szWindowTitle[15];

static POINT StarPoints[]=
    {
    100, 75,
    0, 75,
    80, 0,
    50, 100,
    20, 0,
    100, 75
    };

long FAR PASCAL ShapesWndProc(HWND, unsigned, unsigned, LONG);

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


void DrawSmiley( hDC )
HDC hDC;
{
    Ellipse( hDC, 0, 100, 100, 0 ); //face
    Arc( hDC, 15, 15, 85, 85, 15, 35, 85, 35 ); //smile
    Ellipse( hDC, 25, 70, 35, 60 ); //left eye
    Ellipse( hDC, 65, 70, 75, 60 ); //right eye
}


void ShapesSetupDC( hWnd, hDC )
HWND hWnd;
HDC hDC;
{
    RECT ClientRect;

    SetBkMode( hDC, OPAQUE );
    SetROP2( hDC, R2_COPYPEN );
    GetClientRect( hWnd, (LPRECT)&ClientRect );
    SetMapMode( hDC, MM_ANISOTROPIC );
    SetWindowOrg( hDC, -10, 110 );
    SetWindowExt( hDC, 120, -120 );
    SetViewportOrg( hDC, 0, 0 );
    SetViewportExt( hDC,
                    (short)(ClientRect.right-ClientRect.left),
                    (short)(ClientRect.bottom-ClientRect.top) );
}


void ShapesPaint( hWnd, pps )
HWND hWnd;
PAINTSTRUCT *pps;
{
    HDC hDC = pps->hdc;
    HBRUSH hbr, hbrOld;
    HPEN hpen, hpenOld;

    ShapesSetupDC( hWnd, hDC );
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
    case IDSMILEY:
        DrawSmiley( hDC );
        break;
    }
    SelectObject( hDC, (HANDLE)hpenOld );
    DeleteObject( (HANDLE)hpen );
}


void ShapesCommand( hWnd, id )
HWND hWnd;
WORD id;
{
    HMENU hMenu = GetMenu( hWnd );

    CheckMenuItem( hMenu, cCurrentShape, MF_UNCHECKED );
    cCurrentShape = id;
    CheckMenuItem( hMenu, cCurrentShape, MF_CHECKED );
    InvalidateRect( hWnd, (LPRECT)NULL, TRUE );
}


BOOL ShapesInit( hInstance )
HANDLE hInstance;
{
    PWNDCLASS   pShapesClass;

    /* Loading from string table */
    LoadString( hInstance, IDSNAME, (LPSTR)szAppName, 10 );
    LoadString( hInstance, IDSABOUT, (LPSTR)szAbout, 10 );
    LoadString( hInstance, IDSTITLE, (LPSTR)szWindowTitle, 15 );

    pShapesClass = (PWNDCLASS)LocalAlloc( LPTR, sizeof(WNDCLASS) );

    pShapesClass->hCursor        = LoadCursor( NULL, IDC_ARROW );
    pShapesClass->hIcon          = LoadIcon( hInstance, (LPSTR)szAppName );
    pShapesClass->lpszMenuName   = (LPSTR)szAppName;
    pShapesClass->hInstance      = hInstance;
    pShapesClass->lpszClassName  = (LPSTR)szAppName;
    pShapesClass->hbrBackground  = NULL;
    pShapesClass->style          = CS_HREDRAW | CS_VREDRAW;
    pShapesClass->lpfnWndProc    = ShapesWndProc;

    if (!RegisterClass( (LPWNDCLASS)pShapesClass) )
        return FALSE;   /* Initialization failed */

    LocalFree( (HANDLE)pShapesClass );
    hAccelTable = LoadAccelerators( hInstance, (LPSTR)szAppName );

    return TRUE;    /* Initialization succeeded */
}


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
        if (!ShapesInit( hInstance ))
            return FALSE;
        }
    else {
        /* Copy data from previous instance */
        GetInstanceData( hPrevInstance, (PSTR)szAppName, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szAbout, 10 );
        GetInstanceData( hPrevInstance, (PSTR)szWindowTitle, 15 );
        GetInstanceData( hPrevInstance, (PSTR)&hAccelTable, sizeof(hAccelTable) );
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

    hInst = hInstance;
    lpprocAbout = MakeProcInstance( (FARPROC)About, hInstance );
    hMenu = GetSystemMenu( hWnd, FALSE );
    ChangeMenu( hMenu, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
    ChangeMenu( hMenu, 0, (LPSTR)szAbout, IDSABOUT, MF_APPEND | MF_STRING );

    ShowWindow( hWnd, cmdShow );
    UpdateWindow( hWnd );

    while (GetMessage( (LPMSG)&msg, NULL, 0, 0) ) {
        if (TranslateAccelerator( hWnd, hAccelTable, (LPMSG)&msg ) == 0) {
            TranslateMessage( (LPMSG)&msg );
            DispatchMessage( (LPMSG)&msg );
            }
        }

    return (int)msg.wParam;
}


LONG FAR PASCAL ShapesWndProc( hWnd, message, wParam, lParam )
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
            return DefWindowProc(hWnd, message, wParam, lParam );
            break;
        }
        break;

    case WM_CREATE:
        CheckMenuItem( GetMenu(hWnd), cCurrentShape, MF_CHECKED );
        break;

    case WM_PAINT:
        BeginPaint( hWnd, (LPPAINTSTRUCT)&ps );
        ShapesPaint( hWnd, (PAINTSTRUCT *)&ps );
        EndPaint( hWnd, (LPPAINTSTRUCT)&ps );
        break;

    case WM_ERASEBKGND:
        GetClientRect( hWnd, (LPRECT)&rect );
        hbr = CreateSolidBrush( GetSysColor(COLOR_WINDOW) );
        hbrOld = (HBRUSH)SelectObject( (HDC)wParam, (HANDLE)hbr );
        FillRect((HDC)wParam, (LPRECT)&rect, hbr);
        SelectObject( (HDC)wParam, (HANDLE)hbrOld );
        DeleteObject( (HANDLE)hbr );
        break;

    case WM_COMMAND:
        ShapesCommand( hWnd, wParam );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam );
        break;
    }
    return(0L);
}

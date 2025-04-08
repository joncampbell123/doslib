/*  Fonttest.c
    Fonttest Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "fonttest.h"

typedef BOOL FAR * LPBOOL;

//extern long FAR PASCAL lstrcpy();
//extern long FAR PASCAL lstrlen();

FARPROC       lpprocOptions;
FARPROC       lpprocAbout;
HANDLE        hInst;
HANDLE        hAccelTable;  /* handle to accelerator table */
int           FontYCoord;
short         nMapMode;
LOGFONT       LogFont;
LPLOGFONT     lpLogFont;
TEXTMETRIC    TM;
LPTEXTMETRIC  lpTM = (LPTEXTMETRIC)&TM;

char szAppName[10];
char szAbout[10];
char szWindowTitle[15];
char szAlphabet[LF_FACESIZE+55];

long FAR PASCAL FonttestWndProc(HWND, unsigned, unsigned, LONG);

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


void ShowSampleString( hDC )
HDC hDC;
{
    HANDLE hLogFont, hLogFontOld;

    hLogFont = (HANDLE)CreateFontIndirect( lpLogFont );
    SetMapMode( hDC, nMapMode );
    hLogFontOld = SelectObject( hDC, hLogFont );
    GetTextMetrics( hDC, (LPTEXTMETRIC)lpTM );

    TextOut( hDC, 0, FontYCoord, (LPSTR)szAlphabet,
                                (short)lstrlen((LPSTR)szAlphabet) );
    FontYCoord += lpTM->tmHeight + lpTM->tmExternalLeading;
    SelectObject( hDC, hLogFontOld );
    DeleteObject( hLogFont );
}


void InitDlg( hDlg )
HWND hDlg;
{
    if (lpLogFont->lfUnderline)
        CheckDlgButton( hDlg, IDUNDERLINE, TRUE );
    if (lpLogFont->lfStrikeOut)
        CheckDlgButton( hDlg, IDSTRIKEOUT, TRUE );
    if (lpLogFont->lfItalic)
        CheckDlgButton( hDlg, IDITALIC, TRUE );
    SetDlgItemInt( hDlg, IDHEIGHT, lpLogFont->lfHeight, FALSE );
    SetDlgItemInt( hDlg, IDTMHEIGHT, lpTM->tmHeight, FALSE );
    SetDlgItemInt( hDlg, IDWIDTH,  lpLogFont->lfWidth, FALSE );
    SetDlgItemInt( hDlg, IDTMWIDTH,lpTM->tmAveCharWidth, FALSE );
    SetDlgItemInt( hDlg, IDWEIGHT, lpLogFont->lfWeight, FALSE );
    SetDlgItemInt( hDlg, IDTMWEIGHT, lpTM->tmWeight, FALSE );
    SetDlgItemInt( hDlg, IDAVCHARWIDTH, lpTM->tmAveCharWidth, FALSE );
    SetDlgItemInt( hDlg, IDMAXCHARWIDTH, lpTM->tmMaxCharWidth, FALSE );
    SetDlgItemInt( hDlg, IDESCAPEMENT, lpLogFont->lfEscapement, FALSE );
    SetDlgItemInt( hDlg, IDORIENTATION,lpLogFont->lfOrientation,FALSE );
    SetDlgItemInt( hDlg, IDPITCH, lpLogFont->lfPitchAndFamily, FALSE );
    SetDlgItemInt( hDlg, IDTMPITCH, 1 + lpTM->tmPitchAndFamily, FALSE );
    SetDlgItemInt( hDlg, IDCHARSET, lpLogFont->lfCharSet, FALSE );
    SetDlgItemInt( hDlg, IDTMCHARSET, lpTM->tmCharSet, FALSE );
    SetDlgItemInt( hDlg, IDMAPMODE, nMapMode, FALSE );
}


void DlgCommand( hDlg, wParam, lParam )
HWND hDlg;
WORD wParam;
LONG lParam;
{
    BOOL fValOK;

    (void)lParam;
    switch (wParam)
    {
    case IDOK:
        lpLogFont->lfItalic    = IsDlgButtonChecked( hDlg, IDITALIC )
                                 ? 1 : 0;
        lpLogFont->lfUnderline = IsDlgButtonChecked( hDlg, IDUNDERLINE )
                                 ? 1 : 0;
        lpLogFont->lfStrikeOut = IsDlgButtonChecked( hDlg, IDSTRIKEOUT )
                                 ? 1 : 0;
        lpLogFont->lfWeight = GetDlgItemInt( hDlg,
                                             IDWEIGHT,
                                             (LPBOOL)&fValOK,
                                             FALSE );
        lpLogFont->lfWidth  = GetDlgItemInt( hDlg,
                                             IDWIDTH,
                                             (LPBOOL)&fValOK,
                                             FALSE );
        lpLogFont->lfHeight = GetDlgItemInt( hDlg,
                                             IDHEIGHT,
                                             (LPBOOL)&fValOK,
                                             FALSE );
        lpLogFont->lfEscapement = GetDlgItemInt
                                            ( hDlg,
                                              IDESCAPEMENT,
                                              (LPBOOL)&fValOK,
                                              FALSE );
        lpLogFont->lfOrientation = GetDlgItemInt
                                            ( hDlg,
                                              IDORIENTATION,
                                              (LPBOOL)&fValOK,
                                              FALSE );
        lpLogFont->lfPitchAndFamily = (BYTE)GetDlgItemInt
                                            ( hDlg,
                                              IDPITCH,
                                              (LPBOOL)&fValOK,
                                              FALSE );
        lpLogFont->lfCharSet = (BYTE)GetDlgItemInt
                                            ( hDlg,
                                              IDCHARSET,
                                              (LPBOOL)&fValOK,
                                              FALSE );
        nMapMode = GetDlgItemInt( hDlg, IDMAPMODE, (LPBOOL)&fValOK, FALSE );
        EndDialog( hDlg, TRUE );
        break;

    case IDCANCEL:
        EndDialog( hDlg, TRUE );
        break;

    case IDITALIC:
    case IDUNDERLINE:
    case IDSTRIKEOUT:
        CheckDlgButton( hDlg,  wParam,
                        (IsDlgButtonChecked(hDlg, wParam) ? FALSE : TRUE) );
        break;

    default:
        break;
    }
}


BOOL FAR PASCAL ChangeOptions(hDlg, message, wParam, lParam)
HWND hDlg;
unsigned message;
WORD wParam;
LONG lParam;
{
    switch (message)
    {
    case WM_COMMAND:
        DlgCommand( hDlg, wParam, lParam );
        break;

    case WM_INITDIALOG:
        InitDlg( hDlg );
        break;

    default:
        return FALSE;
    }
    return TRUE;
}


void FonttestCommand( hWindow, id )
HWND hWindow;
WORD id;
{
    HDC hDC;

    switch (id)
    {
    case IDMSAMPLE:
        hDC = GetDC( hWindow );
        ShowSampleString( hDC );
        ReleaseDC( hWindow, hDC );
        break;

    case IDMCLEAR:
        InvalidateRect( hWindow, (LPRECT)NULL, TRUE );
        UpdateWindow( hWindow );
        break;

    case IDMOPTIONS:
        DialogBox( hInst, MAKEINTRESOURCE(OPTIONSBOX), hWindow, lpprocOptions );
        break;

    default:
        break;
    }
}


long FAR PASCAL FonttestWndProc( hWnd, message, wParam, lParam )
HWND hWnd;
unsigned message;
unsigned wParam;
LONG lParam;
{
    PAINTSTRUCT ps;

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

    case WM_PAINT:
        BeginPaint( hWnd, (LPPAINTSTRUCT)&ps );
        EndPaint( hWnd, (LPPAINTSTRUCT)&ps );
        FontYCoord = 0;
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

    case WM_COMMAND:
        FonttestCommand( hWnd, wParam );
        break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
        break;
    }
    return 0L;
}


BOOL FonttestInit( hInstance )
HANDLE hInstance;
{
    PWNDCLASS   pTemplateClass;

    lpLogFont = (LPLOGFONT)&LogFont;

    /* Loading from string table */
    LoadString( hInstance, IDSNAME, (LPSTR)szAppName, 10 );
    LoadString( hInstance, IDSABOUT, (LPSTR)szAbout, 10 );
    LoadString( hInstance, IDSTITLE, (LPSTR)szWindowTitle, 15 );
    LoadString( hInstance, IDSALPHABET, (LPSTR)szAlphabet, LF_FACESIZE+55 );

    pTemplateClass = (PWNDCLASS)LocalAlloc( LPTR, sizeof(WNDCLASS));

    pTemplateClass->hCursor        = LoadCursor(NULL, IDC_ARROW);
    pTemplateClass->hIcon          = LoadIcon(hInstance, (LPSTR)szAppName);
    pTemplateClass->lpszMenuName   = (LPSTR)szAppName;
    pTemplateClass->lpszClassName  = (LPSTR)szAppName;
    pTemplateClass->hbrBackground  = (HBRUSH)GetStockObject (WHITE_BRUSH);
    pTemplateClass->hInstance      = hInstance;
    pTemplateClass->lpfnWndProc    = (WNDPROC)FonttestWndProc;
    pTemplateClass->style          = CS_VREDRAW | CS_HREDRAW;

    if (!RegisterClass((LPWNDCLASS)pTemplateClass))
        return FALSE;

    LocalFree((HANDLE)pTemplateClass);
    hAccelTable = LoadAccelerators(hInstance, (LPSTR)szAppName);

    return TRUE;
}


int PASCAL WinMain(hInstance, hPrev, lpszCmdLine, cmdShow)
HANDLE hInstance, hPrev;
LPSTR lpszCmdLine;
int cmdShow;
{
    MSG msg;
    HWND hWnd;
    HMENU hMenu;

    (void)lpszCmdLine;

    if (!hPrev) {
        if (!FonttestInit(hInstance))
            return(FALSE);
        }
    else {
        GetInstanceData( hPrev, (PSTR)&LogFont,     sizeof(LogFont) );
        GetInstanceData( hPrev, (PSTR)&lpLogFont,   sizeof(lpLogFont) );
        GetInstanceData( hPrev, (PSTR)&hInst,       sizeof(hInst) );
        GetInstanceData( hPrev, (PSTR)&TM,          sizeof(TM) );
        GetInstanceData( hPrev, (PSTR)&lpTM,        sizeof(lpTM) );
        GetInstanceData( hPrev, (PSTR)&nMapMode,    sizeof(nMapMode) );
        GetInstanceData( hPrev, (PSTR)&hAccelTable, sizeof(hAccelTable) );
        GetInstanceData( hPrev, (PSTR)szAppName,    10 );
        GetInstanceData( hPrev, (PSTR)szAbout,      10 );
        GetInstanceData( hPrev, (PSTR)szWindowTitle,15 );
        GetInstanceData( hPrev, (PSTR)szAlphabet,   LF_FACESIZE+55 );
        }

    FontYCoord = 0;
    nMapMode = 1;  /* the default mapmode */

    hWnd = CreateWindow( (LPSTR)szAppName,
                         (LPSTR)szWindowTitle,
                         WS_TILEDWINDOW,
                         0, 0, 0, 0,
                         (HWND)NULL,
                         (HMENU)NULL,
                         (HANDLE)hInstance,
                         (LPSTR)NULL
                       );

    ShowWindow( hWnd, cmdShow );
    UpdateWindow( hWnd );

    hInst = hInstance;
    lpprocOptions = MakeProcInstance( (FARPROC)ChangeOptions, hInstance );
    lpprocAbout = MakeProcInstance( (FARPROC)About, hInstance );
    hMenu = GetSystemMenu( hWnd, FALSE );
    ChangeMenu( hMenu, 0, NULL, 999, MF_APPEND | MF_SEPARATOR );
    ChangeMenu( hMenu, 0, (LPSTR)szAbout, IDSABOUT, MF_APPEND | MF_STRING );

    while (GetMessage((LPMSG)&msg, NULL, 0, 0)) {
        if (TranslateAccelerator(hWnd, hAccelTable, (LPMSG)&msg) == 0) {
            TranslateMessage((LPMSG)&msg);
            DispatchMessage((LPMSG)&msg);
            }
        }

    return (int)msg.wParam;
}

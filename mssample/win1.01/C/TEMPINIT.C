/*  TempINIT.c - Initialization portion of application
    Template Application
    Microsoft Windows Version 1.01
    Copyright (c) Microsoft 1985        */

#include "windows.h"
#include "template.h"

extern HBRUSH  hbrWhite;
extern HBRUSH  hbrBlack;
extern HANDLE  hAccelTable;
extern HANDLE  hInstTemplate;
extern HWND    hWndTemplate;
extern char    szAppName[];

extern FARPROC lpprocTemplateAboutDlg;

/************************************************************************
** The following variables and definitions are for demo purposes only
** and will be replaced when the template is converted to an application
************************************************************************/

extern FARPROC lpprocTemplateDlg;
extern FARPROC lpprocTimerAsync;
extern FARPROC lpprocTimerFlash;

/*********** End of demo purpose variables and definitions ************/

int FAR TemplateInitApp( hInstance, hPrevInstance, lpszCmdLine, cmdShow )
HANDLE hInstance, hPrevInstance;
LPSTR lpszCmdLine;
int cmdShow;
{
    char szTitle[21];

    if (!hPrevInstance)
    {
        if (!TemplateInitInstance(hInstance))
            return FALSE;
    }
    else
    {
        /* Copy global instance variables from previous instance */
        GetInstanceData(hPrevInstance, (PSTR)&hbrWhite, sizeof(hbrWhite));
        GetInstanceData(hPrevInstance, (PSTR)&hbrBlack, sizeof(hbrBlack));
        GetInstanceData(hPrevInstance, (PSTR)&hAccelTable, sizeof(hAccelTable));
        GetInstanceData(hPrevInstance, (PSTR)szAppName, 10);
    }

    lpprocTemplateAboutDlg =
        MakeProcInstance((FARPROC)TemplateAboutDlg, hInstance);

    /******* The code below is for demo purposes only *******/
    lpprocTemplateDlg = MakeProcInstance((FARPROC)TemplateDlg, hInstance);
    lpprocTimerAsync  = MakeProcInstance((FARPROC)TimerAsync,  hInstance);
    lpprocTimerFlash  = MakeProcInstance((FARPROC)TimerFlash,  hInstance);
    /******* The code above is for demo purposes only *******/

    /* Create a window instance of class "Template" */
    hInstTemplate = hInstance;
    LoadString(hInstTemplate, IDSTITLE, (LPSTR)szTitle, 21);
    hWndTemplate = CreateWindow(
        (LPSTR)szAppName, /* window class name                               */
        (LPSTR)szTitle,   /* name appearing in window caption                */
        WS_TILEDWINDOW,   /* Style, | WS_HSCROLL | WS_VSCROLL for scrollbar  */
        0,                /*   x : ignored for tiled window                  */
        0,                /*   y : ignored for tiled window                  */
        0,                /*  cx : ignored for tiled window                  */
        0,                /*  cy : ignored for tiled window                  */
        (HWND)NULL,       /*  parent window                                  */
        (HMENU)NULL,      /*  menu, or child window id                       */
        (HANDLE)hInstance,/*  handle to window instance                      */
        (LPSTR)NULL       /*  params to pass on                              */
        );

    /* Make window visible */
    ShowWindow(hWndTemplate, cmdShow);
    UpdateWindow(hWndTemplate);

    return TRUE;
}


/* Procedure called when the application is loaded */
int TemplateInitInstance(hInstance)
HANDLE hInstance;
{
    PWNDCLASS pTemplateClass;

    /* set up some default brushes */
    hbrWhite = GetStockObject(WHITE_BRUSH);
    hbrBlack = GetStockObject(BLACK_BRUSH);

    /* load string from resource */
    LoadString(hInstance, IDSAPPNAME, (LPSTR)szAppName, 10);

    /* allocate class structure in local heap */
    pTemplateClass = (PWNDCLASS)LocalAlloc(LPTR, sizeof(WNDCLASS));

    /* get necessary resources */
    pTemplateClass->hCursor       = LoadCursor(NULL, IDC_ARROW);
    pTemplateClass->hIcon         = LoadIcon(hInstance, (LPSTR)szAppName);
    pTemplateClass->lpszMenuName  = (LPSTR)szAppName;

    pTemplateClass->lpszClassName = (LPSTR)szAppName;
    /* use the following for a white background
    pTemplateClass->hbrBackground = hbrWhite;
    */
    pTemplateClass->hbrBackground = NULL;   /* app does its own bkgrnd */
    pTemplateClass->hInstance     = hInstance;

    pTemplateClass->style         = CS_VREDRAW | CS_HREDRAW;

    /* Register our Window Proc */
    pTemplateClass->lpfnWndProc   = TemplateWndProc;

    /* register this new class with WINDOWS */
    if (!RegisterClass((LPWNDCLASS)pTemplateClass))
        return FALSE;   /* Initialization failed */

    LocalFree((HANDLE)pTemplateClass);
    hAccelTable = LoadAccelerators(hInstance, (LPSTR)szAppName);
    return TRUE;    /* Initialization succeeded */
}

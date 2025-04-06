/***********************************************************************
** IDM - ID for Menu commands
** IDS - ID for String table
** IDD - ID for Dialog item
***********************************************************************/

#define IDMABOUT       1
#define IDSAPPNAME     21
#define IDSABOUT       22
#define IDSTITLE       23
#define IDSDEFAULT     24
#define IDSSTR1        25       /* These                                */
#define IDSSTR2        26       /*      seven                           */
#define IDSSTR3        27       /*           string                     */
#define IDSSTR4        28       /*                 ids                  */
#define IDSSTR5        29       /*                    must              */
#define IDSSTR6        30       /*                        be            */
#define IDSSTR7        31       /*                          consecutive */

/* Dialog box resource ids */
#define ABOUTDIALOG    1
#define TEMPLATEDIALOG 2


/************ The code below is for demo purposes only *******************/
/* IDM... ID for Menus      */
/* IDT... ID for Timers     */
/* IDS... ID for Strings    */
/* IDD... ID for Dialog Box */

#define IDMFLASH    0
#define IDMCOMMANDS 1

#define IDMCOLOR    11
#define IDMBLACK    12
#define IDMDIALOG   13
#define IDMSTART    14
#define IDMEND      15

#define IDTASYNC    16
#define IDTFLASH    17

/* max number of char in the associated strings in the rc file */
#define CCHMAXSTRING 50
#define IDSCONTENT  3
#define IDSCLOSE    4
#define IDSOKCLOSE  5
#define IDSSAVE     6
#define IDSOKSAVE   7
#define IDSASYNC    8
#define IDSOKASYNC  9

#define IDNULL      -1
#define IDDDEFAULTS 0x0101
#define IDDTEXT     0x0102
#define IDDCHECKBOX 0x0103
#define IDDNULL     0x0104
#define IDDRBLEFT   0x0105
#define IDDRBRIGHT  0x0106
#define IDDRBCENTER 0x0107
#define IDDLISTBOX  0x0108
#define IDDSBH      0x0109
#define IDDSBV      0x010a

/* count of msec for async message box and window flash rate */
#define CMSECASYNC  10000
#define CMSECFLASH  1000

/************ The code above is for demo purposes only *******************/

/*********************************************************************
**  These procedures must be declared FAR, even if this application
**  were SMALL model, because they are exported and called by windows.
*********************************************************************/

BOOL FAR PASCAL TemplateAboutDlg(HWND, unsigned, WORD, LONG);
BOOL FAR PASCAL TemplateDlg     (HWND, unsigned, WORD, LONG);
long FAR PASCAL TemplateWndProc (HWND, unsigned, WORD, LONG);
int  FAR PASCAL TimerAsync(HWND, unsigned, WORD, DWORD);
int  FAR PASCAL TimerFlash(HWND, unsigned, WORD, DWORD);

/*********************************************************************
**  These procedures are declared here to get type and parameter
**  checking for calls between modules.  The compiler switches -AS and
**  -AM for SMALL and MIDDLE model will make them NEAR and FAR
**  respectively, so there is no explicit declaration as NEAR or FAR.
*********************************************************************/

TemplateCharInput(HWND, int, int);
TemplateClose(HWND);
TemplateCommand(HWND, int, HWND, int);
TemplateCreate(HWND, LONG);
TemplateEndSession(HWND, BOOL);
TemplateEraseBkgnd(HWND, HDC);
TemplateGetFocus(HWND, HWND);
TemplateHorzScroll(HWND, int, int);
TemplateKeyInput(HWND, unsigned, int);
TemplateLoseFocus(HWND, HWND);
TemplateMouse(HWND, unsigned, WORD, POINT);
TemplateMove(HWND, int, int);
TemplatePaint(HWND, LPPAINTSTRUCT);
TemplateQueryEndSession(HWND);
TemplateSize(HWND, int, int, int);
long TemplateSysCommand(HWND, unsigned, WORD, LONG);
TemplateTimer(HWND, WORD);
TemplateVertScroll(HWND, int, WORD);
int TemplateInitApp(HANDLE, HANDLE, LPSTR, int);
/********************************************************************/

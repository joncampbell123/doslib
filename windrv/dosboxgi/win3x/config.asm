
	org	0

%macro RGB 3 ; R, G, B, 0
        db      %1,%2,%3,0
%endmacro

;	Machine dependent parameters

        dw      15                      ;Height of vertical thumb
        dw      15                      ;Width of horizontal thumb
	dw	2			;Icon horiz compression factor
	dw	2			;Icon vert compression factor
	dw	1			;Cursor horz compression factor
	dw	1			;Cursor vert compression factor
	dw	0			;Kanji window height
	dw	1			;cxBorder (thickness of vertical lines)
	dw	1			;cyBorder (thickness of horizontal lines)

;	Default system color values

        RGB 128,128,128     ;clrScrollbar
        RGB 255,255,255     ;clrDesktop
        RGB 000,000,000     ;clrActiveCaption
        RGB 255,255,255     ;clrInactiveCaption
        RGB 255,255,255     ;clrMenu
        RGB 255,255,255     ;clrWindow
        RGB 000,000,000     ;clrWindowFrame
        RGB 000,000,000     ;clrMenuText
        RGB 000,000,000     ;clrWindowText
        RGB 255,255,255     ;clrCaptionText
        RGB 128,128,128     ;clrActiveBorder
        RGB 255,255,255     ;clrInactiveBorder
        RGB 255,255,255     ;clrAppWorkspace
        RGB 000,000,000     ;clrHiliteBk
        RGB 255,255,255     ;clrHiliteText
        RGB 255,255,255     ;clrBtnFace
        RGB 128,128,128     ;clrBtnShadow
        RGB 000,000,000     ;clrGrayText	(0 is no solid gray)
        RGB 000,000,000     ;clrBtnText

;	dw	0			;Unused words
;	dw	0
;	dw	0
;	dw	0
;	dw	0
;	dw	0
	dw	0
	dw	0


#include "lab3d.h"

static int jmptable[10];

picrot(posxs, posys, poszs, angs)
unsigned int posxs, posys;
int poszs, angs;
{
	unsigned char shadecoffs;
	char xdir, ydir;
	int i, j, k, x, y, yf0, picplc, yc0, stat;
	int linesatonce, yy, endyy, temp, angle;
	long x1, y1, x2, y2, yinc, xinc, tan1, tan2, x1st, y2st;
	long y1add, x2add;
	unsigned int yl0, picseg, minheight, shadeioffs;

	_asm \
	{
		cld
		mov cx, 2048
		mov ax, seg tempbuf
		mov es, ax
		mov di, offset tempbuf
		xor ax, ax
		rep stosw
	}
	minheight = 65535;
	if (vidmode == 0)
	{
		lincalc[0] = 20;
		lincalc[1] = 276;
		lincalc[2] = 340;
		lincalc[3] = 148;
		lincalc[4] = 308;
		linbits[0] = 0;
		linbits[1] = 0;
		linbits[2] = 0;
		linbits[3] = 128;
		linbits[4] = 32;
		lastline = 5;
	}
	else
	{
		lincalc[0] = 0;
		lincalc[1] = 256;
		lincalc[2] = 320;
		lincalc[3] = 352;
		lincalc[4] = 356;
		lincalc[5] = 357;
		lincalc[6] = 358;
		lincalc[7] = 359;
		lincalc[8] = 128;
		lincalc[9] = 288;
		lincalc[10] = 336;
		lincalc[11] = 354;
		linbits[0] = 0;
		linbits[1] = 0;
		linbits[2] = 0;
		linbits[3] = 0;
		linbits[4] = 0;
		linbits[5] = 0;
		linbits[6] = 0;
		linbits[7] = 0;
		linbits[8] = 128;
		linbits[9] = 32;
		linbits[10] = 16;
		linbits[11] = 2;
		lastline = 12;
	}
	numline = 0;
	x2add = (((long)posxs)<<6);
	y1add = (((long)posys)<<6);
	x1st = x2add & 0xffff0000;
	y2st = y1add & 0xffff0000;
	while (numline < lastline)
	{
		i = lincalc[numline];
		angle = (radarang[i]+angs+2048)&2047;
		tan1 = tantable[(2560-angle)&1023];
		tan2 = tantable[angle&1023];
		if (tan1 != 0)
		{
			x1 = x1st;
			y1 = (posxs&1023);
			yinc = tan2;
			xdir = 1;
			if (((angle+512)&1024) > 0)
			{
				xdir = -1;
				yinc = -yinc;
				x1 += 65535;
				y1 = 1024 - y1;
			}
			_asm \
			{
				test byte ptr tan2[3], 0x80
				jz skiplabstan2
				not word ptr tan2[0]
				not word ptr tan2[2]
				add word ptr tan2[0], 1
				adc word ptr tan2[2], 0
skiplabstan2:

				mov di, word ptr y1[0]            ;y1 = (y1 * tan2)>>10
				mov ax, di
				mul word ptr tan2[0]
				mov bx, dx
				mov si, ax
				mov ax, di
				mul word ptr tan2[2]
				add bx, ax
				adc dx, 0
				mov cl, 10
				ror dx, cl
				ror bx, cl
				ror si, cl
				and si, 0x003f
				mov ax, bx
				and al, 0xc0
				or si, ax
				mov word ptr y1[0], si
				and bx, 0x003f
				and dl, 0xc0
				or bx, dx
				mov word ptr y1[2], bx
			}
			if ((angle&1024) == 0)
				y1 = -y1;
			y1 += y1add;
		}
		if (tan2 != 0)
		{
			y2 = y2st;
			x2 = (posys&1023);
			xinc = tan1;
			ydir = 1;
			if ((angle&1024) > 0)
			{
				ydir = -1;
				xinc = -xinc;
				y2 += 65535;
				x2 = 1024 - x2;
			}
			_asm \
			{
				test byte ptr tan1[3], 0x80
				jz skiplabstan1
				not word ptr tan1[0]
				not word ptr tan1[2]
				add word ptr tan1[0], 1
				adc word ptr tan1[2], 0
skiplabstan1:

				mov di, word ptr x2[0]            ;x2 = (x2 * tan1)>>10
				mov ax, di
				mul word ptr tan1[0]
				mov bx, dx
				mov si, ax
				mov ax, di
				mul word ptr tan1[2]
				add bx, ax
				adc dx, 0
				mov cl, 10
				ror dx, cl
				ror bx, cl
				ror si, cl
				and si, 0x003f
				mov ax, bx
				and al, 0xc0
				or si, ax
				mov word ptr x2[0], si
				and bx, 0x003f
				and dl, 0xc0
				or bx, dx
				mov word ptr x2[2], bx
			}
			if (((angle+512)&1024) == 0)
				x2 = -x2;
			x2 += x2add;
		}
		stat = 0;
		_asm \
		{
			mov al, xdir
			add byte ptr x1[2], al
			mov ax, word ptr yinc[0]
			mov dx, word ptr yinc[2]
			add word ptr y1[0], ax
			adc word ptr y1[2], dx
			mov ax, word ptr xinc[0]
			mov dx, word ptr xinc[2]
			add word ptr x2[0], ax
			adc word ptr x2[2], dx
			mov al, ydir
			add byte ptr y2[2], al
		}
		if (tan2 == 0)
			_asm \
			{
begingetline1:
				mov ax, word ptr x1[2]
				mov cl, 6
				shl ax, cl
				mov si, word ptr y1[2]
				add si, ax
				mov byte ptr ds:tempbuf[si], 1
				shl si, 1
				mov bx, word ptr ds:board[si]
				and bx, 1023
				mov al, byte ptr ds:bmpkind[bx]
				mov byte ptr stat[0], al
				cmp al, 1
				je endcheck1
				mov al, xdir
				add byte ptr x1[2], al
				jmp begingetline1
endcheck1:
			}
		else if (tan1 == 0)
			_asm \
			{
begingetline2:
				mov ax, word ptr x2[2]
				mov cl, 6
				shl ax, cl
				mov si, word ptr y2[2]
				add si, ax
				mov byte ptr ds:tempbuf[si], 1
				shl si, 1
				mov bx, word ptr ds:board[si]
				and bx, 1023
				mov al, byte ptr ds:bmpkind[bx]
				mov byte ptr stat[1], al
				cmp al, 1
				je endcheck2
				mov al, ydir
				add byte ptr y2[2], al
				jmp begingetline2
endcheck2:
			}
		else
			_asm \
			{
				mov bl, xdir
				mov bh, ydir
				mov cl, 6
				cmp bl, 0
				lahf
				mov ch, ah
begcheck:
				mov si, word ptr x1[0]
				mov dx, word ptr x1[2]
				sub si, word ptr x2[0]
				sbb dx, word ptr x2[2]
				lahf
				xor ah, ch
				test ah, 0x80
				jz startcheck2

				mov si, word ptr x1[2]
				shl si, cl
				add si, word ptr y1[2]
				mov byte ptr tempbuf[si], 1
				shl si, 1
				mov si, word ptr board[si]
				and si, 1023
				mov al, byte ptr bmpkind[si]
				cmp al, 1
				je endcheck3a
				add byte ptr x1[2], bl
				mov ax, word ptr yinc[0]
				mov dx, word ptr yinc[2]
				add word ptr y1[0], ax
				adc word ptr y1[2], dx
				jmp begcheck
startcheck2:
				mov si, word ptr x2[2]
				shl si, cl
				add si, word ptr y2[2]
				mov byte ptr tempbuf[si], 1
				shl si, 1
				mov si, word ptr board[si]
				and si, 1023
				mov al, byte ptr bmpkind[si]
				cmp al, 1
				je endcheck3b
				add byte ptr y2[2], bh
				mov ax, word ptr xinc[0]
				mov dx, word ptr xinc[2]
				add word ptr x2[0], ax
				adc word ptr x2[2], dx
				jmp begcheck
endcheck3a:
				mov byte ptr stat[0], al
				jmp endcheck3c
endcheck3b:
				mov byte ptr stat[1], al
endcheck3c:
			}
		if ((stat&0xff) == 1)
		{
			_asm \
			{
;yl0 = (int)(((((x1>>6)-((long)posxs))>>2)*sintable[(angs+512)&2047]
;            +(((y1>>6)-((long)posys))>>2)*sintable[angs])>>16);
				mov ax, word ptr y1[0]
				mov di, word ptr y1[2]
				mov cl, 6
				ror ax, cl
				ror di, cl
				and ax, 0x03ff
				and di, 0xfc00
				or ax, di
				sub ax, posys
				jnc skipnegate1a
				neg ax
skipnegate1a:
				mov bx, angs
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov cx, word ptr sintable[bx]
				mul cx
				mov si, dx
				mov ax, word ptr x1[0]
				mov di, word ptr x1[2]
				mov cl, 6
				ror ax, cl
				ror di, cl
				and ax, 0x03ff
				and di, 0xfc00
				or ax, di
				sub ax, posxs
				jnc skipnegate1b
				neg ax
skipnegate1b:
				mov bx, angs
				add bx, 512
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov cx, word ptr sintable[bx]
				mul cx
				mov ax, angle
				mov bx, angs
				xor ax, bx
				test ax, 1024
				jz skipnegate1c
				neg si
skipnegate1c:
				mov ax, angle
				mov bx, angs
				add ax, 512
				add bx, 512
				xor ax, bx
				test ax, 1024
				jz skipnegate1d
				neg dx
skipnegate1d:
				add si, dx
				shr si, 1
				shr si, 1
				mov yl0, si
			}
			j = ((int)(board[x1>>16][y1>>16]-1)&1023);
			picplc = ((((int)y1)>>4)&0xfff);
			if (((angle+512)&2047) >= 1024)
			{
				picplc ^= 0xfff;
				k = board[(x1+1)>>16][y1>>16];
			}
			else
				k = board[(x1-1)>>16][y1>>16];
			if ((k&8192) == 0)
			{
				k &= 1023;
				if ((k >= door1) && (k <= door1+5)) j = doorside1-1;
				if ((k >= door2) && (k <= door2+5)) j = doorside2-1;
				if ((k >= door3) && (k <= door3+7)) j = doorside3-1;
				if ((k >= door4) && (k <= door4+6)) j = doorside4-1;
				if ((k >= door5) && (k <= door5+7)) j = doorside5-1;
			}
			_asm \
			{
				mov al, byte ptr x1[2]
				mov ah, byte ptr y1[2]
				mov bx, i
				shl bx, 1
				mov linplc[bx], ax
			}
		}
		if ((stat&0xff00) == 256)
		{
			_asm \
			{
;yl0 = (int)(((((x2>>6)-((long)posxs))>>2)*sintable[(angs+512)&2047]
;            +(((y2>>6)-((long)posys))>>2)*sintable[angs])>>16);

				mov ax, word ptr y2[0]
				mov di, word ptr y2[2]
				mov cl, 6
				ror ax, cl
				ror di, cl
				and ax, 0x03ff
				and di, 0xfc00
				or ax, di
				sub ax, posys
				jnc skipnegate2a
				neg ax
skipnegate2a:
				mov bx, angs
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov cx, word ptr sintable[bx]
				mul cx
				mov si, dx
				mov ax, word ptr x2[0]
				mov di, word ptr x2[2]
				mov cl, 6
				ror ax, cl
				ror di, cl
				and ax, 0x03ff
				and di, 0xfc00
				or ax, di
				sub ax, posxs
				jnc skipnegate2b
				neg ax
skipnegate2b:
				mov bx, angs
				add bx, 512
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov cx, word ptr sintable[bx]
				mul cx
				mov ax, angle
				mov bx, angs
				add ax, 512
				add bx, 512
				xor ax, bx
				test ax, 1024
				jz skipnegate2c
				neg dx
skipnegate2c:
				mov ax, angle
				mov bx, angs
				xor ax, bx
				test ax, 1024
				jz skipnegate2d
				neg si
skipnegate2d:
				add si, dx
				shr si, 1
				shr si, 1
				mov yl0, si
			}
			j = ((int)(board[x2>>16][y2>>16]-1)&1023);
			picplc = ((((int)x2)>>4)&0xfff);
			if (angle < 1024)
			{
				picplc ^= 0xfff;
				k = board[x2>>16][(y2-1)>>16];
			}
			else
				k = board[x2>>16][(y2+1)>>16];
			if ((k&8192) > 0)
			{
				k &= 1023;
				if ((k >= door1) && (k <= door1+5)) j = doorside1-1;
				if ((k >= door2) && (k <= door2+5)) j = doorside2-1;
				if ((k >= door3) && (k <= door3+7)) j = doorside3-1;
				if ((k >= door4) && (k <= door4+6)) j = doorside4-1;
				if ((k >= door5) && (k <= door5+7)) j = doorside5-1;
			}
			_asm \
			{
				mov al, byte ptr x2[2]
				mov ah, byte ptr y2[2]
				or al, 0x80
				mov bx, i
				shl bx, 1
				mov linplc[bx], ax
			}
		}
		if (yl0 <= 10)
			height[i] = 16383, minheight = 0;
		else if (yl0 < 24000)
			height[i] = (int)(163840L/((long)yl0));
		else
			height[i] = 0;
		if (height[i] < minheight)
			minheight = height[i];
		if ((j&1023) == invisible-1)
			minheight = 0;
		linum[i] = picplc;
		if (waterstat > 0)
		{
			if ((j&1023) == fountain-1)
				j += (animate2+1);
		}
		walnum[i] = j;
		if ((stat&255) == 1)
			walnum[i] |= 16384;
		if (linbits[numline] > 1)
		{
			if ((linplc[i-linbits[numline]] == linplc[i]) && (height[i] != 0) && (height[i] != 16383))
			{
				for(j=(i-linbits[numline]+1);j<i;j++)
					linplc[j] = -1;
				for(j=(linbits[numline]>>1);j>0;j>>=1)
					for(k=(i-linbits[numline])+j;k<i;k+=j)
						if (linplc[k] == -1)
						{
							height[k] = ((height[k-j]+height[k+j])>>1);
							linum[k] = (int)(((((long)linum[k-j])*((long)height[k-j])+((long)linum[k+j])*((long)height[k+j]))/(labs(((long)height[k-j])+((long)height[k+j]))+1)));
							walnum[k] = walnum[i];
							linplc[k] = linplc[i];
						}
			}
			else
			{
				linbits[lastline] = (linbits[numline]>>1);
				lincalc[lastline] = i-linbits[lastline];
				lastline++;
			}
			if ((linplc[i+linbits[numline]] == linplc[i]) && (height[i] != 0) && (height[i] != 16383))
			{
				for(j=(i+linbits[numline]-1);j>i;j--)
					linplc[j] = -1;
				for(j=(linbits[numline]>>1);j>0;j>>=1)
					for(k=(i+linbits[numline])-j;k>i;k-=j)
						if (linplc[k] == -1)
						{
							height[k] = ((height[k-j]+height[k+j])>>1);
							linum[k] = (int)(((((long)linum[k-j])*((long)height[k-j])+((long)linum[k+j])*((long)height[k+j]))/(labs(((long)height[k-j])+((long)height[k+j]))+1)));
							walnum[k] = walnum[i];
							linplc[k] = linplc[i];
						}
			}
			else
			{
				linbits[lastline] = (linbits[numline]>>1);
				lincalc[lastline] = i+linbits[lastline];
				lastline++;
			}
		}
		numline++;
	}
	_asm \
	{
		mov dx, 0x3c4
		mov ax, 0x0f02
		out dx, al
		inc dx
		mov al, ah
		out dx, al
		mov ax, 0xa000
		mov es, ax
		cld
	}
	if ((minheight != 0) && (minheight != 65535))
	{
		yy = halfheight-(int)(((long)(minheight>>2)*(long)poszs)>>6);
		endyy = yy + (minheight>>2);
		if (yy < 0) yy = 0;
		if (yy > halfheight) yy = halfheight;
		if (endyy < halfheight) endyy = halfheight;
		if (endyy > dside) endyy = dside;
		if (endyy > ((statusbar+1)>>1)) endyy = ((statusbar+1)>>1);
		yy = times90[yy];
		endyy = times90[endyy-halfheight];
	}
	else
	{
		if (vidmode == 0) yy = 9000, endyy = 0;
		else yy = 10800, endyy = 0;
	}
	if (vidmode == 0)
	{
		if (videotype == 0)
			_asm \
			{
				mov di, pageoffset
				mov cx, yy
				shr cx, 1
				mov ax, 0xe3e3
				rep stosw
				mov di, pageoffset
				add di, 9000
				mov bx, endyy
				add di, bx
				mov cx, 9000
				sub cx, bx
				shr cx, 1
				mov ax, 0x8484
				rep stosw
			}
		else
			_asm \
			{
				mov di, pageoffset
				mov cx, yy
				mov al, 0xe3
				rep stosb
				mov di, pageoffset
				add di, 9000
				mov bx, endyy
				add di, bx
				mov cx, 9000
				sub cx, bx
				mov al, 0x84
				rep stosb
			}
	}
	else
	{
		if (videotype == 0)
			_asm \
			{
				mov di, pageoffset
				mov cx, yy
				shr cx, 1
				mov ax, 0xe3e3
				rep stosw
				mov di, pageoffset
				add di, 10800
				mov bx, endyy
				add di, bx
				mov cx, scrsize
				sub cx, 10800
				sub cx, bx
				shr cx, 1
				mov ax, 0x8484
				rep stosw
			}
		else
			_asm \
			{
				mov di, pageoffset
				mov cx, yy
				mov al, 0xe3
				rep stosb
				mov di, pageoffset
				add di, 10800
				mov bx, endyy
				add di, bx
				mov cx, scrsize
				sub cx, 10800
				sub cx, bx
				mov al, 0x84
				rep stosb
			}
	}
	_asm \
	{
		mov dx, 0x3c4
		mov al, 0x2
		out dx, al

		mov bx, lside
		mov cx, rside
		sub cx, bx
		shl bx, 1
fixlinestart:
		and word ptr linum[bx], 0xfc0
		add bx, 2
		loop fixlinestart
	}
	for(i=lside;i<rside;i++)
	{
		yf0 = (height[i]>>2);
		picplc = linum[i];
		if ((walnum[i]&1023) == invisible-1)
			yf0 = 0;
		_asm \
		{
			mov bx, i
			shl bx, 1
			mov ax, walnum[bx]
			mov cl, 5
			shr ah, cl
			and ah, 2
			mov al, ah
			mov shadecoffs, al
			mov shadeioffs, ax
		}
		if ((walnum[i]&1023) == map-1)
		{
			shadecoffs = 0;
			shadeioffs = 0;
		}
		if (memtype[walnum[i]&1023] != 65535)
		{
			if (vms == 1)
				if (memtype[walnum[i]&1023] == 0)
					loadvms(walnum[i]&1023);
			if ((emswalls > 0) && ((memtype[walnum[i]&1023]&1024) > 0))
				emsetpages(memtype[walnum[i]&1023]-1024,0);
			else if ((xmswalls > 0) && ((memtype[walnum[i]&1023]&2048) > 0))
				xmsmemmove(xmshandle,((long)(memtype[walnum[i]&1023]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
		}
		picseg = walseg[walnum[i]&1023];
		_asm \
		{
			mov ax, 0xa000
			mov es, ax
			mov dx, 0x3c5
			mov cl, byte ptr i
			and cl, 3
			mov al, 1
			shl al, cl
			out dx, al
		}
		linesatonce = 0;
		if (((i&3) < 3) && (linum[i] == linum[i+1]) && (linplc[i] == linplc[i+1]))
		{
			_asm \
			{
				mov dx, 0x3c5
				mov cl, byte ptr i
				and cl, 3
				mov al, 3
				shl al, cl
				out dx, al
			}
			linesatonce = 1;
		}
		if (((i&3) < 2) && (linesatonce == 1) && (linum[i] == linum[i+2]) && (linplc[i] == linplc[i+2]))
		{
			_asm \
			{
				mov dx, 0x3c5
				mov cl, byte ptr i
				and cl, 3
				mov al, 7
				shl al, cl
				out dx, al
			}
			linesatonce = 2;
		}
		if (((i&3) == 0) && (linesatonce == 2) && (linum[i] == linum[i+3]) && (linplc[i] == linplc[i+3]))
		{
			_asm \
			{
				mov dx, 0x3c5
				mov al, 15
				out dx, al
			}
			linesatonce = 3;
		}
		_asm \
		{
			mov ax, yf0             ;yy = halfheight-(((long)yf0*poszs)>>6);
			mov bx, poszs
			mul bx
			mov cl, 6
			ror ax, cl
			ror dx, cl
			and ax, 0x03ff
			and dx, 0xfc00
			or ax, dx
			neg ax
			add ax, halfheight
			mov yy, ax
		}
		if ((yf0 > 0) && (yf0 < 64))
		{
			yy = times90[yy]+(i>>2)+pageoffset;
			_asm \
			{
				mov bx, yf0
				shl bx, 1
				mov cx, word ptr less64inc[bx]
				mov dx, cx
				shr dx, 1
				mov bl, dh
				mov si, picplc
				add bx, 192
				sub si, 192
				mov di, yy
				mov ah, shadecoffs
				push ds
				mov ds, picseg
start1:
				mov al, ds:[si+bx]
				add al, ah
				mov es:[di], al
				add dl, cl
				adc bl, ch
				jc end1

				mov al, ds:[si+bx]
				add al, ah
				mov es:[di+90], al
				add dl, cl
				adc bl, ch
				jc end1

				mov al, ds:[si+bx]
				add al, ah
				mov es:[di+180], al
				add dl, cl
				adc bl, ch
				jc end1

				mov al, ds:[si+bx]
				add al, ah
				mov es:[di+270], al
				add di, 360
				add dl, cl
				adc bl, ch
				jnc start1
end1:
				pop ds
			}
		}
		else if ((yf0 >= 64) && (yf0 < halfheight))
		{
			yy = times90[yy]+(i>>2)+pageoffset;
			_asm \
			{
				mov dx, 0
				mov si, picplc
				mov bx, yf0
				mov cl, 3
				shl bx, cl
				mov cx, si
				add cx, 64
				mov di, yy
				push ds
				mov ds, picseg
start2:
				lodsw
				add ax, shadeioffs
				cmp si, cx
				ja endit2
start2a:
				xor dh, dh
				add dx, bx
				cmp dh, 3
				je start2b
				ja start2c
				mov es:[di], al
				mov es:[di+90], ah
				add di, 180
				jmp start2
start2b:
				mov es:[di], al
				mov es:[di+90], al
				mov es:[di+180], ah
				add di, 270
				jmp start2
start2c:
				mov es:[di], al
				mov es:[di+90], al
				mov es:[di+180], ah
				mov es:[di+270], ah
				add di, 360
				jmp start2
endit2:
				pop ds
			}
		}
		else if ((yf0 >= halfheight) && (yf0 < 4096))
		{
			endyy = yy + yf0;
			if (yy < 0)
			{
				_asm \
				{
					mov ax, yy
					neg ax
					mov cl, 6
					rol ax, cl
					mov dx, ax
					and al, 0xc0
					and dx, 0x0f
					mov cx, yf0
					div cx
					add picplc, ax          ;dividend
					mov ax, yf0
					sub ax, dx              ;remainder
					mov yc0, ax
				}
				yy = (i>>2)+pageoffset;
			}
			else
			{
				yc0 = yf0-1;
				yy = times90[yy]+(i>>2)+pageoffset;
			}
			if (endyy <= dside)
			{
				_asm \
				{
					mov dx, yc0
					mov si, picplc
					mov cx, si
					and cx, 0x0fc0
					add cx, 64
					mov bx, yf0
					mov di, yy
					mov ax, bx
					dec ax
				}
				if (yf0 < 128)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart3:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart3
						xor dx, dx
start3:
						lodsw
						add ax, shadeioffs
						cmp si, cx
						jae skip3d
start3a:
						xor dh, dh
						add dx, bx
						cmp dh, 3
						je start3b
						ja start3c
						mov es:[di], al
						mov es:[di+90], ah
						add di, 180
						jmp start3
start3b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						add di, 270
						jmp start3
start3c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						mov es:[di+270], ah
						add di, 360
						jmp start3
skip3d:
						mov ah, 0x84
						je start3a
endit3:
						pop ds
					}
				else if (yf0 < 192)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart4:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart4
						xor dx, dx
start4:
						lodsw
						add ax, shadeioffs
						cmp si, cx
						jae start4d
start4a:
						xor dh, dh
						add dx, bx
						cmp dh, 5
						je start4b
						ja start4c
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						mov es:[di+270], ah
						add di, 360
						jmp start4
start4b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						add di, 450
						jmp start4
start4c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						mov es:[di+450], ah
						add di, 540
						jmp start4
start4d:
						mov ah, 0x84
						je start4a
endit4:
						pop ds
					}
				else if (yf0 < 256)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart5:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart5
						xor dx, dx
start5:
						lodsw
						add ax, shadeioffs
						cmp si, cx
						jae start5d
start5a:
						xor dh, dh
						add dx, bx
						cmp dh, 7
						je start5b
						ja start5c
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						mov es:[di+450], ah
						add di, 540
						jmp start5
start5b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						mov es:[di+360], ah
						mov es:[di+450], ah
						mov es:[di+540], ah
						add di, 630
						jmp start5
start5d:
						mov ah, 0x84
						je start5a
						jmp endit5
start5c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						mov es:[di+360], ah
						mov es:[di+450], ah
						mov es:[di+540], ah
						mov es:[di+630], ah
						add di, 720
						jmp start5
endit5:
						pop ds
					}
				else
					_asm \
					{
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
						jmp start6b
start6:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						add di, 360
						sub dx, 256
						js start6c
						or dh, dh
						jnz start6
start6b:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns start6b
start6c:
						lodsb
						add al, shadecoffs
						add dx, bx
						cmp si, cx
						jbe start6
						pop ds
					}
			}
			else
			{
				_asm \
				{
					mov dx, yc0
					mov si, picplc
					mov cx, pageoffset
					add cx, scrsize
					mov bx, yf0
					mov di, yy
				}
				if (yf0 < 128)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart7:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart7
						xor dx, dx
start7:
						lodsw
						add ax, shadeioffs
						cmp di, cx
						jbe start7a
						jmp endit7
start7a:
						xor dh, dh
						add dx, bx
						cmp dh, 3
						je start7b
						ja start7c
						mov es:[di], al
						mov es:[di+90], ah
						add di, 180
						jmp start7
start7b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						add di, 270
						jmp start7
start7c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						mov es:[di+270], ah
						add di, 360
						jmp start7
endit7:
						pop ds
					}
				else if (yf0 < 192)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart8:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart8
						xor dx, dx
start8:
						lodsw
						add ax, shadeioffs
						cmp di, cx
						ja endit8
start8a:
						xor dh, dh
						add dx, bx
						cmp dh, 5
						je start8b
						ja start8c
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], ah
						mov es:[di+270], ah
						add di, 360
						jmp start8
start8b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						add di, 450
						jmp start8
start8c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						mov es:[di+450], ah
						add di, 540
						jmp start8
endit8:
						pop ds
					}
				else if (yf0 < 256)
					_asm \
					{
						inc cx
						shl bx, 1
						shl bx, 1
						shl bx, 1
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
prestart9:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns prestart9
						xor dx, dx
start9:
						lodsw
						add ax, shadeioffs
						cmp di, cx
                        jna start9a
						jmp endit9
start9a:
						xor dh, dh
						add dx, bx
						cmp dh, 7
						je start9b
						ja start9c
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], ah
						mov es:[di+360], ah
						mov es:[di+450], ah
						add di, 540
						jmp start9
start9b:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						mov es:[di+360], ah
						mov es:[di+450], ah
						mov es:[di+540], ah
						add di, 630
						jmp start9
start9c:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						mov es:[di+360], ah
						mov es:[di+450], ah
						mov es:[di+540], ah
						mov es:[di+630], ah
						add di, 720
						jmp start9
endit9:
						pop ds
					}
				else
					_asm \
					{
						push ds
						mov ds, picseg
						lodsb
						add al, shadecoffs
						jmp start10b
start10:
						mov es:[di], al
						mov es:[di+90], al
						mov es:[di+180], al
						mov es:[di+270], al
						add di, 360
						cmp di, cx
						jae endit10
						sub dx, 256
						js start10c
						or dh, dh
						jnz start10
start10b:
						mov es:[di], al
						add di, 90
						sub dx, 64
						jns start10b
start10c:
						lodsb
						add al, shadecoffs
						add dx, bx
						jmp start10
endit10:
						pop ds
					}
			}
		}
		i += linesatonce;
	}
	for(k=0;k<bulnum;k++)
		if (bulkind[k] == 7)
		{
			x1 = ((long)bulx[k]-(long)posxs);
			y1 = ((long)buly[k]-(long)posys);
			if (labs(x1)+labs(y1) < 32768)
			{
				x1 >>= 2;
				y1 >>= 2;
				x2 = (((x1*sintable[bulang[k]])-(y1*sintable[(bulang[k]+512)&2047]))>>16);
				y2 = (((x1*sintable[(bulang[k]+512)&2047])+(y1*sintable[bulang[k]]))>>16);
				if ((x2|y2) != 0)
				{
					j = ((x2*clockspeed)<<11)/(x2*x2+y2*y2);
					bulang[k] += j;
				}
			}
		}
	for(i=0;i<mnum;i++)
	{
		_asm \
		{
			mov bx, i
			shl bx, 1
			mov ax, word ptr mposx[bx]
			mov dx, word ptr mposy[bx]
			mov cl, 10
			shr ax, cl
			shr dx, cl
			mov cl, 6
			shl ax, cl
			add ax, dx
			mov j, ax
		}
		if (tempbuf[j] != 0)
		{
			temp = mstat[i];
			if ((temp != monbal) && (temp != monhol) && (temp != monke2) && (temp != mondog))
				for(k=0;k<bulnum;k++)
					if ((bulkind[k] == 3) || (bulkind[k] == 4))
					{
						x1 = ((long)bulx[k]-(long)mposx[i]);
						y1 = ((long)buly[k]-(long)mposy[i]);
						if (labs(x1)+labs(y1) < 32768)
						{
							x1 >>= 2;
							y1 >>= 2;
							x2 = (((x1*sintable[bulang[k]])-(y1*sintable[(bulang[k]+512)&2047]))>>16);
							y2 = (((x1*sintable[(bulang[k]+512)&2047])+(y1*sintable[bulang[k]]))>>16);
							if ((x2|y2) != 0)
							{
								j = ((x2*clockspeed)<<11)/(x2*x2+y2*y2);
								if (temp == monali)
									j >>= 1;
								if ((temp == monzor) || (temp == monan2))
									j = -j;
								if (temp == monan3)
								{
									if (mshock[i] > 0)
										j = 0;
									else
										j = -j;
								}
								bulang[k] += j;
							}
						}
					}
			switch(temp)
			{
				case monken:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monken+oscillate5);
					break;
				case monbal:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monbal+animate4);
					break;
				case mongho:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,mongho+oscillate3);
					break;
				case hive:
					if (((mshock[i]&16384) == 0) && (mstat[i] > 0))
						checkobj(mposx[i],mposy[i],posxs,posys,angs,hive);
					else
					{
						checkobj(mposx[i],mposy[i],posxs,posys,angs,hivetohoney+((mshock[i]-16384)>>5));
						mshock[i] += clockspeed;
						if (((mshock[i]-16384)>>5) >= 5)
						{
							board[mposx[i]>>10][mposy[i]>>10] = honey+1024;
							mnum--;
							moldx[i] = moldx[mnum];
							moldy[i] = moldy[mnum];
							mposx[i] = mposx[mnum];
							mposy[i] = mposy[mnum];
							mgolx[i] = mgolx[mnum];
							mgoly[i] = mgoly[mnum];
							mstat[i] = mstat[mnum];
							mshot[i] = mshot[mnum];
							mshock[i] = mshock[mnum];
						}
					}
					break;
				case monske:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monske+oscillate3);
					break;
				case monmum:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monmum+animate8);
					break;
				case mongre:
					if (mshock[i] > 0)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,mongre+5);
					else
						checkobj(mposx[i],mposy[i],posxs,posys,angs,mongre+oscillate5);
					break;
				case monrob:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monrob+animate10);
					break;
				case monro2:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monro2+animate8);
					break;
				case mondog:
					if (boardnum >= 10)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,mondog+animate15+15);
					else
						checkobj(mposx[i],mposy[i],posxs,posys,angs,mondog+animate15);
					break;
				case monwit:
					if (mshock[i] > 0)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monwit);
					else
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monwit+oscillate5);
					break;
				case monand:
					if (mshock[i] > 0)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monand+3+animate2);
					else
					{
						j = monand;
						if ((posxs > mposx[i]) == (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] > moldy[i]))
								j = monand+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] < moldy[i]))
								j = monand+2;
							if ((posxs < mposx[i]) && (posys < mposy[i]) && (j != monand))
								j = monand+monand+3-j;
						}
						if ((posxs > mposx[i]) != (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] < moldy[i]))
								j = monand+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] > moldy[i]))
								j = monand+2;
							if ((posxs > mposx[i]) && (posys < mposy[i]) && (j != monand))
								j = monand+monand+3-j;
						}
						checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
					}
					break;
				case monali:
					if (mshock[i] > 0)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monali+1);
					else
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monali);
					break;
				case monhol:
					if (mshock[i] > 0)
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monhol);
					else
						checkobj(mposx[i],mposy[i],posxs,posys,angs,monhol+oscillate3+1);
					break;
				case monzor:
					if ((mshock[i]&8192) > 0)
					{
						if (mshock[i] < 8192+monzor+11)
							mshock[i] = 0;
						else if (mshock[i] <= 8192+monzor+12)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,mshock[i]&1023);
						else
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monzor+13+animate2);
					}
					if (mshock[i] < 8192)
					{
						if (mshock[i] > 0)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monzor+10);
						else
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monzor+animate10);
					}
					if ((mshock[i]&16384) > 0)
					{
						j = mshock[i]&1023;
						if (mshock[i] <= 16384+monzor+12)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
						else
						{
							mshock[i] = monzor+13+8192;
							if (clockspeed > 0)
							{
								mshock[i] += 512/clockspeed;
								if (mshock[i] > 16383)
									mshock[i] = 16383;
							}
						}
					}
					break;
				case monbat:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monbat+oscillate3);
					break;
				case monear:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monear+oscillate5);
					break;
				case monbee:
					checkobj(mposx[i]+(rand()&127)-64,mposy[i]+(rand()&127)-64,posxs,posys,angs,monbee+animate6);
					break;
				case monspi:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,monspi+animate6);
					break;
				case mongr2:
					checkobj(mposx[i],mposy[i],posxs,posys,angs,mongr2+animate11);
					break;
				case monke2:
					if ((mshock[i]&8192) > 0)
					{
						j = (mshock[i]&1023);
						if (mshock[i] < 8192+monke2+7)
							mshock[i] = 0;
						else if (mshock[i] <= 8192+monke2+13)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,mshock[i]&1023);
					}
					if (mshock[i] < 8192)
					{
						if (mshock[i] > 0)
						{
							if (mshot[i] < 32)
								checkobj(mposx[i],mposy[i],posxs,posys,angs,monke2+oscillate3+3);
							else
								checkobj(mposx[i],mposy[i],posxs,posys,angs,monke2+6);
						}
						else
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monke2+oscillate3);
					}
					if ((mshock[i]&16384) > 0)
					{
						j = mshock[i]&1023;
						if (mshock[i] <= 16384+monke2+13)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
						else
						{
							mshock[i] = monke2+13+8192;
							x = (mgolx[i]>>10);
							y = (mgoly[i]>>10);
							board[moldx[i]>>10][moldy[i]>>10] &= 0xbfff;
							board[x][y] &= 0xbfff;
							for(k=0;k<16;k++)
							{
								j = (rand()&3);
								if ((j == 0) && ((board[x-1][y]&0x4c00) == 1024))
									x--;
								if ((j == 1) && ((board[x+1][y]&0x4c00) == 1024))
									x++;
								if ((j == 2) && ((board[x][y-1]&0x4c00) == 1024))
									y--;
								if ((j == 3) && ((board[x][y+1]&0x4c00) == 1024))
									y++;
							}
							board[x][y] |= 0x4000;
							mposx[i] = (((unsigned)x)<<10)+512;
							mposy[i] = (((unsigned)y)<<10)+512;
							mgolx[i] = mposx[i];
							mgoly[i] = mposy[i];
						}
					}
					break;
				case monan2:
					if (mshock[i] > 0)
					{
						if (mshot[i] > 32)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monan2+3);
						else
							checkobj(mposx[i],mposy[i],posxs,posys,angs,monan2+4+animate2);
					}
					else
					{
						j = monan2;
						if ((posxs > mposx[i]) == (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] > moldy[i]))
								j = monan2+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] < moldy[i]))
								j = monan2+2;
							if ((posxs < mposx[i]) && (posys < mposy[i]) && (j != monan2))
								j = monan2+monan2+3-j;
						}
						if ((posxs > mposx[i]) != (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] < moldy[i]))
								j = monan2+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] > moldy[i]))
								j = monan2+2;
							if ((posxs > mposx[i]) && (posys < mposy[i]) && (j != monan2))
								j = monan2+monan2+3-j;
						}
						checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
					}
					break;
				case monan3:
					if ((mshock[i]&8192) > 0)
					{
						if (mshock[i] < 8192+monan3+3)
							mshock[i] = 0;
						else if (mshock[i] <= 8192+monan3+8)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,mshock[i]&1023);
					}
					if (mshock[i] == 0)
					{
						j = monan3;
						if ((posxs > mposx[i]) == (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] > moldy[i]))
								j = monan3+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] < moldy[i]))
								j = monan3+2;
							if ((posxs < mposx[i]) && (posys < mposy[i]) && (j != monan3))
								j = monan3+monan3+3-j;
						}
						if ((posxs > mposx[i]) != (posys > mposy[i]))
						{
							if ((mgolx[i] < moldx[i]) || (mgoly[i] < moldy[i]))
								j = monan3+1;
							if ((mgolx[i] > moldx[i]) || (mgoly[i] > moldy[i]))
								j = monan3+2;
							if ((posxs > mposx[i]) && (posys < mposy[i]) && (j != monan3))
								j = monan3+monan3+3-j;
						}
						checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
					}
					if ((mshock[i]&16384) > 0)
					{
						j = mshock[i]&1023;
						if (mshock[i] <= 16384+monan3+8)
							checkobj(mposx[i],mposy[i],posxs,posys,angs,j);
						else
						{
							mshock[i] = monan3+8+8192;
							if (clockspeed > 0)
							{
								mshock[i] += 240/clockspeed;
								if (mshock[i] > 16383)
									mshock[i] = 16383;
							}
						}
					}
					break;
			}
		}
	}
	for(k=0;k<4096;k++)
		if (tempbuf[k] != 0)
		{
			_asm \
			{
				mov bx, k
				mov tempbuf[bx], 0
				shl bx, 1
				mov ax, word ptr board[bx]
				and ax, 1023
				mov i, ax
			}
			if (bmpkind[i] >= 2)
			{
				if ((i == exitsign) || (i == soda) || (i == tentacles) || (i == tablecandle))
					i += animate2;
				if (i == minicolumn)
					i += animate4;
				j = sortcnt;
				checkobj(((k&0xfc0)<<4)+512,((k&63)<<10)+512,posxs,posys,angs,i);
				if (bmpkind[i] == 4)
				{
					sortcnt = j+1;
					sortang[sortcnt-1] = k;
					if (i == door3+1)
					{
						if (sorti[sortcnt-1] > 512)
							sortbnum[sortcnt-1] = door3;
						else if (sorti[sortcnt-1] > 470)
							sortbnum[sortcnt-1] = door3+1;
						else if (sorti[sortcnt-1] > 431)
							sortbnum[sortcnt-1] = door3+2;
						else if (sorti[sortcnt-1] > 395)
							sortbnum[sortcnt-1] = door3+3;
						else if (sorti[sortcnt-1] > 362)
							sortbnum[sortcnt-1] = door3+4;
						else if (sorti[sortcnt-1] > 332)
							sortbnum[sortcnt-1] = door3+5;
						else if (sorti[sortcnt-1] > 279)
							sortbnum[sortcnt-1] = door3+6;
						else if (sorti[sortcnt-1] > 256)
							sortbnum[sortcnt-1] = door3+7;
						else
							sortcnt--;
					}
				}
			}
		}
	totalsortcnt = sortcnt;
	for(i=0;i<totalsortcnt;i++)
	{
		temp = 0;
		for(j=0;j<sortcnt;j++)
			if (sorti[j] < sorti[temp])
				temp = j;
		k = sortbnum[temp];
		if (bmpkind[k] == 2)
		{
			if (k == warp)
				pictur(sortang[temp],halfheight-(((sorti[temp]>>2)*poszs)>>6)+(sorti[temp]>>3),sorti[temp],(int)((totalclock<<2)&2047),warp);
			else if (k == bul8fly)
				pictur(sortang[temp],halfheight-(((sorti[temp]>>2)*poszs)>>6)+(sorti[temp]>>3),sorti[temp],(int)((totalclock<<3)&2047),bul8fly+animate2);
			else if (k == bul10fly)
				pictur(sortang[temp],halfheight-(((sorti[temp]>>2)*poszs)>>6)+(sorti[temp]>>3),sorti[temp],(int)((totalclock<<3)&2047),bul10fly+animate2);
			else
				spridraw(sortang[temp]-(sorti[temp]>>3),halfheight-(((sorti[temp]>>2)*poszs)>>6),sorti[temp],k);
			if (k == fan)
				pictur(sortang[temp],halfheight-(((sorti[temp]>>2)*poszs)>>6)+(sorti[temp]>>3),sorti[temp],(int)((totalclock<<2)&2047),fan+1);
		}
		if (bmpkind[k] == 4)
		{
			if (k == slotto)
				if (slottime > 0)
					k++;
			doordraw((unsigned)(sortang[temp]>>6),(unsigned)(sortang[temp]&63),posxs,posys,poszs,angs,k);
		}
		sortcnt--;
		sortang[temp] = sortang[sortcnt];
		sorti[temp] = sorti[sortcnt];
		sortbnum[temp] = sortbnum[sortcnt];
	}
}

spridraw(x, y, siz, walnume)
int x, y, siz, walnume;
{
	int i, lsid, rsid, xc, picplc, linesatonce, realsiz;

	realsiz = (siz>>2);
	if (memtype[walnume-1] != 65535)
	{
		if (vms == 1)
			if (memtype[walnume-1] == 0)
				loadvms(walnume-1);
		if ((emswalls > 0) && ((memtype[walnume-1]&1024) > 0))
			emsetpages(memtype[walnume-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[walnume-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[walnume-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	picplc = 0;
	lsid = lborder[walnume];
	rsid = rborder[walnume];
	_asm \
	{
		mov dx, 0x3c4
		mov al, 0x2
		out dx, al
	}
	xc = 64;
	if (realsiz < 64)
	{
		for(i=0;i<64;i++)
		{
			if (xc >= 64)
			{
				if ((picplc >= lsid) && (picplc < rsid) && (x >= lside) && (x < rside) && (siz >= height[x]))
				{
					_asm \
					{
						mov dx, 0x3c5
						mov cl, byte ptr x
						and cl, 3
						mov al, 1
						shl al, cl
						out dx, al
					}
					vline(x,y,realsiz,walnume,picplc);
				}
				xc -= 64;
				x++;
			}
			picplc += 64;
			xc += realsiz;
		}
	}
	else if (realsiz >= 64)
	{
		for(i=x;i<x+realsiz;i++)
		{
			if ((i >= lside) && (i < rside))
			{
				if ((siz >= height[i]) && (picplc >= lsid) && (picplc < rsid))
					sprinum[i] = picplc;
				else
					sprinum[i] = 65535;
			}
			xc += 64;
			if (xc >= realsiz)
			{
				xc -= realsiz;
				picplc += 64;
			}
		}
		if ((x+realsiz) < rside)
			sprinum[x+realsiz] = 65534;
		for(i=x;i<x+realsiz;i++)
			if ((i >= lside) && (i < rside) && (sprinum[i] != 65535))
			{
				_asm \
				{
					mov dx, 0x3c5
					mov cl, byte ptr i
					and cl, 3
					mov al, 1
					shl al, cl
					out dx, al
				}
				linesatonce = 0;
				if (((i&3) < 3) && (sprinum[i] == sprinum[i+1]))
				{
					_asm \
					{
						mov dx, 0x3c5
						mov cl, byte ptr i
						and cl, 3
						mov al, 3
						shl al, cl
						out dx, al
					}
					linesatonce = 1;
					if (((i&3) < 2) && (sprinum[i] == sprinum[i+2]))
					{
						_asm \
						{
							mov dx, 0x3c5
							mov cl, byte ptr i
							and cl, 3
							mov al, 7
							shl al, cl
							out dx, al
						}
						linesatonce = 2;
						if (((i&3) == 0) && (sprinum[i] == sprinum[i+3]))
						{
							_asm \
							{
								mov dx, 0x3c5
								mov al, 15
								out dx, al
							}
							linesatonce = 3;
						}
					}
				}
				vline(i,y,realsiz,walnume,sprinum[i]);
				i += linesatonce;
			}
	}
}

pictur(x,y,siz,ang,walnume)
int x, y, siz, ang, walnume;
{
	int i, lsid, rsid, numvpixels, xplc, linesatonce, realsiz;
	int x0, y0, x1, y1, x2, y2, waitodraw;
	unsigned p, mapmaskval, picseg;
	unsigned xc0, yc0, xi0, yi0, xa0, ya0, xi1, yi1, xa1, ya1;
	unsigned long templong;

	if ((siz < 1) || (siz > 16383) || (y+(siz>>3) < 0) || (y-(siz>>3) >= dside))
		return(-1);
	if (siz < (128<<2))
		linesatonce = 1;
	else if (siz < (256<<2))
		linesatonce = 2;
	else
		linesatonce = 4;
	realsiz = (int)((((long)siz)*41)/116);
	if (realsiz == 0)
		return(-1);
	if (realsiz > 4095)
		realsiz = 4095;
	if (memtype[walnume-1] != 65535)
	{
		if (vms == 1)
			if (memtype[walnume-1] == 0)
				loadvms(walnume-1);
		if ((emswalls > 0) && ((memtype[walnume-1]&1024) > 0))
			emsetpages(memtype[walnume-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[walnume-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[walnume-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	lsid = lborder[walnume];
	rsid = rborder[walnume];
	picseg = walseg[walnume-1];
	x0 = (int)(sintable[(3328-ang)&2047]>>10);
	y0 = (int)(sintable[(1280+ang)&2047]>>10);
	x1 = (int)(sintable[(2816-ang)&2047]>>10);
	y1 = (int)(sintable[(1792+ang)&2047]>>10);
	templong = (((long)abs(x1-x0)*linesatonce)<<16)/((long)realsiz);
	_asm \
	{
		mov ax, word ptr templong[0]
		mov dx, word ptr templong[2]
		mov xi0, ax
		mov xa0, dx
	}
	templong = (((long)abs(y1-y0)*linesatonce)<<16)/((long)realsiz);
	_asm \
	{
		mov ax, word ptr templong[0]
		mov dx, word ptr templong[2]
		mov yi0, ax
		mov ya0, dx
	}
	templong = (((long)abs(x0+x1))<<16)/((long)realsiz);
	_asm \
	{
		mov ax, word ptr templong[0]
		mov dx, word ptr templong[2]
		mov xi1, ax
		mov xa1, dx
	}
	templong = (((long)abs(y0+y1))<<16)/((long)realsiz);
	_asm \
	{
		mov ax, word ptr templong[0]
		mov dx, word ptr templong[2]
		mov yi1, ax
		mov ya1, dx
	}
	xc0 = 32768;
	yc0 = 32768;
	x2 = x0+32; y2 = y0+32;
	p = (y-(realsiz>>1));
	numvpixels = realsiz;
	if (p >= 32768)
		waitodraw = p;
	else
		waitodraw = 0;
	if (p+numvpixels >= dside)
		numvpixels = dside-p;
	p = (p*90)+((x-(realsiz>>1))>>2)+pageoffset;
	xplc = x-(realsiz>>1);
	if (linesatonce == 1) mapmaskval = (xplc&3);
	else if (linesatonce == 2) mapmaskval = (xplc&2);
	else mapmaskval = 0;
	_asm \
	{
		mov ax, xi1
		mov word ptr cs:[piclab1a+2], ax
		mov word ptr cs:[piclab1b+2], ax
		mov ax, yi1
		mov word ptr cs:[piclab3a+2], ax
		mov word ptr cs:[piclab3b+2], ax
		mov ax, xa1
		mov byte ptr cs:[piclab2a+2], al
		mov byte ptr cs:[piclab2b+2], al
		mov ax, ya1
		mov byte ptr cs:[piclab4a+1], al
		mov byte ptr cs:[piclab4b+1], al
		mov ax, x0
		neg ax
		cmp x1, ax
		jl skipmachine1
		or byte ptr cs:[piclab2a+1], 0x08
		or byte ptr cs:[piclab2b+1], 0x08
		jmp skipmachine2
skipmachine1:
		and byte ptr cs:[piclab2a+1], 0xf7
		and byte ptr cs:[piclab2b+1], 0xf7
skipmachine2:
		mov ax, y0
		neg ax
		cmp y1, ax
		jl skipmachine3
		or byte ptr cs:[piclab4a], 0x08
		or byte ptr cs:[piclab4b], 0x08
		jmp skipmachine4
skipmachine3:
		and byte ptr cs:[piclab4a], 0xf7
		and byte ptr cs:[piclab4b], 0xf7
skipmachine4:
		mov ax, x0
		cmp x1, ax
		jg skipmachine5
		or byte ptr cs:[piclab5], 0x08
		jmp skipmachine6
skipmachine5:
		and byte ptr cs:[piclab5], 0xf7
skipmachine6:
		mov ax, y0
		cmp y1, ax
		jg skipmachine7
		or byte ptr cs:[piclab6], 0x08
		jmp skipmachine8
skipmachine7:
		and byte ptr cs:[piclab6], 0xf7
skipmachine8:
		mov ax, 0xa000
		mov es, ax
	}
	for(i=0;i<realsiz;i+=linesatonce)
	{
		_asm \
		{
			mov ax, xi0
			mov dx, xa0
			add xc0, ax
piclab5: adc x2, dx
			mov ax, yi0
			mov dx, ya0
			add yc0, ax
piclab6: adc y2, dx
		}
		if ((xplc >= lside) && (xplc < rside) && (siz >= height[xplc]))
			_asm \
			{
				mov dx, 0x3c4
				mov al, 2
				out dx, al
				inc dx
				mov cx, mapmaskval
				mov al, 1
				cmp linesatonce, 2
				jb skipsetwidthc
				je skipsetwidtha
				mov al, 15
				jmp skipsetwidthc
skipsetwidtha:
				mov al, 3
skipsetwidthc:
				shl al, cl
				out dx, al
				mov di, p
				mov ah, byte ptr x2[0]
				mov al, byte ptr y2[0]
				mov cx, numvpixels
				mov dx, xc0
				mov si, yc0
				test ang, 512
				jnz skipnottera
				neg dx
				jmp skipnotterb
skipnottera:
				neg si
skipnotterb:
				mov bx, waitodraw
				push ds
				mov ds, picseg
begpictur1:
piclab1a:   add dx, 0x8888
piclab2a:   adc ah, 0x88
piclab3a:   add si, 0x8888
piclab4a:   adc al, 0x88
				inc bx
				add di, 90
				test ax, 0xc0c0
				loopnz begpictur1
				jcxz endpictur
				cmp bx, 0
				jle begpictur1
begpictur2:
piclab1b:   add dx, 0x8888
piclab2b:   adc ah, 0x88
piclab3b:   add si, 0x8888
piclab4b:   adc al, 0x88
				test ax, 0xc0c0
				jnz endpictur
				xor bl, bl
				mov bh, ah
				shr bx, 1
				shr bx, 1
				or bl, al
				mov bl, byte ptr ds:[bx]
				cmp bl, 255
				je skipictur
				mov es:[di], bl
skipictur:
				add di, 90
				loop begpictur2
endpictur:
				pop ds
			}
		mapmaskval += linesatonce;
		if (mapmaskval >= 4)
		{
			mapmaskval -= 4;
			p++;
		}
		xplc += linesatonce;
	}
}

doordraw(x,y,posxs,posys,poszs,angs,walnume)
unsigned int x, y, posxs, posys;
int poszs, walnume, angs;
{
	unsigned char lstat[2], rstat[2];
	int i, way, lsid, rsid, x1, x2, y1, y2, scale, oldsortcnt, siz;
	int xc, yc, xf, yf, yd, xpos, ypos, linesatonce, ysizincauto, passcount;
	unsigned int doorxsiz, picplc, picplcinc;
	unsigned int clipx, clipy, clipx1, clipy1, clipx2, clipy2;
	long tanz1, tanz2, delta, templong1, templong2, templong3;

	oldsortcnt = sortcnt;
	if (memtype[walnume-1] != 65535)
	{
		if (vms == 1)
			if (memtype[walnume-1] == 0)
				loadvms(walnume-1);
		if ((emswalls > 0) && ((memtype[walnume-1]&1024) > 0))
			emsetpages(memtype[walnume-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[walnume-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[walnume-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	lsid = lborder[walnume];
	rsid = rborder[walnume];
	picplc = 0;
	xc = 0;
	yc = 0;
	way = 0;
	if ((board[x][y]&8192) > 0)
		way = 1;
	clipx = (x<<10);
	clipy = (y<<10);
	doorxsiz = 1024;
	picplcinc = 64;
	if (way == 0)
	{
		clipy1 = clipy+512;
		tanz1 = tantable[(2816-angs)&1023];
		tanz2 = tantable[(2304-angs)&1023];
		delta = ((long)clipy1)-((long)posys);
		templong1 = posxs;
		templong2 = posxs;
		if (labs(tanz1) < 32768L)
			templong1 += ((delta*tanz1)>>16);
		else if (labs(tanz1) < 8388608L)
			templong1 += ((delta*(tanz1>>8))>>8);
		else
			templong1 += (delta*(tanz1>>16));
		if (labs(tanz2) < 32768L)
			templong2 += ((delta*tanz2)>>16);
		else if (labs(tanz2) < 8388608L)
			templong2 += ((delta*(tanz2>>8))>>8);
		else
			templong2 += (delta*(tanz2>>16));
		if (posys >= clipy1)
		{
			if ((templong1&0xfffffc00) != (long)clipx)
				clipx1 = clipx;
			else
				clipx1 = (unsigned)templong1;
			if ((templong2&0xfffffc00) != (long)clipx)
				clipx2 = clipx+1023;
			else
				clipx2 = (unsigned)templong2;
			sortcnt = 510, checkobj(clipx1,clipy1,posxs,posys,angs,1023);
			sortcnt = 511, checkobj(clipx2,clipy1,posxs,posys,angs,1023);
			if (clipx1 > clipx)
			{
				sortang[510] = 0;
				picplc = (clipx1&1023);
				doorxsiz = 1023-picplc;
				xc = (picplc&15);
				picplc = ((picplc>>4)<<6);
			}
			if (clipx2 < clipx+1023)
			{
				sortang[511] = 359;
				doorxsiz = (clipx2&1023);
			}
		}
		else
		{
			if (((walnume >= door1) && (walnume <= door1+5)) || ((walnume >= door2) && (walnume <= door2+5)) || ((walnume >= door5) && (walnume <= door5+7)) || (walnume == 180))
				picplcinc = 4032;
			if ((templong2&0xfffffc00) != (long)clipx)
				clipx2 = clipx;
			else
				clipx2 = (unsigned)templong2;
			if ((templong1&0xfffffc00) != (long)clipx)
				clipx1 = clipx+1023;
			else
				clipx1 = (unsigned)templong1;
			sortcnt = 510, checkobj(clipx2,clipy1,posxs,posys,angs,1023);
			sortcnt = 511, checkobj(clipx1,clipy1,posxs,posys,angs,1023);
			if (clipx2 > clipx)
			{
				sortang[510] = 359;
				doorxsiz = 1023-(clipx2&1023);
			}
			if (clipx1 < clipx+1023)
			{
				sortang[511] = 0;
				picplc = (clipx1&1023);
				doorxsiz = picplc;
				xc = ((picplc&15)^15);
				picplc = ((picplc>>4)<<6);
			}
			if (((walnume < door1) || (walnume > door1+5)) && ((walnume < door2) || (walnume > door2+5)) && ((walnume < door5) || (walnume > door5+7)) && (walnume != 180))
				picplc = 4032-picplc;
		}
	}
	else
	{
		clipx1 = clipx+512;
		tanz1 = tantable[(1792+angs)&1023];
		tanz2 = tantable[(2304+angs)&1023];
		delta = ((long)clipx1)-((long)posxs);
		templong1 = posys;
		templong2 = posys;
		if (labs(tanz1) < 32768L)
			templong1 += ((delta*tanz1)>>16);
		else if (labs(tanz1) < 8388608L)
			templong1 += ((delta*(tanz1>>8))>>8);
		else
			templong1 += (delta*(tanz1>>16));
		if (labs(tanz2) < 32768L)
			templong2 += ((delta*tanz2)>>16);
		else if (labs(tanz2) < 8388608L)
			templong2 += ((delta*(tanz2>>8))>>8);
		else
			templong2 += (delta*(tanz2>>16));
		if (posxs <= clipx1)
		{
			if ((templong1&0xfffffc00) != (long)clipy)
				clipy1 = clipy;
			else
				clipy1 = (unsigned)templong1;
			if ((templong2&0xfffffc00) != (long)clipy)
				clipy2 = clipy+1023;
			else
				clipy2 = (unsigned)templong2;
			sortcnt = 510, checkobj(clipx1,clipy1,posxs,posys,angs,1023);
			sortcnt = 511, checkobj(clipx1,clipy2,posxs,posys,angs,1023);
			if (clipy1 > clipy)
			{
				sortang[510] = 0;
				picplc = (clipy1&1023);
				doorxsiz = 1023-picplc;
				xc = (picplc&15);
				picplc = ((picplc>>4)<<6);
			}
			if (clipy2 < clipy+1023)
			{
				sortang[511] = 359;
				doorxsiz = (clipy2&1023);
			}
		}
		else
		{
			if (((walnume >= door1) && (walnume <= door1+5)) || ((walnume >= door2) && (walnume <= door2+5)) || ((walnume >= door5) && (walnume <= door5+7)) || (walnume == 180))
				picplcinc = 4032;
			if ((templong2&0xfffffc00) != (long)clipy)
				clipy2 = clipy;
			else
				clipy2 = (unsigned)templong2;
			if ((templong1&0xfffffc00) != (long)clipy)
				clipy1 = clipy+1023;
			else
				clipy1 = (unsigned)templong1;
			sortcnt = 510, checkobj(clipx1,clipy2,posxs,posys,angs,1023);
			sortcnt = 511, checkobj(clipx1,clipy1,posxs,posys,angs,1023);
			if (clipy2 > clipy)
			{
				sortang[510] = 359;
				doorxsiz = 1023-(clipy2&1023);
			}
			if (clipy1 < clipy+1023)
			{
				sortang[511] = 0;
				picplc = (clipy1&1023);
				doorxsiz = picplc;
				xc = ((picplc&15)^15);;
				picplc = ((picplc>>4)<<6);
			}
			if (((walnume < door1) || (walnume > door1+5)) && ((walnume < door2) || (walnume > door2+5)) && ((walnume < door5) || (walnume > door5+7)) && (walnume != 180))
				picplc = 4032-picplc;
		}
	}
	x1 = sortang[510];
	x2 = sortang[511];
	y1 = sorti[510];
	y2 = sorti[511];
	if (x1 > x2)
	{
		i = x1, x1 = x2, x2 = i;
		i = y1, y1 = y2, y2 = i;
	}
	_asm \
	{
		mov dx, 0x3c4
		mov al, 0x2
		out dx, al
	}
	xf = x2-x1;
	yf = abs(y2-y1);
	if ((xf == 0) || (xf >= 512))
	{
		sortcnt = oldsortcnt;
		return(0);
	}
	templong1 = (long)y1*(long)y2;
	if (templong1 > 1)
	{
		templong2 = (long)((y1+y2)>>1);
		do
		{
			templong3 = templong2;
			templong2 = (templong2>>1)+(templong1/((templong2)<<1));
		}
		while (labs(templong3-templong2)>1);
		if (doorxsiz == 1024)
			scale = (templong2<<6)/((long)xf);
		else if (doorxsiz > 0)
			scale = (templong2*((long)doorxsiz))/(((long)xf)<<4);
	}
	else
		scale = 1024;
	ysizincauto = 0;
	if (yf > xf)
	{
		ysizincauto = yf / xf;
		yf = yf % xf;
	}
	yd = 1;
	if (y2 < y1)
	{
		yd = -1;
		ysizincauto = -ysizincauto;
	}
	xpos = x1;
	ypos = y1;
	if (xc > 0)
		xc = (int)(((long)xc*(long)ypos)>>4);
	passcount = 0;
	for(i=x1;i<=x2;i++)
	{
		if ((i >= lside) && (i < rside))
		{
			if ((ypos >= height[i]) && (ypos < 4096) && (picplc >= lsid) && (picplc <= rsid))
			{
				sprinum[i] = picplc;
				lincalc[i] = ypos;
			}
			else
				sprinum[i] = 65535;
		}
		yc += yf;
		if (yc >= xf)
		{
			yc -= xf;
			ypos += yd;
		}
		ypos += ysizincauto;
		xc += scale;
		while ((xc > ypos) && (ypos > 0))
		{
			xc -= ypos;
			picplc = (picplc+picplcinc)&4095;
			if ((picplcinc == 64) && (picplc < 64))
				passcount++;
			if ((picplcinc == 4032) && (picplc > 4032))
				passcount++;
		}
	}
	if (passcount > 4)
	{
		sortcnt = oldsortcnt;
		return(0);
	}
	for(i=x1;i<=x2;i++)
		if ((i >= lside) && (i < rside) && (sprinum[i] != 65535))
		{
			_asm \
			{
				mov dx, 0x3c5
				mov cl, byte ptr i
				and cl, 3
				mov al, 1
				shl al, cl
				out dx, al
			}
			linesatonce = 0;
			if (((i&3) < 3) && (sprinum[i] == sprinum[i+1]))
			{
				_asm \
				{
					mov dx, 0x3c5
					mov cl, byte ptr i
					and cl, 3
					mov al, 3
					shl al, cl
					out dx, al
				}
				linesatonce = 1;
				if (((i&3) < 2) && (sprinum[i] == sprinum[i+2]))
				{
					_asm \
					{
						mov dx, 0x3c5
						mov cl, byte ptr i
						and cl, 3
						mov al, 7
						shl al, cl
						out dx, al
					}
					linesatonce = 2;
					if (((i&3) == 0) && (sprinum[i] == sprinum[i+3]))
					{
						_asm \
						{
							mov dx, 0x3c5
							mov al, 15
							out dx, al
						}
						linesatonce = 3;
					}
				}
			}
			vline(i,halfheight-(((lincalc[i]>>2)*poszs)>>6),lincalc[i]>>2,walnume,sprinum[i]);
			i += linesatonce;
		}
	sortcnt = oldsortcnt;
}

statusbardraw(picx, picy, xsiz, ysiz, x, y, walnume, pageoffs)
unsigned picx, picy, x, y, xsiz, ysiz, pageoffs;
int walnume;
{
	int i, xc, picplc;
	unsigned int picseg, plc;

	if ((statusbar == 479) && (pageoffs == 0))
		return(0);
	if (memtype[walnume-1] != 65535)
	{
		if (vms == 1)
			if (memtype[walnume-1] == 0)
				loadvms(walnume-1);
		if ((emswalls > 0) && ((memtype[walnume-1]&1024) > 0))
			emsetpages(memtype[walnume-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[walnume-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[walnume-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	_asm \
	{
		mov ax, 0xa000
		mov es, ax
	}
	picseg = walseg[walnume-1];
	picplc = picy+(picx<<6);
	_asm \
	{
		mov dx, 0x3c4
		mov al, 0x2
		out dx, al
	}
	xc = 0;
	if (vidmode == 1)
		x += 20;
	plc = pageoffs+times90[y]+(x>>2);
	for(i=0;i<xsiz;i++)
	{
		_asm \
		{
			mov dx, 0x3c5
			mov cl, byte ptr x
			and cl, 3
			mov al, 1
			shl al, cl
			out dx, al
			mov di, plc
			mov si, picplc
			mov cx, si
			add cx, ysiz
			push ds
			mov ds, picseg
startbar1:
			lodsb
			cmp al, 255
			je skipbar1
			mov es:[di], al
skipbar1:
			add di, 90
			cmp si, cx
			jb startbar1
			pop ds
		}
		picplc += 64;
		x++;
		plc += ((x&3) == 0);
	}
}

vline(x, y, siz, walnume, picplc)
int x, y, siz, walnume, picplc;
{
	unsigned int picseg, plc;

	if ((siz < 1) || (siz > 4096) || (y > dside) || (y+siz < 0))
		return(0);
	picseg = walseg[walnume-1];
	_asm \
	{
		mov ax, 0xa000
		mov es, ax
		mov bx, y
		or bx, bx
		jns notovertop
		xor bx, bx
notovertop:
		shl bx, 1
		mov ax, times90[bx]
		mov bx, x
		shr bx, 1
		shr bx, 1
		add ax, bx
		add ax, pageoffset
		mov plc, ax
	}
	if (siz < 64)
	{
		_asm \
		{
			mov bx, siz
			shl bx, 1
			mov cx, less64inc[bx]
			mov si, picplc
			mov dx, cx
			shr dx, 1
			xor bh, bh
			mov di, plc
			push ds
			mov ds, picseg
startspri1:
			mov bl, dh
			mov al, ds:[si+bx]
			cmp al, 255
			jb skipitspri1
			add di, 90
			add dx, cx
			cmp dh, 63
			jbe startspri1
			jmp enditspri1
skipitspri1:
			mov es:[di], al
			add di, 90
			add dx, cx
			cmp dh, 63
			jbe startspri1
enditspri1:
			pop ds
		}
		return(0);
	}
	if (y < 0)
	{
		_asm \
		{
			mov ax, y
			neg ax
			mov cl, 6
			rol ax, cl
			mov dx, ax
			and al, 0xc0
			and dx, 0x0f
			mov cx, siz
			div cx
			add picplc, ax           ;ax is dividend
			sub dx, siz              ;dx is remainder
			neg dx

			mov di, x
			shr di, 1
			shr di, 1
			add di, pageoffset
			push ds
			mov ds, picseg
			mov si, picplc           ;remember dx is (siz - remainder)
			lodsb
prespridraw:
			cmp al, 255
			je prespriskip
			mov es:[di], al
prespriskip:
			add di, 90
			sub dx, 64
			jns prespridraw
			pop ds
			mov plc, di
			mov picplc, si
		}
	}
	if (y+siz < dside)
	{
		_asm \
		{
			mov word ptr jmptable[2], offset begless128a
			mov word ptr jmptable[4], offset begless192a
			mov word ptr jmptable[6], offset begless256a
			mov word ptr jmptable[8], offset begmore256a
			mov bx, siz
			mov cl, 6
			shr bx, cl
			cmp bx, 4
			jge dontjumpmorethan256
			shl bx, 1
			cmp bx, 8
			jmp jmptable[bx]
dontjumpmorethan256:
			jmp begmore256a
begless128a:
			mov si, picplc
			mov cx, si
			and cx, 0x0fc0
			add cx, 65
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri2:
			lodsw
			cmp si, cx
			jae skipspri2d
startspri2a:
			cmp ax, 0xffff
			je skipspri2e
			xor dh, dh
			add dx, bx
			cmp dh, 3
			je startspri2b
			ja startspri2c
			add di, 180
			cmp al, 255
			je skipspri2a
			mov es:[di-180], al
			cmp ah, 255
			je startspri2
skipspri2a:
			mov es:[di-90], ah
			jmp startspri2
skipspri2d:
			mov ah, 255
			je startspri2a
			jmp enditspri2
startspri2b:
			add di, 270
			cmp al, 255
			je skipspri2b
			mov es:[di-270], al
			mov es:[di-180], al
			cmp ah, 255
			je startspri2
skipspri2b:
			mov es:[di-90], ah
			jmp startspri2
startspri2c:
			add di, 360
			cmp al, 255
			je skipspri2c
			mov es:[di-360], al
			mov es:[di-270], al
			cmp ah, 255
			je startspri2
skipspri2c:
			mov es:[di-180], ah
			mov es:[di-90], ah
			jmp startspri2
skipspri2e:
			xor dh, dh
			add dx, bx
			cmp dh, 3
			je skipspri2f
			ja skipspri2g
			add di, 180
			jmp startspri2
skipspri2f:
			add di, 270
			jmp startspri2
skipspri2g:
			add di, 360
			jmp startspri2
enditspri2:
			pop ds
			jmp returnit
begless192a:
			mov si, picplc
			mov cx, si
			and cx, 0x0fc0
			add cx, 65
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri3:
			lodsw
			cmp si, cx
			jae skipspri3g
startspri3a:
			xor dh, dh
			add dx, bx
			cmp dh, 5
			je startspri3b
			ja startspri3c
			cmp al, 255
			je skipspri3a
			mov es:[di], al
			mov es:[di+90], al
skipspri3a:
			cmp ah, 255
			je skipspri3b
			mov es:[di+180], ah
			mov es:[di+270], ah
skipspri3b:
			add di, 360
			jmp startspri3
startspri3b:
			cmp al, 255
			je skipspri3c
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri3c:
			cmp ah, 255
			je skipspri3d
			mov es:[di+270], ah
			mov es:[di+360], ah
skipspri3d:
			add di, 450
			jmp startspri3
startspri3c:
			cmp al, 255
			je skipspri3e
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri3e:
			cmp ah, 255
			je skipspri3f
			mov es:[di+270], ah
			mov es:[di+360], ah
			mov es:[di+450], ah
skipspri3f:
			add di, 540
			jmp startspri3
skipspri3g:
			mov ah, 255
			jne enditspri3
			jmp startspri3a
enditspri3:
			pop ds
			jmp returnit
begless256a:
			mov si, picplc
			mov cx, si
			and cx, 0x0fc0
			add cx, 65
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri4:
			lodsw
			cmp si, cx
			jae skipspri4g
startspri4a:
			xor dh, dh
			add dx, bx
			cmp dh, 7
			je startspri4b
			ja startspri4c
			cmp al, 255
			je skipspri4a
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri4a:
			cmp ah, 255
			je skipspri4b
			mov es:[di+270], ah
			mov es:[di+360], ah
			mov es:[di+450], ah
skipspri4b:
			add di, 540
			jmp startspri4
skipspri4g:
			mov ah, 255
			jne enditspri4
			jmp startspri4a
startspri4b:
			cmp al, 255
			je skipspri4c
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
			mov es:[di+270], al
skipspri4c:
			cmp ah, 255
			je skipspri4d
			mov es:[di+360], ah
			mov es:[di+450], ah
			mov es:[di+540], ah
skipspri4d:
			add di, 630
			jmp startspri4
startspri4c:
			cmp al, 255
			je skipspri4e
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
			mov es:[di+270], al
skipspri4e:
			cmp ah, 255
			je skipspri4f
			mov es:[di+360], ah
			mov es:[di+450], ah
			mov es:[di+540], ah
			mov es:[di+630], ah
skipspri4f:
			add di, 720
			jmp startspri4
enditspri4:
			pop ds
			jmp returnit
begmore256a:
			mov di, plc
			mov si, picplc
			mov cx, si
			and cx, 0x0fc0
			add cx, 64
			mov bx, siz
			mov dx, bx
			dec dx
			push ds
			mov ds, picseg
			lodsb
startspri5:
			cmp al, 255
			je skipspri5a
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
			mov es:[di+270], al
skipspri5a:
			add di, 360
			sub dx, 256
			js startspri5c
			or dh, dh
			jnz startspri5
startspri5b:
			cmp al, 255
			je skipspri5b
			mov es:[di], al
skipspri5b:
			add di, 90
			sub dx, 64
			jns startspri5b
startspri5c:
			lodsb
			add dx, bx
			cmp si, cx
			jbe startspri5
			pop ds
returnit:
		}
		return(0);
	}
	if (siz < 128)
	{
		_asm \
		{
			mov si, picplc
			mov cx, pageoffset
			add cx, scrsize
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri6:
			lodsw
			cmp di, cx
			jae skipspri6d
startspri6a:
			cmp ax, 0xffff
			je skipspri6e
			xor dh, dh
			add dx, bx
			cmp dh, 3
			je startspri6b
			ja startspri6c
			add di, 180
			cmp al, 255
			je skipspri6a
			mov es:[di-180], al
			cmp ah, 255
			je startspri6
skipspri6a:
			mov es:[di-90], ah
			jmp startspri6
skipspri6d:
			mov ah, 255
			je startspri6a
			jmp enditspri6
startspri6b:
			add di, 270
			cmp al, 255
			je skipspri6b
			mov es:[di-270], al
			mov es:[di-180], al
			cmp ah, 255
			je startspri6
skipspri6b:
			mov es:[di-90], ah
			jmp startspri6
startspri6c:
			add di, 360
			cmp al, 255
			je skipspri6c
			mov es:[di-360], al
			mov es:[di-270], al
			cmp ah, 255
			je startspri6
skipspri6c:
			mov es:[di-180], ah
			mov es:[di-90], ah
			jmp startspri6
skipspri6e:
			xor dh, dh
			add dx, bx
			cmp dh, 3
			je skipspri6f
			ja skipspri6g
			add di, 180
			jmp startspri6
skipspri6f:
			add di, 270
			jmp startspri6
skipspri6g:
			add di, 360
			jmp startspri6
enditspri6:
			pop ds
		}
		return(0);
	}
	if (siz < 192)
	{
		_asm \
		{
			mov si, picplc
			mov cx, pageoffset
			add cx, scrsize
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri7:
			lodsw
			cmp di, cx
			jae skipspri7g
startspri7a:
			xor dh, dh
			add dx, bx
			cmp dh, 5
			je startspri7b
			ja startspri7c
			cmp al, 255
			je skipspri7a
			mov es:[di], al
			mov es:[di+90], al
skipspri7a:
			cmp ah, 255
			je skipspri7b
			mov es:[di+180], ah
			mov es:[di+270], ah
skipspri7b:
			add di, 360
			jmp startspri7
startspri7b:
			cmp al, 255
			je skipspri7c
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri7c:
			cmp ah, 255
			je skipspri7d
			mov es:[di+270], ah
			mov es:[di+360], ah
skipspri7d:
			add di, 450
			jmp startspri7
startspri7c:
			cmp al, 255
			je skipspri7e
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri7e:
			cmp ah, 255
			je skipspri7f
			mov es:[di+270], ah
			mov es:[di+360], ah
			mov es:[di+450], ah
skipspri7f:
			add di, 540
			jmp startspri7
skipspri7g:
			mov ah, 255
			jae enditspri7
			jmp startspri7a
enditspri7:
			pop ds
		}
		return(0);
	}
	if (siz < 256)
	{
		_asm \
		{
			mov si, picplc
			mov cx, pageoffset
			add cx, scrsize
			mov bx, siz
			mov dx, bx
			dec dx
			shl bx, 1
			shl bx, 1
			shl bx, 1
			mov di, plc
			push ds
			mov ds, picseg
startspri8:
			lodsw
			cmp di, cx
			jae skipspri8g
startspri8a:
			xor dh, dh
			add dx, bx
			cmp dh, 7
			je startspri8b
			ja startspri8c
			cmp al, 255
			je skipspri8a
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
skipspri8a:
			cmp ah, 255
			je skipspri8b
			mov es:[di+270], ah
			mov es:[di+360], ah
			mov es:[di+450], ah
skipspri8b:
			add di, 540
			jmp startspri8
skipspri8g:
			mov ah, 255
			jae enditspri8
			jmp startspri8a
startspri8b:
			cmp al, 255
			je skipspri8c
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
			mov es:[di+270], al
skipspri8c:
			cmp ah, 255
			je skipspri8d
			mov es:[di+360], ah
			mov es:[di+450], ah
			mov es:[di+540], ah
skipspri8d:
			add di, 630
			jmp startspri8
startspri8c:
			cmp al, 255
			je skipspri8e
			mov es:[di], al
			mov es:[di+90], al
			mov es:[di+180], al
			mov es:[di+270], al
skipspri8e:
			cmp ah, 255
			je skipspri8f
			mov es:[di+360], ah
			mov es:[di+450], ah
			mov es:[di+540], ah
			mov es:[di+630], ah
skipspri8f:
			add di, 720
			jmp startspri8
enditspri8:
			pop ds
		}
		return(0);
	}
	_asm \
	{
		mov di, plc
		mov si, picplc
		mov cx, pageoffset
		add cx, scrsize
		mov bx, siz
		mov dx, bx
		dec dx
		push ds
		mov ds, picseg
		lodsb
		cmp al, 255
		je startspri9d
startspri9:
		mov es:[di], al
		mov es:[di+90], al
		mov es:[di+180], al
		mov es:[di+270], al
		add di, 360
		cmp di, cx
		jae enditspri9
		sub dx, 256
		js startspri9c
		cmp dh, 0
		jg startspri9
startspri9b:
		mov es:[di], al
		add di, 90
		sub dx, 64
		jns startspri9b
startspri9c:
		lodsb
		add dx, bx
		cmp al, 255
		jne startspri9
startspri9d:
		add di, 360
		cmp di, cx
		jae enditspri9
		sub dx, 256
		js startspri9c
		cmp dh, 0
		jg startspri9d
startspri9e:
		add di, 90
		sub dx, 64
		jns startspri9e
		jmp startspri9c
enditspri9:
		pop ds
	}
}

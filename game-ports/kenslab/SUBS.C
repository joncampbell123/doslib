#include "lab3d.h"
#define giflen1 7649
#define giflen2 8258
#define ksaylength 133
#define ksaycodestart 0xc
#define ksaybufptr 0xf
#define ksayminicntinc 0x18
unsigned char ksaycode[ksaylength] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x50,0x2E,0xA0,0xFF,0xFF,0xE6,0x42,0x2E,
	0x81,0x06,0x08,0x00,0x00,0x80,0x2E,0x80,0x16,0x0F,
	0x00,0x00,0x78,0x06,0xB0,0x20,0xE6,0x20,0x58,0xCF,
	0x2E,0x80,0x16,0x10,0x00,0x00,0x70,0x25,0x2E,0x80,
	0x36,0x20,0x00,0x01,0x06,0x53,0x2E,0x8B,0x1E,0x04,
	0x00,0x2E,0x8E,0x06,0x06,0x00,0x2E,0x81,0x06,0x0A,
	0x00,0x51,0xC9,0x26,0x83,0x17,0x02,0x5B,0x07,0xB0,
	0x20,0xE6,0x20,0x58,0xCF,0xE4,0x61,0x24,0xFC,0xE6,
	0x61,0xB0,0x36,0xE6,0x43,0xB0,0x6C,0xE6,0x40,0xB0,
	0x13,0xE6,0x40,0x06,0x33,0xC0,0x8E,0xC0,0xFA,0x2E,
	0xA1,0x00,0x00,0x26,0xA3,0x20,0x00,0x2E,0xA1,0x02,
	0x00,0x26,0xA3,0x22,0x00,0xFB,0x07,0xB0,0x20,0xE6,
	0x20,0x58,0xCF,
};

loadboard()
{
	unsigned char bitcnt, numbits;
	int i, j, k, fil, bytecnt1, bytecnt2;
	int currstr, strtot, compleng, dat, goalstr;
	unsigned int walsegg, walofss;
	long templong;
	int prepdie;

	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
	if ((fil = open("boards.kzp",O_BINARY|O_RDWR,S_IREAD)) != 1)
	{
		prepdie = 0;
		numwarps = 0;
		justwarped = 0;
		read(fil,&boleng[0],30*4);
		templong = (long)(30*4);
		for(i=0;i<(boardnum<<1);i++)
			templong += ((long)(boleng[i]+2));
		lseek(fil,templong,SEEK_SET);
		_asm \
		{
			mov es, lzwbuf
			mov cx, 256
			mov ax, 0
boardsetuplzwbuf:
			inc ax
			mov di, ax
			mov byte ptr es:[di+8200], al
			mov bx, ax
			mov word ptr es:[di+bx], ax
			loop boardsetuplzwbuf
			mov word ptr es:[0], 0
			mov byte ptr es:[8200], 0
		}
		for(i=0;i<2;i++)
		{
			compleng = boleng[(boardnum<<1)+i];
			read(fil,&strtot,2);
			read(fil,&tempbuf[0],compleng);
			_asm \
			{
				mov ax, seg board
				mov walsegg, ax
				mov ax, offset board
				mov walofss, ax
			}
			if (i == 1)
				walsegg += 256;
			if (strtot > 0)
			{
				tempbuf[compleng] = 0;
				tempbuf[compleng+1] = 0;
				tempbuf[compleng+2] = 0;
				bytecnt2 = 0;
				bytecnt1 = 0;
				bitcnt = 0;
				currstr = 256;
				goalstr = 512;
				numbits = 9;
				do
				{
					_asm \
					{
						mov es, lzwbuf
						mov dl, bitcnt
						mov dh, numbits
						mov si, bytecnt2
						mov di, word ptr tempbuf[si]
						mov cl, dl
						shr di, cl
						mov cl, dh
						mov bx, 1
						shl bx, cl
						dec bx
						and di, bx
						add cl, dl
						cmp cl, 16
						jbe boardskipextrabits2
						mov al, byte ptr tempbuf[si+2]
						mov bx, 1
						rol bx, cl
						dec bx
						and ax, bx
						mov cl, dl
						ror ax, cl
						add di, ax
boardskipextrabits2:
						mov dat, di
						add dl, dh
						mov dh, 0
						mov bx, dx
						mov cl, 3
						shr bx, cl
						add bytecnt2, bx
						and dl, 7
						mov bitcnt, dl

						mov cx, 1
						mov bx, dat
						mov di, currstr
						shl di, 1
						mov es:[di], bx
						cmp bx, 256
						jb boardendsearchtable2
boardbeginsearchtable2:
						inc cx
						mov al, es:[bx+8200]
						push ax
						shl bx, 1
						mov bx, es:[bx]
						cmp bx, 256
						jae boardbeginsearchtable2
boardendsearchtable2:
						mov ax, bx
						mov ah, al
						shr di, 1
						mov es:[di+8199], ax
						shl bx, 1
						mov bx, es:[bx]
						push bx
						mov es, walsegg
						mov di, bytecnt1
						mov bx, walofss
boardreversedata:
						pop ax
						cmp di, 4096
						jae boardskipstoredata
						mov es:[di+bx], al
						inc di
boardskipstoredata:
						loop boardreversedata
						mov bytecnt1, di
					}
					currstr++;
					if (currstr == goalstr)
					{
						numbits++;
						goalstr = (goalstr<<1);
					}
				}
				while (currstr <= strtot);
			}
			else
				_asm \
				{
					mov cx, 2048
					mov es, walsegg
					mov di, walofss
					mov si, offset tempbuf
					mov ax, seg tempbuf
					push ds
					mov ds, ax
					rep movsw
					pop ds
				}
		}
		close(fil);
		if (memtype[map-1] != 65535)
		{
			if ((vms == 1) && (memtype[map-1] == 0))
				loadvms(map-1);
			if ((emswalls > 0) && ((memtype[map-1]&1024) > 0))
				emsetpages(memtype[map-1]-1024,0);
		}
		_asm \
		{
			cld
			mov es, walseg[(map-1)+(map-1)]
			mov si, offset board
			mov ax, seg board
			mov di, 0
			mov cx, 4096
			push ds
			mov ds, ax
copyboard1:
			lodsw
			stosb
			loop copyboard1
			pop ds
		}
		if ((xmswalls > 0) && ((memtype[map-1]&2048) >= 0) && (memtype[map-1] != 65535))
			xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[map-1]-2048))<<12,4096L);
		mnum = 0;
		for(i=0;i<63;i++)
			for(j=0;j<63;j++)
			{
				board[i][j] &= 0xbfff;
				k = board[i][j]&1023;
				if ((k == warp) && (numwarps < 16))
				{
					xwarp[numwarps] = (char)i;
					ywarp[numwarps] = (char)j;
					numwarps++;
				}
				if ((k == monken) || (k == mongre) || (k == monand) ||
					 (k == monbal) || (k == monali) || (k == monhol) ||
					 (k == monzor) || (k == monbat) || (k == mongho) ||
					 (k == monske) || (k == monke2) || (k == monan2) ||
					 (k == monan3) || (k == monwit) || (k == monbee) ||
					 (k == monspi) || (k == mongr2) || (k == monear) ||
					 (k == monrob) || (k == monro2) || (k == mondog) ||
					 (k == monmum) || (k == hive))
				{
					mposx[mnum] = (i<<10)+512;
					mposy[mnum] = (j<<10)+512;
					mgolx[mnum] = mposx[mnum];
					mgoly[mnum] = mposy[mnum];
					moldx[mnum] = mposx[mnum];
					moldy[mnum] = mposy[mnum];
					mstat[mnum] = k;
					mshock[mnum] = 0;
					if (k == monken) mshot[mnum] = 1;
					if (k == monbal) mshot[mnum] = 255;
					if (k == mongre) mshot[mnum] = 2;
					if (k == mongr2) mshot[mnum] = 3;
					if (k == monrob) mshot[mnum] = 5;
					if (k == monwit) mshot[mnum] = 1;
					if (k == monand) mshot[mnum] = 5;
					if (k == monali) mshot[mnum] = 10, prepdie |= 2;
					if (k == monhol) mshot[mnum] = 255;
					if (k == monzor) mshot[mnum] = 47, prepdie |= 1;
					if (k == monbat) mshot[mnum] = 1;
					if (k == monear) mshot[mnum] = 1;
					if (k == monbee) mshot[mnum] = 1;
					if (k == monspi) mshot[mnum] = 1;
					if (k == mongho) mshot[mnum] = 1;
					if (k == monske) mshot[mnum] = 1;
					if (k == monmum) mshot[mnum] = 2;
					if (k == monke2) mshot[mnum] = 47, prepdie |= 1;
					if (k == monan2) mshot[mnum] = 47, prepdie |= 1;
					if (k == monan3) mshot[mnum] = 1;
					if (k == hive) mshot[mnum] = 15;
					if (k == monro2) mshot[mnum] = 2;
					if (k == mondog) mshot[mnum] = 1;
					mnum++;
					board[i][j] = 16384+1024;
				}
				if ((board[i][j]&4096) > 0)
				{
					posx = (i<<10)+512;
					posy = (j<<10)+512;
					yourhereoldpos = ((posx>>10)<<6)+(posy>>10);
					youarehere();
					ang = ((board[i][j]&3)<<9);
					startx = posx;
					starty = posy;
					startang = ang;
					board[i][j] = stairtop+1024;
				}
			}
		if ((prepdie&1) > 0)
			ksay(21);
		else if ((prepdie&2) > 0)
			ksay(20);
		posz = 32;
		angvel = 0;
		vel = 0;
		mxvel = 0;
		myvel = 0;
		svel = 0;
		hvel = 0;
		for(i=0;i<32;i++)
			bulstat[i] = 0;
		lastbulshoot = 0;
		bulnum = 0;
		keys[0] = 0;
		keys[1] = 0;
		death = 4095;
	}
}

void interrupt far keyhandler()
{
	oldreadch = readch;
	_asm \
	{
		in al, 0x60
		mov readch, al
		in al, 0x61
		or al, 0x80
		out 0x61, al
		and al, 0x7f
		out 0x61, al
	}
	if (readch == 0xe0)
		extended = 128;
	else
	{
		if (oldreadch != readch)
			keystatus[(readch&127)+extended] = ((readch>>7)^1);
		extended = 0;
	}
	_asm \
	{
		mov al, 0x20
		out 0x20, al
	}
}

loadtables()
{
	int fil, i;

	if ((fil = open("tables.dat",O_BINARY|O_RDWR,S_IREAD)) != -1)
	{
		read(fil,&sintable[0],8192);
		read(fil,&tantable[0],4096);
		read(fil,&radarang[0],720);
		read(fil,&option[0],numoptions);
		read(fil,&keydefs[0],numkeys);
		read(fil,&joyx1,2);
		read(fil,&joyy1,2);
		read(fil,&joyx2,2);
		read(fil,&joyy2,2);
		read(fil,&joyx3,2);
		read(fil,&joyy3,2);
		read(fil,&ksayfreq,2);
		close(fil);
	}
}

int ksay(filenum)
unsigned int filenum;
{
	int infile, numfiles;
	unsigned int sndoffset, leng, i, minicntinc;
	long sndfiloffs;
	unsigned char timerval, ksaybuflookup[256];

	if ((speechstatus == 0) || (mute == 1))
		return(-1);
	if ((lastsoundnum != filenum) || (speechstatus == 1))
	{
		if ((infile = open("sounds.kzp", O_BINARY, S_IREAD)) == -1)
			return(-1);
		read(infile,&numfiles,2);
		if (filenum >= numfiles)
		{
			close(infile);
			return(-1);
		}
	}
	else
	{
		leng = oldsndleng;
		sndfiloffs = oldsndfiloffs;
	}
	sndoffset = (sndaddr&32768);
	if (speechstatus == 1)
	{
		if (ksayfreq < 11025)
			ksayfreq = 11025;
		if (ksayfreq > 22050)
			ksayfreq = 22050;
		minicntinc = (unsigned)(11289600L/((long)(ksayfreq>>6)));
		if (minicntinc < 32000)
			minicntinc = 65535;
		timerval = (char)(1193280L / ((long)ksayfreq));
		for(i=0;i<256;i++)
			ksaybuflookup[i] = (unsigned char)((i*((unsigned)timerval))>>8)+1;
		if (sndoffset == 32768)
			snd += 2048;
		_asm \
		{
			mov ax, offset clockspeed
			mov dx, ds
			mov ksaycode[4], ax
			mov ksaycode[6], dx
			xor ax, ax
			mov es, ax
			mov cx, ksaycodestart
			mov dx, snd
			cli
			cmp word ptr es:[32], cx
			jne skiprestoreintvecta
			cmp word ptr es:[34], dx
			jne skiprestoreintvecta
			sti
			in al, 97
			and al, 252
			out 97, al
			mov al, 0x36
			out 0x43, al
			mov al, 255
			out 40h, al
			out 40h, al
			mov es, snd
			mov cx, word ptr es:[0]
			mov dx, word ptr es:[2]
			xor ax, ax
			mov es, ax
			cli
			mov word ptr es:[32], cx
			mov word ptr es:[34], dx
skiprestoreintvecta:
			sti
		}
		if (sndoffset == 32768)
			snd -= 2048;
	}
	if ((lastsoundnum != filenum) || (speechstatus == 1))
	{
		_asm \
		{
			mov ah, 0x3f
			mov bx, infile
			mov cx, numfiles
			shl cx, 1
			mov dx, cx
			shl cx, 1
			add cx, dx
			mov dx, sndoffset
			push ds
			mov ds, snd
			int 0x21
			pop ds
			push es
			mov es, snd
			mov di, filenum
			shl di, 1
			mov ax, di
			shl di, 1
			add di, ax
			add di, sndoffset
			mov ax, word ptr es:[di]
			mov dx, word ptr es:[di+2]
			mov word ptr sndfiloffs[0], ax
			mov word ptr sndfiloffs[2], dx
			mov ax, word ptr es:[di+4]
			mov leng, ax
			pop es
		}
		lseek(infile,sndfiloffs,SEEK_SET);
		_asm \
		{
			mov ah, 0x3f
			mov bx, infile
			mov cx, leng
			mov dx, sndoffset
			push ds
			mov ds, snd
			int 0x21
			pop ds
			mov leng, ax
		}
		close(infile);
		lastsoundnum = filenum;
		oldsndleng = leng;
		oldsndfiloffs = sndfiloffs;
	}
	if (speechstatus == 1)
	{
		if (sndoffset == 32768)
			snd += 2048;
		_asm \
		{
			std                              ;shifting buffer to right place
			mov es, snd
			mov cx, leng                     ;ax = leng from int 0x21 call
			mov di, 32767
			mov bx, cx
fixsndbuffer:
			mov si, byte ptr es:[bx]         ;fix up snd buffer by shifting
			and si, 0x00ff
			dec bx
			mov al, byte ptr ksaybuflookup[si]
			stosb
			loop fixsndbuffer
			cld
			mov word ptr ds:ksaycode[ksaybufptr], di
			mov word ptr ds:ksaycode[8], 0                ;minicnt
			mov ax, minicntinc
			mov word ptr ds:ksaycode[ksayminicntinc], ax
			mov ah, 0x3e                     ;close file
			mov bx, infile
			int 0x21
			mov cx, ksaylength               ;copy ksay language to snd buf
			xor di, di
			mov si, offset ksaycode
			rep movsb
			mov al, 0x36                     ;prepare timers
			out 67, al
			mov al, timerval                 ;54 = speed = 1193280 / (ksayfreq)
			out 64, al
			xor al, al
			out 64, al
			mov al, 144
			out 67, al
			xor al, al
			out 66, al
			xor ax, ax                       ;do vector stuff
			mov es, ax
			mov cx, ksaycodestart            ;IMPORTANT - vector offset
			mov dx, snd
			mov ax, dx
			cli
			xchg word ptr es:[32], cx
			xchg word ptr es:[34], dx
			mov es, ax
			mov word ptr es:[0], cx
			mov word ptr es:[2], dx
			sti                              ;start the sound!
			in al, 97                        ;speaker on
			or al, 3
			out 97, al
		}
		if (sndoffset == 32768)
			snd -= 2048;
		return(0);
	}
	_asm \
	{
		mov al, 0x5
		out 0xa, al
	}
	reset_dsp();
	_asm \
	{
		mov dx, 0x20c
		mov ax, speechstatus
		mov cl, 4
		shl ax, cl
		add dx, ax
ksbdowait1:
		in al, dx
		test al, 0x80
		jnz ksbdowait1
		mov al, 0xd1
		out dx, al
ksbdowait2:
		in al, dx
		test al, 0x80
		jnz ksbdowait2
		mov al, 0x40
		out dx, al
ksbdowait3:
		in al, dx
		test al, 0x80
		jnz ksbdowait3
		mov al, 165
		out dx, al

		mov cx, word ptr sndaddr[0]      ;dma stuff
		mov dx, word ptr sndaddr[2]
		add cx, word ptr sndoffset[0]
		adc dx, 0
		mov bx, word ptr leng[0]
		dec bx
		mov al, dl
		out 0x83, al
		xor al, al
		out 0xc, al
		mov al, cl
		out 0x2, al
		mov al, ch
		out 0x2, al
		mov al, bl
		out 0x3, al
		mov al, bh
		out 0x3, al
		mov al, 0x49
		out 0xb, al
		mov al, 0x1
		out 0xa, al

		mov dx, 0x20c
		mov ax, speechstatus
		mov cl, 4
		shl ax, cl
		add dx, ax
ksbdowait4:
		in al, dx
		test al, 0x80
		jnz ksbdowait4
		mov al, 0x14
		out dx, al
ksbdowait5:
		in al, dx
		test al, 0x80
		jnz ksbdowait5
		mov bx, leng
		dec bx
		mov al, bl
		out dx, al
ksbdowait6:
		in al, dx
		test al, 0x80
		jnz ksbdowait6
		mov al, bh
		out dx, al
	}
}

reset_dsp()
{
	_asm \
	{
		mov dx, 0x206
		mov ax, speechstatus
		mov cl, 4
		shl ax, cl
		add dx, ax
		mov al, 1
		out dx, al
		in al, dx
		in al, dx
		in al, dx
		in al, dx
		xor al, al
		out dx, al
		add dx, 4
		mov cx, 100
resetsb1:
		in al, dx
		cmp al, 0xaa
		je resetsb2
		loop resetsb1
		mov ax, 1
		ret
resetsb2:
		xor ax, ax
	}
}

void interrupt far divzerohandler()
{
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
		mov di, 0
		mov ax, 0
startdivzero:
		inc ax
		stosb
		jmp startdivzero
	}
}

checkobj(x,y,posxs,posys,angs,num)
unsigned x, y, posxs, posys;
int angs, num;
{
	int angle, siz, ysiz;

	if (((posxs|1023) != (x|1023)) || ((posys|1023) != (y|1023)))
	{
		siz = (int)(((((long)x-(long)posxs)>>2)*sintable[(angs+512)&2047]+(((long)y-(long)posys)>>2)*sintable[angs])>>16);
		if (siz != 0)
		{
			ysiz = (int)(((((long)x-(long)posxs)>>2)*sintable[angs]-(((long)y-(long)posys)>>2)*sintable[(angs+512)&2047])>>16);
			angle = 180-(int)((180*(long)ysiz)/(long)siz);
			siz = (int)(163840L/((long)siz));
			sortang[sortcnt] = angle;
			sorti[sortcnt] = siz;
			sortbnum[sortcnt] = (num&1023);
			if (((angle+(siz>>3)) >= lside) && ((angle-(siz>>3)) < rside) && ((siz&0xc000) == 0))
				sortcnt++;
		}
	}
}

linecompare(lin)
unsigned int lin;
{
	_asm \
	{
		mov bx, lin
		mov dx, 0x3d4
		mov al, 0x18
		mov ah, bl
		out dx, al
		inc dx
		mov al, ah
		out dx, al
	}
	outp(0x3d4,0x7); outp(0x3d5,(inp(0x3d5)&239)|((lin&256)>>4));
	outp(0x3d4,0x9); outp(0x3d5,(inp(0x3d5)&191)|((lin&512)>>3));
}

drawlife()
{
	int lifespot, olifespot;

	if (statusbar == 479)
		return(0);
	if (life < 0) life = 0;
	if (life > 4095) life = 4095;
	lifespot = (life>>6);
	olifespot = (oldlife>>6);
	if (lifespot < olifespot)
		statusbardraw(lifespot,57,olifespot-lifespot+1,7,lifespot+128,4,statusbarinfo,0);
	else if (lifespot > olifespot)
		statusbardraw(olifespot,50,lifespot-olifespot+1,7,olifespot+128,4,statusbarinfo,0);
	oldlife = life;
}

setmode360240()
{
	outp(0x3c4,0x4); outp(0x3c5,0x6);
	outp(0x3c4,0x0); outp(0x3c5,0x1);
	outp(0x3c2,0xe7);
	outp(0x3c4,0x0); outp(0x3c5,0x3);
	outp(0x3d4,0x11); outp(0x3d5,inp(0x3d5)&0x7f);      //Unlock indeces 0-7
	outp(0x3d4,0x0); outp(0x3d5,0x6b);
	outp(0x3d4,0x1); outp(0x3d5,0x59);
	outp(0x3d4,0x2); outp(0x3d5,0x5a);
	outp(0x3d4,0x3); outp(0x3d5,0x8e);
	outp(0x3d4,0x4); outp(0x3d5,0x5e);
	outp(0x3d4,0x5); outp(0x3d5,0x8a);
	outp(0x3d4,0x6); outp(0x3d5,0xd);
	outp(0x3d4,0x7); outp(0x3d5,0x3e);
	outp(0x3d4,0x9); outp(0x3d5,0x41);
	outp(0x3d4,0x10); outp(0x3d5,0xea);
	outp(0x3d4,0x11); outp(0x3d5,0xac);
	outp(0x3d4,0x12); outp(0x3d5,0xdf);
	outp(0x3d4,0x13); outp(0x3d5,45);
	outp(0x3d4,0x14); outp(0x3d5,0x0);
	outp(0x3d4,0x15); outp(0x3d5,0xe7);
	outp(0x3d4,0x16); outp(0x3d5,0x6);
	outp(0x3d4,0x17); outp(0x3d5,0xe3);
}

loadwalls()
{
	unsigned char bitcnt, numbits;
	int i, j, k, fil, bytecnt1, bytecnt2;
	int currstr, strtot, compleng, dat, goalstr;
	unsigned int walsegg;

	if ((fil = open("walls.kzp",O_BINARY|O_RDWR,S_IREAD)) != -1)
	{
		bmpkind[0] = 0;
		wallheader[0] = 8;
		read(fil,&wallheader[1],numwalls);
		read(fil,&tileng[0],numwalls*2);
		tioffs[0] = (long)(numwalls+numwalls+numwalls);
		for(i=1;i<=numwalls;i++)
		{
			tioffs[i] = tioffs[i-1]+((long)(tileng[i-1]+2));
			bmpkind[i] = 1+(wallheader[i]&7);
			if (bmpkind[i] == 3)
				bmpkind[i] = 4;
		}
		_asm \
		{
			mov es, lzwbuf
			mov cx, 256
			mov ax, 0
setuplzwbuf:
			inc ax
			mov di, ax
			mov byte ptr es:[di+8200], al
			mov bx, ax
			mov word ptr es:[di+bx], ax
			loop setuplzwbuf
			mov word ptr es:[0], 0
			mov byte ptr es:[8200], 0
		}
		for(i=0;i<numwalls;i++)
		{
			compleng = tileng[i];
			read(fil,&strtot,2);
			read(fil,&tempbuf[0],compleng);
			_asm \
			{
				mov bx, i
				shl bx, 1
				mov ax, walseg[bx]
				mov walsegg, ax
			}
			if ((emswalls > 0) && ((memtype[i]&1024) > 0))
				emsetpages(memtype[i]-1024,0);
			if (strtot > 0)
			{
				tempbuf[compleng] = 0;
				tempbuf[compleng+1] = 0;
				tempbuf[compleng+2] = 0;
				bytecnt2 = 0;
				bytecnt1 = 0;
				bitcnt = 0;
				currstr = 256;
				goalstr = 512;
				numbits = 9;
				do
				{
					_asm \
					{
						mov es, lzwbuf
						mov dl, bitcnt
						mov dh, numbits
						mov si, bytecnt2
						mov di, word ptr tempbuf[si]
						mov cl, dl
						shr di, cl
						mov cl, dh
						mov bx, 1
						shl bx, cl
						dec bx
						and di, bx
						add cl, dl
						cmp cl, 16
						jbe skipextrabits2
						mov al, byte ptr tempbuf[si+2]
						mov bx, 1
						rol bx, cl
						dec bx
						and ax, bx
						mov cl, dl
						ror ax, cl
						add di, ax
skipextrabits2:
						mov dat, di
						add dl, dh
						mov dh, 0
						mov bx, dx
						mov cl, 3
						shr bx, cl
						add bytecnt2, bx
						and dl, 7
						mov bitcnt, dl

						mov cx, 1
						mov bx, dat
						mov di, currstr
						shl di, 1
						mov es:[di], bx
						cmp bx, 256
						jb endsearchtable2
beginsearchtable2:
						inc cx
						mov al, es:[bx+8200]
						push ax
						shl bx, 1
						mov bx, es:[bx]
						cmp bx, 256
						jae beginsearchtable2
endsearchtable2:
						mov ax, bx
						mov ah, al
						shr di, 1
						mov es:[di+8199], ax
						shl bx, 1
						mov bx, es:[bx]
						push bx
						mov es, walsegg
						mov di, bytecnt1
reversedata:
						pop ax
						cmp di, 4096
						jae skipstoredata
						stosb
skipstoredata:
						loop reversedata
						mov bytecnt1, di
					}
					currstr++;
					if (currstr == goalstr)
					{
						numbits++;
						goalstr = (goalstr<<1);
					}
				}
				while (currstr <= strtot);
			}
			else
				_asm \
				{
					mov cx, 2048
					mov es, walsegg
					mov di, 0
					mov si, offset tempbuf
					mov ax, seg tempbuf
					push ds
					mov ds, ax
					rep movsw
					pop ds
				}
			if ((xmswalls > 0) && (memtype[i] != 65535) && ((memtype[i]&2048) > 0))
				xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[i]-2048))<<12,4096L);
			if (bmpkind[i+1] >= 2)
			{
				_asm \
				{
					mov es, walsegg
					mov bx, i
					shl bx, 1
					mov di, 0
begcheckltrim:
					cmp es:[di], 255
					jnz endcheckltrim
					inc di
					cmp di, 4096
					jb begcheckltrim
endcheckltrim:
					and di, 0xfc0
					mov lborder[bx+2], di
					mov di, 4095
begcheckrtrim:
					cmp es:[di], 255
					jnz endcheckrtrim
					sub di, 1
					jns begcheckrtrim
endcheckrtrim:
					and di, 0xfc0
					add di, 64
					mov rborder[bx+2], di
				}
			}
			else
			{
				lborder[i+1] = 0;
				rborder[i+1] = 4096;
			}
			if (i < 127)
				fade(64+(i>>1));
			if (i < (numwalls>>1))
				_asm \
				{
					mov ax, 0xa000
					mov es, ax
					mov di, 17950
					mov ax, numwalls
					mov cl, 4
					shr ax, cl
					sub di, ax
					mov dx, 0x3c4
					mov al, 2
					out dx, al
					mov ax, i
					mov cx, ax
					shr ax, 1
					shr ax, 1
					add di, ax
					mov al, 1
					and cl, 3
					shl al, cl
					inc dx
					out dx, al
					mov al, 255
					mov es:[di], al
				}
			else
				_asm \
				{
					mov ax, 0xa000
					mov es, ax
					mov di, 17950
					mov ax, numwalls
					mov cl, 4
					shr ax, cl
					sub di, ax
					mov dx, 0x3c4
					mov al, 2
					out dx, al
					mov ax, i
					mov bx, numwalls
					shr bx, 1
					sub ax, bx
					mov cx, ax
					shr ax, 1
					shr ax, 1
					add di, ax
					mov al, 1
					and cl, 3
					shl al, cl
					inc dx
					out dx, al
					mov al, 63
					mov es:[di], al
				}
		}
		close(fil);
		if (vms == 1)
		{
			memtype[numwalls-1] = memtype[0];
			walseg[numwalls-1] = walseg[0];
			memtype[0] = 0;
		}
	}
}

loadgame(gamenum)
int gamenum;
{
	char filename[20];
	int i, fil;

	filename[0] = 'S', filename[1] = 'A', filename[2] = 'V';
	filename[3] = 'G', filename[4] = 'A', filename[5] = 'M';
	filename[6] = 'E', filename[7] = gamenum+48;
	filename[8] = '.', filename[9] = 'D', filename[10] = 'A';
	filename[11] = 'T', filename[12] = 0;
	if((fil=open(filename,O_BINARY|O_RDONLY,S_IREAD))==-1)
		return(-1);
	musicoff();
	read(fil,&hiscorenam[0],16);
	read(fil,&hiscorenamstat,1);
	read(fil,&boardnum,2);

	if (boardnum >= numboards)
		quitgame = 2;
	filename[0] = 'L', filename[1] = 'A', filename[2] = 'B';
	filename[3] = 'S', filename[4] = 'N', filename[5] = 'G';
	filename[6] = (boardnum/10)+48, filename[7] = (boardnum%10)+48;
	filename[8] = 0;
	mute = 1;
	loadmusic(filename);
	musicon();

	read(fil,&scorecount,4);
	read(fil,&scoreclock,4);

	read(fil,&board[0][0],8192);
	read(fil,&skilevel,2);
	read(fil,&life,2);
	read(fil,&death,2);
	read(fil,&lifevests,2);
	read(fil,&lightnings,2);
	read(fil,&firepowers[0],6);
	read(fil,&bulchoose,2);
	read(fil,&keys[0],4);
	read(fil,&coins,2);
	read(fil,&compass,2);
	read(fil,&cheated,2);
	read(fil,&animate2,2);
	read(fil,&animate3,2);
	read(fil,&animate4,2);
	read(fil,&oscillate3,2);
	read(fil,&oscillate5,2);
	read(fil,&animate6,2);
	read(fil,&animate7,2);
	read(fil,&animate8,2);
	read(fil,&animate10,2);
	read(fil,&animate11,2);
	read(fil,&animate15,2);
	read(fil,&statusbar,2);
	read(fil,&statusbargoal,2);
	read(fil,&posx,2);
	read(fil,&posy,2);
	read(fil,&posz,2);
	read(fil,&ang,2);
	read(fil,&startx,2);
	read(fil,&starty,2);
	read(fil,&startang,2);
	read(fil,&angvel,2);
	read(fil,&vel,2);
	read(fil,&mxvel,2);
	read(fil,&myvel,2);
	read(fil,&svel,2);
	read(fil,&hvel,2);
	read(fil,&oldposx,2);
	read(fil,&oldposy,2);
	read(fil,&bulnum,2);
	read(fil,&bulang[0],bulnum<<1);
	read(fil,&bulkind[0],bulnum<<1);
	read(fil,&bulx[0],bulnum<<1);
	read(fil,&buly[0],bulnum<<1);
	read(fil,&bulstat[0],bulnum<<2);
	read(fil,&lastbulshoot,4);
	read(fil,&mnum,2);
	read(fil,&mposx[0],mnum<<1);
	read(fil,&mposy[0],mnum<<1);
	read(fil,&mgolx[0],mnum<<1);
	read(fil,&mgoly[0],mnum<<1);
	read(fil,&moldx[0],mnum<<1);
	read(fil,&moldy[0],mnum<<1);
	read(fil,&mstat[0],mnum<<1);
	read(fil,&mshock[0],mnum<<1);
	read(fil,&mshot[0],mnum);
	read(fil,&doorx,2);
	read(fil,&doory,2);
	read(fil,&doorstat,2);
	read(fil,&numwarps,1);
	read(fil,&justwarped,1);
	read(fil,&xwarp[0],numwarps);
	read(fil,&ywarp[0],numwarps);
	read(fil,&totalclock,4);
	ototclock = totalclock;
	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = totalclock;
	read(fil,&purpletime,4);
	read(fil,&greentime,4);
	read(fil,&capetime[0],8);
	read(fil,&musicstatus,4);
	read(fil,&clockspeed,2);
	read(fil,&count,4);
	read(fil,&countstop,4);
	read(fil,&nownote,2);
	read(fil,&i,2);
	read(fil,&chanage,18<<2);
	read(fil,&chanfreq,18);
	read(fil,&midiinst,2);
	read(fil,&mute,2);
	read(fil,&namrememberstat,1);
	read(fil,&fadewarpval,2);
	read(fil,&fadehurtval,2);
	read(fil,&slottime,2);
	read(fil,&slotpos[0],6);
	read(fil,&owecoins,2);
	read(fil,&owecoinwait,2);
	close(fil);
	if (((i == -1) || (i > 2)) && (musicsource >= 0))
	{
		musicoff();
		loadmusic(filename);
		musicon();
	}
	totalclock -= 2;
	copyslots(slotto);
	totalclock++;
	copyslots(slotto+1);
	totalclock++;
	if (memtype[map-1] != 65535)
	{
		if ((vms == 1) && (memtype[map-1] == 0))
			loadvms(map-1);
		if ((emswalls > 0) && ((memtype[map-1]&1024) > 0))
			emsetpages(memtype[map-1]-1024,0);
	}
	yourhereoldpos = ((posx>>10)<<6)+(posy>>10);
	_asm \
	{
		cld
		mov es, walseg[(map-1)+(map-1)]
		mov si, offset board
		mov ax, seg board
		mov di, 0
		mov cx, 4096
		push ds
		mov ds, ax
copyboard2:
		lodsw
		stosb
		loop copyboard2
		pop ds

		mov di, yourhereoldpos                      ;youareherepixel
		mov al, 255
		stosb
	}
	if ((xmswalls > 0) && ((memtype[map-1]&2048) > 0) && (memtype[map-1] != 65535))
		xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[map-1]-2048))<<12,4096L);
	if ((vidmode == 0) && (statusbargoal > 400))
	{
		statusbar -= 80;
		statusbargoal -= 80;
	}
	if ((vidmode == 1) && (statusbargoal < 400))
	{
		statusbar += 80;
		statusbargoal += 80;
	}
	lastpageoffset = 2880;
	pageoffset = 2880;
	if (vidmode == 0)
	{
		scrsize = 18000-2880;
		if (statusbar == 399)
			scrsize += 2880;
	}
	else
	{
		scrsize = 21600-2880;
		if (statusbar == 479)
		{
			scrsize += 2880;
			lastpageoffset = 0;
			pageoffset = 0;
		}
	}
	linecompare(statusbar);
	statusbaralldraw();
	fade(63);
}

savegame(gamenum)
int gamenum;
{
	char filename[20];
	int i, fil;

	filename[0] = 'S', filename[1] = 'A', filename[2] = 'V';
	filename[3] = 'G', filename[4] = 'A', filename[5] = 'M';
	filename[6] = 'E', filename[7] = gamenum+48;
	filename[8] = '.', filename[9] = 'D', filename[10] = 'A';
	filename[11] = 'T', filename[12] = 0;
	if((fil=open(filename,O_BINARY|O_CREAT|O_WRONLY,S_IWRITE))==-1)
		return(-1);
	write(fil,&hiscorenam[0],16);
	write(fil,&hiscorenamstat,1);
	write(fil,&boardnum,2);
	write(fil,&scorecount,4);
	write(fil,&scoreclock,4);

	for(i=0;i<16;i++)
		gamehead[gamenum][i] = hiscorenam[i];
	gamehead[gamenum][16] = hiscorenamstat;
	i = gamenum*27;
	_asm \
	{
		mov si, i
		mov ax, boardnum
		mov word ptr gamehead[si+17], ax
		mov ax, word ptr scorecount[0]
		mov dx, word ptr scorecount[2]
		mov word ptr gamehead[si+19], ax
		mov word ptr gamehead[si+21], dx
		mov ax, word ptr scoreclock[0]
		mov dx, word ptr scoreclock[2]
		mov word ptr gamehead[si+23], ax
		mov word ptr gamehead[si+25], dx
	}
	gamexist[gamenum] = 1;
	write(fil,&board[0][0],8192);
	write(fil,&skilevel,2);
	write(fil,&life,2);
	write(fil,&death,2);
	write(fil,&lifevests,2);
	write(fil,&lightnings,2);
	write(fil,&firepowers[0],6);
	write(fil,&bulchoose,2);
	write(fil,&keys[0],4);
	write(fil,&coins,2);
	write(fil,&compass,2);
	write(fil,&cheated,2);
	write(fil,&animate2,2);
	write(fil,&animate3,2);
	write(fil,&animate4,2);
	write(fil,&oscillate3,2);
	write(fil,&oscillate5,2);
	write(fil,&animate6,2);
	write(fil,&animate7,2);
	write(fil,&animate8,2);
	write(fil,&animate10,2);
	write(fil,&animate11,2);
	write(fil,&animate15,2);
	write(fil,&statusbar,2);
	write(fil,&statusbargoal,2);
	write(fil,&posx,2);
	write(fil,&posy,2);
	write(fil,&posz,2);
	write(fil,&ang,2);
	write(fil,&startx,2);
	write(fil,&starty,2);
	write(fil,&startang,2);
	write(fil,&angvel,2);
	write(fil,&vel,2);
	write(fil,&mxvel,2);
	write(fil,&myvel,2);
	write(fil,&svel,2);
	write(fil,&hvel,2);
	write(fil,&oldposx,2);
	write(fil,&oldposy,2);
	write(fil,&bulnum,2);
	write(fil,&bulang[0],bulnum<<1);
	write(fil,&bulkind[0],bulnum<<1);
	write(fil,&bulx[0],bulnum<<1);
	write(fil,&buly[0],bulnum<<1);
	write(fil,&bulstat[0],bulnum<<2);
	write(fil,&lastbulshoot,4);
	write(fil,&mnum,2);
	write(fil,&mposx[0],mnum<<1);
	write(fil,&mposy[0],mnum<<1);
	write(fil,&mgolx[0],mnum<<1);
	write(fil,&mgoly[0],mnum<<1);
	write(fil,&moldx[0],mnum<<1);
	write(fil,&moldy[0],mnum<<1);
	write(fil,&mstat[0],mnum<<1);
	write(fil,&mshock[0],mnum<<1);
	write(fil,&mshot[0],mnum);
	write(fil,&doorx,2);
	write(fil,&doory,2);
	write(fil,&doorstat,2);
	write(fil,&numwarps,1);
	write(fil,&justwarped,1);
	write(fil,&xwarp[0],numwarps);
	write(fil,&ywarp[0],numwarps);
	write(fil,&totalclock,4);
	write(fil,&purpletime,4);
	write(fil,&greentime,4);
	write(fil,&capetime[0],8);
	write(fil,&musicstatus,4);
	write(fil,&clockspeed,2);
	write(fil,&count,4);
	write(fil,&countstop,4);
	write(fil,&nownote,2);
	write(fil,&musicsource,2);
	write(fil,&chanage,18<<2);
	write(fil,&chanfreq,18);
	write(fil,&midiinst,2);
	write(fil,&mute,2);
	write(fil,&namrememberstat,1);
	write(fil,&fadewarpval,2);
	write(fil,&fadehurtval,2);
	write(fil,&slottime,2);
	write(fil,&slotpos[0],6);
	write(fil,&owecoins,2);
	write(fil,&owecoinwait,2);
	close(fil);
	ksay(16);
}

introduction(songnum)
int songnum;
{
	unsigned int plc;
	int i, j, k, m, x, y, fil, sharemessplc, newgamepisode;
	int animater2, leaveintro;
	long dai, dalasti, pickskiltime;

	outp(0x3c4,0x4); outp(0x3c5,inp(0x3c5)&(255-8));    //Chain 4 off
	outp(0x3ce,0x5); outp(0x3cf,64);                    //Tim's suggestion
	outp(0x3d4,0x17); outp(0x3d5,inp(0x3d5)|64);        //Word Mode
	outp(0x3d4,0x14); outp(0x3d5,inp(0x3d5)&(255-64));  //Double Word Mode
	outp(0x3ce,8); outp(0x3cf,255);
	lside = 20;
	rside = 340;
	dside = 200;
	halfheight = 100;
	scrsize = 18000;
	dalasti = 0;
	_asm \
	{
		cld
		mov ax, 0xa000
		mov es, ax
		mov dx, 0x3c4
		mov ax, 0x0f02
		out dx, al
		inc dx
		mov al, ah
		out dx, al

		mov di, 18000                     ;clear below Kenlab sign (check di)
		mov cx, 3780
		mov al, 80
		rep stosb
	}
	if (songnum == 0)
		loadmusic("INTRO");
	else
		loadmusic("INTRO2");
	pageoffset = 0;
	clockspeed = 0;
	ototclock = -1;
	totalclock = 1;
	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
	plc = 5;
	outp(0x3d4,0xc); outp(0x3d5,plc>>8);
	outp(0x3d4,0xd); outp(0x3d5,plc&255);
	_asm \
	{
		mov dx, 0x3d4
		mov al, 0x13
		out dx, al
		mov dx, 0x3da
retrace1a:
		in al, dx
		test al, 8
		jnz retrace1a
retrace2a:
		in al, dx
		test al, 8
		jz retrace2a
		mov dx, 0x3d5                                     ;Bytes per Line
		mov al, 45
		out dx, al
	}
	musicon();
	if (saidwelcome == 0)
	{
		ksay(18);
		saidwelcome = 1;
	}
	for(i=0;i<360;i++)
		height[i] = 0;
	boardnum = 0;
	keystatus[57] = 0;
	keystatus[28] = 0;
	keystatus[1] = 0;
	sharemessplc = 64;
	newgamepisode = 1;
	animater2 = 0;
	totalclock = 0;
	leaveintro = 0;
	pickskiltime = -32768;
	while (leaveintro == 0)
	{
		bstatus = 0;
		if (moustat == 0)
			_asm \
			{
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
		if (joystat == 0)
			_asm
			{
				mov dx, 0x201
				in al, dx
				and ax, 0x30
				xor al, 0x30
				or bstatus, ax
			}
		j = (((int)labs((totalclock%120)-60))>>3);
		_asm \
		{
			mov dx, 0x3da
retrace1:
			in al, dx
			test al, 8
			jnz retrace1
retrace2:
			in al, dx
			test al, 8
			jz retrace2
		}
		totalclock += clockspeed;
		clockspeed = 0;
		dalasti = dai;
		dai = (totalclock>>2);
		if ((dai >= 0) && (dai < 128))
			fade(64+((int)(dai>>1)));
		if ((dai >= 128) && (dalasti < 128))
			fade(63);
		if ((dai >= 0) && (dai < 300))
		{
			plc = 5+times90[dai>>2];
			if (plc > 3785)
				plc = 3785;
			pageoffset = 21780;
			outp(0x3d4,0xc); outp(0x3d5,(plc>>8)&255);
			outp(0x3d4,0xd); outp(0x3d5,plc&255);
			_asm \
			{
				cld                      ;clear prepare copy area
				mov dx, 0x3c4
				mov ax, 0x0f02
				out dx, al
				inc dx
				mov al, ah
				out dx, al
				mov ax, 0xa000
				mov es, ax
				mov cx, 7920
				mov di, 21780+10080
				mov ax, 0x5050
				cmp videotype, 0
				je introclear4a
				rep stosb
				jmp introclear4b
introclear4a:
				shr cx, 1
				rep stosw
introclear4b:
			}
			if (dai < 240)
			{
				pictur(180+(240-((int)dai)),164,64*4,((((unsigned)dai)<<8)/15)&2047,copyright);
				pictur(180-(240-((int)dai)),164,64*4,((((unsigned)dai)<<8)/15)&2047^2047,copyright);
			}
			else
			{
				spridraw(180-32,164-32,64*4,copyright);
				if (vidmode == 0)
					m = 20;
				else
					m = 0;
				statusbardraw(0,44,63,20,128+m,112,sodapics,pageoffset);
			}
			copywindow((unsigned)(21780+10080),(unsigned)(3780+10080),7920);
		}
		if ((dai >= 300) && (dai < 364))
		{
			pageoffset = 21780;
			outp(0x3d4,0xc); outp(0x3d5,(plc>>8)&255);
			outp(0x3d4,0xd); outp(0x3d5,plc&255);
			_asm \
			{
				cld                      ;clear prepare copy area
				mov dx, 0x3c4
				mov ax, 0x0f02
				out dx, al
				inc dx
				mov al, ah
				out dx, al
				mov ax, 0xa000
				mov es, ax
				mov cx, 7920
				mov di, 21780+10080
				mov ax, 0x5050
				cmp videotype, 0
				je introclear5a
				rep stosb
				jmp introclear5b
introclear5a:
				shr cx, 1
				rep stosw
introclear5b:
			}
			m = (364-dai);
			spridraw(180-(m>>1),164-(m>>1),m<<2,copyright);
			if (vidmode == 0)
				m = 20;
			else
				m = 0;
			statusbardraw(0,44,63,20,128+m,112,sodapics,pageoffset);
			copywindow((unsigned)(21780+10080),(unsigned)(3780+10080),7920);
		}
		if ((dai >= 364) && (dai < 424))
		{
			pageoffset = 3780;
			m = (dai-360);
			spridraw(180-96-(m>>1),152-(m>>1),m<<2,episodesign1off);
			spridraw(180-(m>>1),152-(m>>1),m<<2,episodesign2off);
			spridraw(180+96-(m>>1),152-(m>>1),m<<2,episodesign3off);
		}
		if (dai >= 424)
		{
			if (sharemessplc >= 0)
			{
				if (vidmode == 0)
					m = 20;
				else
					m = 0;
				k = 0;
				if (numboards == 10)
					k = 32;
				pageoffset = 3780;
				statusbardraw(sharemessplc,k,64-sharemessplc,32,m,32,sharewaremessage,pageoffset);
				statusbardraw(0,k,64-sharemessplc,32,256+m+sharemessplc,32,sharewaremessage,pageoffset);
				sharemessplc--;
			}
			if (pickskiltime < 0)
			{
				pageoffset = 3780;
				if (dalasti < 424)
				{
					if (vidmode == 0)
						m = 20;
					else
						m = 0;
					statusbardraw(0,36,63,8,128+m,112,sodapics,pageoffset);
					strcpy(&textbuf[0],"Select Episode");
					textprint(124,112,(char)96,pageoffset);
				}
				animater2 = (dai&16);
				if ((newgamepisode != 1) || (animater2 == 0))
					spridraw(180-128,120,64*4,episodesign1off);
				else
					spridraw(180-128,120,64*4,episodesign1on);
				if ((newgamepisode != 2) || (animater2 == 0))
					spridraw(180-32,120,64*4,episodesign2off);
				else
					spridraw(180-32,120,64*4,episodesign2on);
				if ((newgamepisode != 3) || (animater2 == 0))
					spridraw(180+64,120,64*4,episodesign3off);
				else
					spridraw(180+64,120,64*4,episodesign3on);
			}
			else if (dai >= pickskiltime+128)
			{
				pageoffset = 3780;
				if (dalasti < pickskiltime+128)
				{
					strcpy(&textbuf[0],"Select Difficulty");
					textprint(112,112,(char)96,pageoffset);
				}
				animater2 = (dai&16);
				if ((skilevel != 0) || (animater2 == 0))
					spridraw(180-48-32,120,64*4,skilevel1);
				else
					spridraw(180-48-32,120,64*4,skilblank);
				if ((skilevel != 1) || (animater2 == 0))
					spridraw(180+48-32,120,64*4,skilevel2);
				else
					spridraw(180+48-32,120,64*4,skilblank);
			}
		}
		if ((pickskiltime >= 0) && (dai < pickskiltime+64))
		{
			pageoffset = 21780;
			outp(0x3d4,0xc); outp(0x3d5,(plc>>8)&255);
			outp(0x3d4,0xd); outp(0x3d5,plc&255);
			_asm \
			{
				cld                      ;clear prepare copy area
				mov dx, 0x3c4
				mov ax, 0x0f02
				out dx, al
				inc dx
				mov al, ah
				out dx, al
				mov ax, 0xa000
				mov es, ax
				mov cx, 7920
				mov di, 21780+10080
				mov ax, 0x5050
				cmp videotype, 0
				je introclear6a
				rep stosb
				jmp introclear6b
introclear6a:
				shr cx, 1
				rep stosw
introclear6b:
			}
			m = ((pickskiltime+63)-dai);
			spridraw(180-96-(m>>1),152-(m>>1),m<<2,episodesign1off);
			spridraw(180-(m>>1),152-(m>>1),m<<2,episodesign2off);
			spridraw(180+96-(m>>1),152-(m>>1),m<<2,episodesign3off);
			copywindow((unsigned)(21780+10080),(unsigned)(3780+10080),7920);
		}
		if ((pickskiltime >= 0) && (dai >= pickskiltime+64) && (dai < pickskiltime+128))
		{
			pageoffset = 21780;
			outp(0x3d4,0xc); outp(0x3d5,(plc>>8)&255);
			outp(0x3d4,0xd); outp(0x3d5,plc&255);
			_asm \
			{
				cld                      ;clear prepare copy area
				mov dx, 0x3c4
				mov ax, 0x0f02
				out dx, al
				inc dx
				mov al, ah
				out dx, al
				mov ax, 0xa000
				mov es, ax
				mov cx, 7920
				mov di, 21780+10080
				mov ax, 0x5050
				cmp videotype, 0
				je introclear7a
				rep stosb
				jmp introclear7b
introclear7a:
				shr cx, 1
				rep stosw
introclear7b:
			}
			m = (dai-(pickskiltime+64));
			spridraw(180-48-(m>>1),152-(m>>1),m<<2,skilevel1);
			spridraw(180+48-(m>>1),152-(m>>1),m<<2,skilevel2);
			copywindow((unsigned)(21780+10080),(unsigned)(3780+10080),7920);
		}
		if ((keystatus[0xc9]|keystatus[0xc8]|keystatus[0xcb]) != 0)
		{
			if (pickskiltime < 0)
			{
				newgamepisode--;
				if (newgamepisode == 0)
					newgamepisode = 3;
			}
			else
				skilevel = 1 - skilevel;
			ksay(27);
			keystatus[0xc9] = 0;
			keystatus[0xc8] = 0;
			keystatus[0xcb] = 0;
		}
		if ((keystatus[0xd1]|keystatus[0xd0]|keystatus[0xcd]) != 0)
		{
			if (pickskiltime < 0)
			{
				newgamepisode++;
				if (newgamepisode == 4)
					newgamepisode = 1;
			}
			else
				skilevel = 1 - skilevel;
			ksay(27);
			keystatus[0xd1] = 0;
			keystatus[0xd0] = 0;
			keystatus[0xcd] = 0;
		}
		if (keystatus[keydefs[17]] > 0)
		{
			lastunlock = 1;
			lastshoot = 1;
			lastbarchange = 1;
			m = pageoffset;
			k = lastpageoffset;
			copywindow(plc,(unsigned)29536,18000);
			if (vidmode == 1)
			{
				pageoffset = plc-1805;
				if (pageoffset > 62000)
					pageoffset = 0;
				lastpageoffset = pageoffset;
			}
			else
			{
				pageoffset = plc-5;
				lastpageoffset = pageoffset;
			}
			sortcnt = -1;
			saidwelcome = plc;
			j = mainmenu();
			saidwelcome = 1;
			sortcnt = 0;
			if ((j == 0) && (newgameplace >= 0))
			{
				if (newgameplace == 0) boardnum = 0, newgamepisode = 1;
				if (newgameplace == 1) boardnum = 10, newgamepisode = 2;
				if (newgameplace == 2) boardnum = 20, newgamepisode = 3;
				leaveintro = 1;
			}
			if ((j == 1) && (loadsavegameplace >= 0))
			{
				musicoff();
				setgamevideomode();
				if (vidmode == 0)
				{
					lside = 20;
					rside = 340;
					dside = 200;
					halfheight = 100;
					scrsize = 18000-2880;
				}
				else
				{
					lside = 0;
					rside = 360;
					dside = 240;
					halfheight = 120;
					scrsize = 21600-2880;
				}
				loadgame(loadsavegameplace);
				fade(63);
				return(0);
			}
			if (j == 8)
			{
				newgamepisode = 1;
				leaveintro = 1;
				quitgame = 1;
			}
			keystatus[keydefs[17]] = 0;
			keystatus[1] = 0;
			keystatus[57] = 0;
			keystatus[28] = 0;
			pageoffset = m;
			lastpageoffset = k;
			copywindow((unsigned)29536,plc,18000);
			clockspeed = 0;
		}
		if ((keystatus[1] != 0) || (keystatus[57] != 0) || (keystatus[28] != 0) || (bstatus != 0))
		{
			if (dai < 420)
				leaveintro = 1;
			else if (((newgamepisode == 2) && (numboards < 20)) || ((newgamepisode == 3) && (numboards < 30)))
			{
				ksay(12);
				copywindow(plc,(unsigned)39744,18000);
				m = pageoffset;
				k = lastpageoffset;
				if (vidmode == 1)
				{
					pageoffset = plc-1805;
					lastpageoffset = plc-1805;
				}
				else
				{
					pageoffset = plc-5;
					lastpageoffset = plc-5;
				}
				orderinfomenu();
				pageoffset = m;
				lastpageoffset = k;
				copywindow((unsigned)39744,plc,18000);
				clockspeed = 0;
			}
			else
			{
				if (pickskiltime == -32768)
					pickskiltime = dai;
				else
					leaveintro = 1;
			}
			keystatus[1] = 0;
			keystatus[57] = 0;
			keystatus[28] = 0;
		}
	}
	if (newgamepisode == 1) boardnum = 0;
	if (newgamepisode == 2) boardnum = 10;
	if (newgamepisode == 3) boardnum = 20;
	if ((newgamepisode == 2) && (numboards < 20))
		newgamepisode = 1, boardnum = 0;
	if ((newgamepisode == 3) && (numboards < 30))
		newgamepisode = 1, boardnum = 0;
	musicoff();
	setgamevideomode();
	fade(0);
	keystatus[57] = 0;
	keystatus[28] = 0;
	keystatus[1] = 0;
	if (vidmode == 0)
	{
		lside = 20;
		rside = 340;
		dside = 200;
		halfheight = 100;
		scrsize = 18000-2880;
	}
	else
	{
		lside = 0;
		rside = 360;
		dside = 240;
		halfheight = 120;
		scrsize = 21600-2880;
	}
	_asm \
	{
		mov ax, 0xa000
		mov es, ax
		cld
		mov di, 0
		mov cx, 24576
		mov ax, 0x5050
		cmp videotype, 0
		je introclear3a
		shl cx, 1
		rep stosb
		jmp introclear3b
introclear3a:
		rep stosw
introclear3b:
	}
	pageoffset = 2880;
	loadboard();
	pageoffset = 2880;
	owecoins = 0;
	sortcnt = 0;
	oldlife = 0;
	life = 4095;
	death = 4095;
	lifevests = 1;
	switch (boardnum)
	{
		case 0: firepowers[0] = 0; firepowers[1] = 0;
				  firepowers[2] = 0; lightnings = 0;
				  break;
		case 10: firepowers[0] = 3; firepowers[1] = 2;
				  firepowers[2] = 0; lightnings = 1;
				  break;
		case 20: firepowers[0] = 4; firepowers[1] = 3;
				  firepowers[2] = 2; lightnings = 2;
				  break;
	}
	coins = 0;
	bulchoose = 0;
	animate2 = 0;
	animate3 = 0;
	animate4 = 0;
	oscillate3 = 0;
	oscillate5 = 0;
	animate6 = 0;
	animate7 = 0;
	animate8 = 0;
	animate10 = 0;
	animate11 = 0;
	animate15 = 0;
	ototclock = -1;
	totalclock = 1;
	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
	purpletime = 0;
	greentime = 0;
	capetime[0] = 0;
	capetime[1] = 0;
	compass = 0;
	if (boardnum >= 10) compass = 1;
	cheated = 0;
	doorstat = 0;
	statusbar = 335;
	if (vidmode == 1)
		statusbar += 80;
	statusbargoal = statusbar;
	linecompare(statusbar);
	lastpageoffset = 2880;
	pageoffset = 2880;
	namrememberstat = hiscorenamstat;
	hiscorenamstat = 0;
	hiscorenam[0] = 0;
	clockspeed = 0;
	scoreclock = 0;
	scorecount = 0;
	fadewarpval = 63;
	fadehurtval = 0;
	statusbaralldraw();
}

loadmusic(filename)
char filename[12];
{
	char buffer[256], instbuf[11];
	int infile, i, j, k, numfiles;
	long filoffs;

	if (musicsource == -1)
		return(-1);
	if (firstime == 1)
	{
		if (musicsource == 1)
		{
			midiscrap = 256;
			while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
				midiscrap--;
			outp(statport,0x3f);
			midiscrap = 32000;
			while ((midiscrap > 0) && ((inp(dataport) & 0xfe) != 0))
				midiscrap--;
		}
		if (musicsource == 2)
		{
			if((infile=open("insts.dat",O_BINARY|O_RDONLY,S_IREAD))==-1)
				return(-1);
			for(i=0;i<256;i++)
			{
				read(infile,&buffer,33);
				for(j=0;j<11;j++)
					inst[i][j] = buffer[j+20];
			}
			close(infile);
			numchans = 9;
			outdata((char)0,(char)0x1,(char)32);  //clear test stuff
			outdata((char)0,(char)0x4,(char)0);   //reset
			outdata((char)0,(char)0x8,(char)0);   //2-operator synthesis
			firstime = 0;
		}
	}
	if ((infile=open("songs.kzp",O_BINARY|O_RDONLY,S_IREAD))==-1)
		return(-1);
	read(infile,&numfiles,2);
	i = 0;
	j = 1;
	while ((j == 1) && (i < numfiles))
	{
		read(infile,&buffer[0],12);
		j = 0;
		k = 0;
		while ((filename[k] != 0) && (k < 8))
		{
			if (buffer[k] != filename[k])
				j = 1;
			k++;
		}
		i++;
	}
	if (j == 1)
	{

		close(infile);
		return(-1);
	}
	_asm \
	{
		mov ax, word ptr buffer[8]
		mov dx, word ptr buffer[10]
		mov word ptr filoffs[0], ax
		mov word ptr filoffs[2], dx
	}
	lseek(infile,filoffs,SEEK_SET);
	read(infile,&trinst[0],16);
	read(infile,&trquant[0],16);
	read(infile,&trchan[0],16);
	read(infile,&trprio[0],16);
	read(infile,&trvol[0],16);
	read(infile,&numnotes,2);
	_asm \
	{
		mov ah, 0x3f
		mov bx, infile
		mov cx, numnotes
		shl cx, 1
		shl cx, 1
		mov dx, 0
		push ds
		mov ds, note
		int 0x21
		pop ds
	}
	close(infile);
	numchans = 9-trchan[11]*3;
	if (musicsource == 1)
		setmidiinsts();
	if (musicsource == 2)
	{
		if (trchan[11] == 0)
		{
			drumstat = 0;
			outdata((char)0,(unsigned char)0xbd,(unsigned char)drumstat);
		}
		if (trchan[11] == 1)
		{
			for(i=0;i<11;i++)
				instbuf[i] = inst[trinst[11]][i];
			instbuf[1] = ((instbuf[1]&192)|(trvol[11])^63);
			setinst(0,6,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
			for(i=0;i<5;i++)
				instbuf[i] = inst[trinst[12]][i];
			for(i=5;i<11;i++)
				instbuf[i] = inst[trinst[15]][i];
			instbuf[1] = ((instbuf[1]&192)|(trvol[12])^63);
			instbuf[6] = ((instbuf[6]&192)|(trvol[15])^63);
			setinst(0,7,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
			for(i=0;i<5;i++)
				instbuf[i] = inst[trinst[14]][i];
			for(i=5;i<11;i++)
				instbuf[i] = inst[trinst[13]][i];
			instbuf[1] = ((instbuf[1]&192)|(trvol[14])^63);
			instbuf[6] = ((instbuf[6]&192)|(trvol[13])^63);
			setinst(0,8,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
			outdata((char)0,(unsigned char)0xa6,(unsigned char)(600&255));
			outdata((char)0,(unsigned char)0xb6,(unsigned char)((600>>8)&223));
			outdata((char)0,(unsigned char)0xa7,(unsigned char)(400&255));
			outdata((char)0,(unsigned char)0xb7,(unsigned char)((400>>8)&223));
			outdata((char)0,(unsigned char)0xa8,(unsigned char)(5510&255));
			outdata((char)0,(unsigned char)0xb8,(unsigned char)((5510>>8)&223));
			drumstat = 32;
			outdata((char)0,(unsigned char)0xbd,(unsigned char)drumstat);
		}
	}
}

outdata(synth,index,data)
unsigned char synth,index,data;
{
	if (synth == 1)
		_asm mov dx, 0x38a ; get the right 3812 address
	else
		_asm mov dx, 0x388 ; get the left 3812 address
	_asm \
	{
		mov ax, word ptr index    ; get the index value
		out dx, al                ; output to both chips
		in al, dx                 ; slow down for the index to settle
		in al, dx
		in al, dx
		in al, dx
		in al, dx
		inc dx                    ; move to the data register
		mov ax, word ptr data
		out dx, al                ; write the data out
		mov cx, 33                ; load the loop count
 lbl1:in al, dx
		loop lbl1
	}
}

musicon()
{
	char ch;
	int i, j, k, offs, ksaystat;
	unsigned char instbuf[11];
	unsigned long templong;

	if (musicsource >= 0)
	{
		for(i=0;i<numchans;i++)
		{
			chantrack[i] = 0;
			chanage[i] = 0;
		}
		j = 0;
		for(i=0;i<16;i++)
			if ((trchan[i] > 0) && (j < numchans))
			{
				k = trchan[i];
				while ((j < numchans) && (k > 0))
				{
					chantrack[j] = i;
					k--;
					j++;
				}
			}
		for(i=0;i<numchans;i++)
		{
			if (musicsource == 2)
			{
				for(j=0;j<11;j++)
					instbuf[j] = inst[trinst[chantrack[i]]][j];
				instbuf[1] = ((instbuf[1]&192)|(63-trvol[chantrack[i]]));
				setinst(0,i,instbuf[0],instbuf[1],instbuf[2],instbuf[3],instbuf[4],instbuf[5],instbuf[6],instbuf[7],instbuf[8],instbuf[9],instbuf[10]);
			}
			chanfreq[i] = 0;
		}
	}
	_asm \
	{
		mov es, note
		mov ax, word ptr es:[0]
		mov dx, word ptr es:[2]
		mov word ptr templong[0], ax
		mov word ptr templong[2], dx
	}
	count = (templong>>12)-1;
	countstop = (templong>>12)-1;
	nownote = 0;
	musicstatus = 1;
	ksaystat = 0;
	if (speechstatus == 1)
	{
		if ((sndaddr&32768) > 0)
			snd += 2048;
		_asm \
		{
			xor ax, ax
			mov es, ax
			mov cx, ksaycodestart
			mov dx, snd
			cli
			cmp word ptr es:[32], cx
			jne skiprestoreintvectb
			cmp word ptr es:[34], dx
			jne skiprestoreintvectb
			mov ksaystat, 1
			mov es, snd
			mov cx, offset ksmhandler        ;snd[0-3] points to music vector
			mov dx, seg ksmhandler
			mov word ptr es:[0], cx
			mov word ptr es:[2], dx
skiprestoreintvectb:
			sti
		}
		if ((sndaddr&32768) > 0)
			snd -= 2048;
	}
	if (ksaystat == 0)
	{
		outp(0x43,54); outp(0x40,4972&255); outp(0x40,4972>>8);
		_disable(); _dos_setvect(0x8, ksmhandler); _enable();
	}
}

musicoff()
{
	int i, ksaystat;

	ksaystat = 0;
	if (speechstatus == 1)
	{
		if ((sndaddr&32768) > 0)
			snd += 2048;
		_asm \
		{
			xor ax, ax
			mov es, ax
			mov cx, ksaycodestart
			mov dx, snd
			cli
			cmp word ptr es:[32], cx
			jne skiprestoreintvectc
			cmp word ptr es:[34], dx
			jne skiprestoreintvectc
			mov ksaystat, 1
			mov es, snd
			mov cx, word ptr oldhand[0]     ;snd[0-3] points to originialvector
			mov dx, word ptr oldhand[2]
			mov word ptr es:[0], cx
			mov word ptr es:[2], dx
skiprestoreintvectc:
			sti
		}
		if ((sndaddr&32768) > 0)
			snd -= 2048;
	}
	if (ksaystat == 0)
	{
		outp(0x43,54); outp(0x40,255); outp(0x40,255);
		_disable(); _dos_setvect(0x8, oldhand); _enable();
		if (musicsource == 0)
			outp(97,inp(97)&252);
	}
	if (musicsource == 1)
		for(i=0;i<numchans;i++)
		{
			midiscrap = 256;
			while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
				midiscrap--;
			if (midiscrap > 0)
			{
				if (i < 6)
					outp(dataport,0x90);
				else
					outp(dataport,0x91);
			}
			midiscrap = 256;
			while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
				midiscrap--;
			if (midiscrap > 0)
				outp(dataport,chanfreq[i]+35);
			midiscrap = 256;
			while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
				midiscrap--;
			if (midiscrap > 0)
				outp(dataport,0);
		}
	if (musicsource == 2)
		for(i=0;i<numchans;i++)
		{
			outdata((char)0,(unsigned char)(0xa0+i),(char)0);
			outdata((char)0,(unsigned char)(0xb0+i),(char)0);
		}
	musicstatus = 0;
}

void interrupt far ksmhandler()
{
	int i, j, quanter, bufnum, chan, drumnum, freq;
	unsigned long temp, templong;

	clockspeed++;
	count++;
	if ((count >= countstop) && (musicsource >= 0))
	{
		bufnum = 0;
		while (count >= countstop)
		{
			_asm \
			{
				push es
				mov es, note
				mov di, nownote
				shl di, 1
				shl di, 1
				mov ax, word ptr es:[di]
				mov dx, word ptr es:[di+2]
				mov word ptr templong[0], ax
				mov word ptr templong[2], dx
				pop es
			}
			if (musicsource == 0)
				if ((((templong>>8)&15) == 0) && ((templong&64) > 0))
					databuf[bufnum++] = (unsigned char)(templong&63);
			if (musicsource > 0)
			{
				if (((templong&255) >= 1) && ((templong&255) <= 61))
				{
					i = 0;
					while (((chanfreq[i] != (templong&63)) || (chantrack[i] != ((templong>>8)&15))) && (i < numchans))
						i++;
					if (i < numchans)
					{
						if (musicsource == 1)
						{
							if (i < 6)
								databuf[bufnum++] = (unsigned char)(0x90);
							else
								databuf[bufnum++] = (unsigned char)(0x91);
							databuf[bufnum++] = (unsigned char)(templong&63)+35;
							databuf[bufnum++] = (unsigned char)0;
						}
						if (musicsource == 2)
						{
							databuf[bufnum++] = (unsigned char)(0xa0+i);
							databuf[bufnum++] = (unsigned char)(adlibfreq[templong&63]&255);
							databuf[bufnum++] = (unsigned char)(0xb0+i);
							databuf[bufnum++] = (unsigned char)((adlibfreq[templong&63]>>8)&223);
						}
						chanfreq[i] = 0;
						chanage[i] = 0;
					}
				}
				else if (((templong&255) >= 65) && ((templong&255) <= 125))
				{
					if (((templong>>8)&15) < 11)
					{
						temp = 0;
						i = numchans;
						for(j=0;j<numchans;j++)
							if ((countstop - chanage[j] >= temp) && (chantrack[j] == ((templong>>8)&15)))
							{
								temp = countstop - chanage[j];
								i = j;
							}
						if (i < numchans)
						{
							if (musicsource == 1)
							{
								if (i < 6)
									databuf[bufnum++] = (unsigned char)(0x90);
								else
									databuf[bufnum++] = (unsigned char)(0x91);
								databuf[bufnum++] = (unsigned char)(templong&63)+35;
								databuf[bufnum++] = (unsigned char)64;
							}
							if (musicsource == 2)
							{
								databuf[bufnum++] = (unsigned char)(0xa0+i);
								databuf[bufnum++] = (unsigned char)0;
								databuf[bufnum++] = (unsigned char)(0xb0+i);
								databuf[bufnum++] = (unsigned char)0;
								databuf[bufnum++] = (unsigned char)(0xa0+i);
								databuf[bufnum++] = (unsigned char)(adlibfreq[templong&63]&255);
								databuf[bufnum++] = (unsigned char)(0xb0+i);
								databuf[bufnum++] = (unsigned char)((adlibfreq[templong&63]>>8)|32);
							}
							chanfreq[i] = templong&63;
							chanage[i] = countstop;
						}
					}
					else
					{
						if (musicsource == 1)
						{
							databuf[bufnum++] = (unsigned char)(0x92);
							switch((templong>>8)&15)
							{
								case 11: drumnum = 38; break;
								case 12: drumnum = 43; break;
								case 13: drumnum = 64; break;
								case 14: drumnum = 50; break;
								case 15: drumnum = 48; break;
							}
							databuf[bufnum++] = (unsigned char)drumnum;
							databuf[bufnum++] = (unsigned char)64;
						}
						if (musicsource == 2)
						{
							freq = adlibfreq[templong&63];
							switch((unsigned char)((templong>>8)&15))
							{
								case 11: drumnum = 16; chan = 6; freq -= 2048; break;
								case 12: drumnum = 8; chan = 7; freq -= 2048; break;
								case 13: drumnum = 4; chan = 8; break;
								case 14: drumnum = 2; chan = 8; break;
								case 15: drumnum = 1; chan = 7; freq -= 2048; break;
							}
							databuf[bufnum++] = (unsigned char)(0xa0+chan);
							databuf[bufnum++] = (unsigned char)(freq&255);
							databuf[bufnum++] = (unsigned char)(0xb0+chan);
							databuf[bufnum++] = (unsigned char)((freq>>8)&223);
							databuf[bufnum++] = (unsigned char)(0xbd);
							databuf[bufnum++] = (((unsigned char)(drumstat))&((unsigned char)(255-drumnum)));
							drumstat |= drumnum;
							databuf[bufnum++] = (unsigned char)(0xbd);
							databuf[bufnum++] = (unsigned char)(drumstat);
						}
					}
				}
			}
			nownote++;
			if (nownote >= numnotes)
				nownote = 0;
			_asm \
			{
				push es
				mov es, note
				mov di, nownote
				shl di, 1
				shl di, 1
				mov ax, word ptr es:[di]
				mov dx, word ptr es:[di+2]
				mov word ptr templong[0], ax
				mov word ptr templong[2], dx
				pop es
			}
			if (nownote == 0)
				count = (templong>>12)-1;
			quanter = (240/trquant[(templong>>8)&15]);
			countstop = (((templong>>12)+(quanter>>1)) / quanter) * quanter;
		}
		if (mute == 0)
		{
			if (musicsource == 0)
			{
				j = 0;
				for(i=0;i<bufnum;i++)
					if (databuf[i] > j)
						j = databuf[i];
				if (j == 0)
					outp(97,inp(97)&0xfc);
				if (j > 0)
				{
					outp(67,182);
					outp(66,(unsigned)(1193280L / pcfreq[j])&255);
					outp(66,(unsigned)(1193280L / pcfreq[j])>>8);
					outp(97,inp(97)|0x3);
				}
			}
			if (musicsource == 1)
				for(i=0;i<bufnum;i++)
				{
					midiscrap = 256;
					while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
						midiscrap--;
					if (midiscrap > 0)
						outp(dataport,databuf[i]);
				}
			if (musicsource == 2)
				for(i=0;i<bufnum;i+=2)
					_asm \
					{
						mov dx, 0x388                ; get the left 3812 address
						mov bx, i
						mov ax, word ptr databuf[bx] ; get the index value
						out dx, al                   ; output to both chips
						in al, dx                    ; slow down for the index to settle
						in al, dx
						in al, dx
						in al, dx
						in al, dx
						inc dx                       ; move to the data register
						inc bx
						mov ax, word ptr databuf[bx]
						out dx, al                   ; write the data out
						mov cx, 33                   ; load the loop count
				 lbl2:in al, dx
						loop lbl2
					}
		}
	}
	outp(0x20,0x20);
}

setinst(synth,chan,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10)
int chan;
unsigned char synth,v0,v1,v2,v3,v4,v5,v6,v7,v8,v9,v10;
{
	int offs;

	outdata(synth,(unsigned char)(0xa0+chan),(unsigned char)0);
	outdata(synth,(unsigned char)(0xb0+chan),(unsigned char)0);
	outdata(synth,(unsigned char)(0xc0+chan),v10);
	if (chan == 0)
		offs = 0;
	if (chan == 1)
		offs = 1;
	if (chan == 2)
		offs = 2;
	if (chan == 3)
		offs = 8;
	if (chan == 4)
		offs = 9;
	if (chan == 5)
		offs = 10;
	if (chan == 6)
		offs = 16;
	if (chan == 7)
		offs = 17;
	if (chan == 8)
		offs = 18;
	outdata(synth,(unsigned char)(0x20+offs),v5);
	outdata(synth,(unsigned char)(0x40+offs),v6);
	outdata(synth,(unsigned char)(0x60+offs),v7);
	outdata(synth,(unsigned char)(0x80+offs),v8);
	outdata(synth,(unsigned char)(0xe0+offs),v9);
	offs+=3;
	outdata(synth,(unsigned char)(0x20+offs),v0);
	outdata(synth,(unsigned char)(0x40+offs),v1);
	outdata(synth,(unsigned char)(0x60+offs),v2);
	outdata(synth,(unsigned char)(0x80+offs),v3);
	outdata(synth,(unsigned char)(0xe0+offs),v4);
}

setmidiinsts()
{
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,0xc0);
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,midiinst);
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,0xc1);
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,midiinst);
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,0xc2);
	midiscrap = 256;
	while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
		midiscrap--;
	if (midiscrap > 0)
		outp(dataport,14);
}

checkhitwall(oposx,oposy,posix,posiy)
unsigned oposx,oposy,posix,posiy;
{
	int i, j, k, m, xdir, ydir, cntx, cnty, xpos, ypos, xinc, yinc;
	unsigned x1, y1, x2, y2, x3, y3, xspan, yspan;
	long templong;

	if (oposx < posix)
	{
		x1 = (oposx>>10);
		xdir = 1;
		xinc = posix-oposx;
	}
	else
	{
		x1 = (posix>>10);
		xdir = -1;
		xinc = oposx-posix;
	}
	if (oposy < posiy)
	{
		y1 = (oposy>>10);
		ydir = 1;
		yinc = posiy-oposy;
	}
	else
	{
		y1 = (posiy>>10);
		ydir = -1;
		yinc = oposy-posiy;
	}
	xspan = abs(((int)(posix>>10))-((int)(oposx>>10)))+1;
	yspan = abs(((int)(posiy>>10))-((int)(oposy>>10)))+1;
	xpos = oposx-(x1<<10);
	ypos = oposy-(y1<<10);
	for(i=0;i<=(xspan<<2);i++)
	{
		x2 = ((i+3)>>2)+x1-1;
		x3 = ((i+5)>>2)+x1-1;
		if (((x2|x3)&0xffc0) == 0)
			for(j=0;j<=(yspan<<2);j++)
			{
				y2 = ((j+3)>>2)+y1-1;
				y3 = ((j+5)>>2)+y1-1;
				k = i+(j<<6);
				tempbuf[k] = 0;
				if (((y2|y3)&0xffc0) == 0)
				{
					m = board[x2][y2];
					if (((m&3072) != 1024) && ((m&1023) != 0)) tempbuf[k] = 1;
					m = board[x2][y3];
					if (((m&3072) != 1024) && ((m&1023) != 0)) tempbuf[k] = 1;
					m = board[x3][y2];
					if (((m&3072) != 1024) && ((m&1023) != 0)) tempbuf[k] = 1;
					m = board[x3][y3];
					if (((m&3072) != 1024) && ((m&1023) != 0)) tempbuf[k] = 1;
				}
				else
					tempbuf[k] = 1;
			}
	}
	cntx = 0, cnty = 0;
	for(i=0;i<256;i++)
	{
		_asm \
		{
			mov dx, xinc
			add cntx, dx
			mov cx, cntx
			mov byte ptr cntx[1], 0
			mov cl, ch
			xor ch, ch
			jcxz endcntx
			mov dx, xdir
			mov si, ypos
			and si, 0xff00
			shr si, 1
			shr si, 1
begincntx:
			mov bx, xpos
			add bx, dx
			mov bl, bh
			xor bh, bh
			cmp byte ptr tempbuf[bx+si], 0
			jne skipcntx
			add xpos, dx
skipcntx:
			loop begincntx
endcntx:
			mov dx, yinc
			add cnty, dx
			mov cx, cnty
			mov byte ptr cnty[1], 0
			mov cl, ch
			xor ch, ch
			jcxz endcnty
			mov dx, ydir
			mov bx, xpos
			mov bl, bh
			xor bh, bh
begincnty:
			mov si, ypos
			add si, dx
			and si, 0xff00
			shr si, 1
			shr si, 1
			cmp byte ptr tempbuf[bx+si], 0
			jne skipcnty
			add ypos, dx
skipcnty:
			loop begincnty
endcnty:
		}
	}
	posx = xpos + (((unsigned)x1)<<10);
	posy = ypos + (((unsigned)y1)<<10);
	if (((posx&0xfc00) != (oposx&0xfc00)) || ((posy&0xfc00) != (oposy&0xfc00)))
	{
		if ((board[posx>>10][posy>>10]&1023) == stairs)
		{
			if ((boardnum >= 10) && (boardnum < 20))
			{
				j = -1;
				for(i=0;i<mnum;i++)
					if (mstat[i] == mondog)
					{
						if (mposx[i] > posx) templong = (long)(mposx[i]-posx);
						else templong = (long)(posx-mposx[i]);
						if (mposy[i] > posy) templong += (long)(mposy[i]-posy);
						else templong += (long)(posy-mposy[i]);
						j = 0;
						if (templong >= 4096)
							j = 1;
					}
				if (j != 0)
				{
					posx = oposx;
					posy = oposy;
					strcpy(&textbuf[0],"Where's your dog?");
					textprint(112,40,(char)96,lastpageoffset+90);
				}
			}
		}
		youarehere();
	}
}

/*reviewboard()
{
	int i, angdir, fadeness;
	unsigned temposx, temposy, temang;
	long revtotalclock, revototclock;

	revototclock = -1;
	revtotalclock = 0;
	fadeness = 63;
	while ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0))
	{
		bstatus = 0;
		if (moustat == 0)
			_asm \
			{
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
		if (joystat == 0)
			_asm
			{
				mov dx, 0x201
				in al, dx
				and ax, 0x30
				xor al, 0x30
				or bstatus, ax
			}
		if (fadeness == 63)
		{
			if ((revototclock == -1) || ((revototclock|1023) != (revtotalclock|1023)))
			{
				do
				{
					temposx = (((unsigned)rand())<<1);
					temposy = (((unsigned)rand())<<1);
					i = 0;
					if ((board[(temposx-128)>>10][(temposy-128)>>10]&1023) != 0)
						i = 1;
					if ((board[(temposx+128)>>10][(temposy-128)>>10]&1023) != 0)
						i = 1;
					if ((board[(temposx-128)>>10][(temposy+128)>>10]&1023) != 0)
						i = 1;
					if ((board[(temposx+128)>>10][(temposy+128)>>10]&1023) != 0)
						i = 1;
					if ((board[temposx>>10][temposy>>10]&1023) != 0)
						i = 1;
				}
				while (i == 1);
				temang = (rand()&2047);
				if (compass > 0)
					showcompass(temang);
				posz = (rand()&31)+16;
				angdir = (rand()&1);
				fadeness = 62;
			}
			picrot(temposx,temposy,posz,temang);
			if (revtotalclock < 480)
			{
				for(i=lside;i<rside;i++)
					height[i] = 0;
				spridraw((int)180-64,(int)halfheight-64,(int)128*4,(int)congratsign);
			}
			sortcnt = 0;
		}
		if (fadeness < 63)
		{
			if ((fadeness&1) == 0)
				fadeness -= 4;
			if ((fadeness&1) == 1)
				fadeness += 4;
			if (fadeness == 2)
			{
				picrot(temposx,temposy,posz,temang);
				sortcnt = 0;
				fadeness = 3;
			}
			fade(fadeness);
		}
		if ((fadeness == 63) || (fadeness == 3))
		{
			lastpageoffset = pageoffset;
			if (vidmode == 0)
			{
				_asm \
				{
					mov bx, lastpageoffset
					add bx, 5
					mov al, 0xc
					mov ah, bh
					mov cl, 0xd
					mov ch, bl
					mov dx, 0x3d4
					out dx, al
					inc dx
					mov al, ah
					out dx, al
					mov ax, cx
					dec dx
					out dx, al
					inc dx
					mov al, ah
					out dx, al
				}
				if (lastpageoffset < 21312) pageoffset = 21312;
				if (lastpageoffset == 21312) pageoffset = 39744;
				if (lastpageoffset > 21312) pageoffset = 2880;
			}
			else
			{
				_asm \
				{
					mov bx, lastpageoffset
					mov al, 0xc
					mov ah, bh
					mov cl, 0xd
					mov ch, bl
					mov dx, 0x3d4
					out dx, al
					inc dx
					mov al, ah
					out dx, al
					mov ax, cx
					dec dx
					out dx, al
					inc dx
					mov al, ah
					out dx, al
				}
				if (statusbar == 415)
				{
					if (lastpageoffset < 21824) pageoffset = 21824;
					if (lastpageoffset == 21824) pageoffset = 40768;
					if (lastpageoffset > 21824) pageoffset = 2880;
				}
				else
				{
					if (lastpageoffset < 21600) pageoffset = 21600;
					if (lastpageoffset == 21600) pageoffset = 43200;
					if (lastpageoffset > 21600) pageoffset = 0;
				}
			}
		}
		if (fadeness == 63)
		{
			oldposx = temposx;
			oldposy = temposy;
			temposx += (int)(((long)100*clockspeed*sintable[(temang+512)&2047])>>19);
			temposy += (int)(((long)100*clockspeed*sintable[temang])>>19);
			i = checkhitwall(oldposx,oldposy,temposx,temposy,temang);
			if ((i&1) > 0)
				temposx = (oldposx&0xfc00)+128;
			if ((i&2) > 0)
				temposx = (oldposx&0xfc00)+895;
			if ((i&4) > 0)
				temposy = (oldposy&0xfc00)+128;
			if ((i&8) > 0)
				temposy = (oldposy&0xfc00)+895;
			if ((i&16) > 0)
				temposx = ((oldposx-128)&0xfc00)+128;
			if ((i&32) > 0)
				temposx = ((oldposx+128)&0xfc00)+895;
			if ((i&64) > 0)
				temposy = ((oldposy-128)&0xfc00)+128;
			if ((i&128) > 0)
				temposy = ((oldposy+128)&0xfc00)+895;
			if (i > 0)
			{
				if (angdir == 0)
					temang = (temang+(clockspeed<<2))&2047;
				else
					temang = (temang+2048-(clockspeed<<2))&2047;
				if (compass > 0)
					showcompass(temang);
			}
		}
		if (fadeness == 59)
			fadeness = 63;
		revototclock = revtotalclock;
		revtotalclock += clockspeed;
		clockspeed = 0;
	}
	posz = 32;
	lastunlock = 1;
	lastshoot = 1;
	lastbarchange = 1;
}*/

wingame(episode)
int episode;
{
	unsigned l, startx, starty;
	int i, j, brightness, oldmute;
	long revtotalclock, revototclock, templong;

	oldmute = mute;
	revototclock = -1;
	revtotalclock = 0;
	musicoff();
	if (episode == 1) loadmusic("WINGAME0");
	if (episode == 2) loadmusic("WINGAME1");
	musicon();
	posz = 32;
	if (episode == 1)
	{
		startx = (((unsigned)58)<<10);
		starty = (((unsigned)19)<<10);
	}
	else
	{
		startx = (((unsigned)5)<<10);
		starty = (((unsigned)12)<<10);
	}
	while ((death != 4096) || ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0)))
	{
		bstatus = 0;
		if (moustat == 0)
			_asm \
			{
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
		if (joystat == 0)
			_asm
			{
				mov dx, 0x201
				in al, dx
				and ax, 0x30
				xor al, 0x30
				or bstatus, ax
			}
		if (death != 4096)
		{
			ang = 512;
			posx = (startx+512);
			if (revtotalclock < 3840)
				posy = starty+512+(unsigned)((revtotalclock<<6)/15);
			else
				posy = starty+16384+512;
			picrot(posx,posy,posz,ang);
			sortcnt = 0;
			if ((musicstatus == 1) && (clockspeed >= 0) && (clockspeed < 3))
				_asm \
				{
limitframerate2:
					cmp clockspeed, 3
					jb limitframerate2
				}
			lastpageoffset = pageoffset;
			if (vidmode == 0)
			{
				_asm \
				{
					mov bx, lastpageoffset
					add bx, 5
					mov al, 0xc
					mov ah, bh
					mov cl, 0xd
					mov ch, bl
					mov dx, 0x3d4
					out dx, al
					inc dx
					mov al, ah
					out dx, al
					mov ax, cx
					dec dx
					out dx, al
					inc dx
					mov al, ah
					out dx, al
				}
				if (lastpageoffset < 21312) pageoffset = 21312;
				if (lastpageoffset == 21312) pageoffset = 39744;
				if (lastpageoffset > 21312) pageoffset = 2880;
			}
			else
			{
				_asm \
				{
					mov bx, lastpageoffset
					mov al, 0xc
					mov ah, bh
					mov cl, 0xd
					mov ch, bl
					mov dx, 0x3d4
					out dx, al
					inc dx
					mov al, ah
					out dx, al
					mov ax, cx
					dec dx
					out dx, al
					inc dx
					mov al, ah
					out dx, al
				}
				if (statusbar == 415)
				{
					if (lastpageoffset < 21824) pageoffset = 21824;
					if (lastpageoffset == 21824) pageoffset = 40768;
					if (lastpageoffset > 21824) pageoffset = 2880;
				}
				else
				{
					if (lastpageoffset < 21600) pageoffset = 21600;
					if (lastpageoffset == 21600) pageoffset = 43200;
					if (lastpageoffset > 21600) pageoffset = 0;
				}
			}
		}
		if (episode == 2)
		{
			if ((revtotalclock >= 840) && (revototclock < 840))
			{
				life += 1280;
				if (life > 4095)
					life = 4095;
				drawlife();
				ksay(7);
			}
			if ((revtotalclock >= 1560) && (revototclock < 1560))
			{
				life += 1280;
				if (life > 4095)
					life = 4095;
				drawlife();
				ksay(7);
			}
		}
		if (revtotalclock >= 3840)
		{
			if (revototclock < 3840)
			{
				death = 4094;
				if (episode == 1) ksay(6);
				if (episode == 2) ksay(9);
				mute = 1;
			}
			if (death < 4095)
			{
				if (death > 0)
				{
					posz+=2;
					if (posz > 64)
						posz = 64;
					death -= (((((int)(revtotalclock-revototclock))<<5)&0xff80)+128);
					if (death <= 0)
					{
						death = 0;
						posz = 32;
					}
					fade(death>>6);
				}
				if ((death == 0) && (revtotalclock >= 4800))
				{
					_disable();
					_asm \
					{
						mov es, note
						mov ax, word ptr es:[0]
						mov dx, word ptr es:[2]
						mov word ptr templong[0], ax
						mov word ptr templong[2], dx
					}
					count = (templong>>12)-1;
					countstop = (templong>>12)-1;
					nownote = 0;
					mute = oldmute;
					_enable();
					death = 4096;
					if ((vidmode == 0) && (statusbar < 399))
					{
						l = times90[((unsigned)statusbar+1)>>1]+5;
						copywindow(0,lastpageoffset+l,18000-l);
						linecompare(399);
					}
					if ((vidmode == 1) && (statusbar < 479))
					{
						l = times90[((unsigned)statusbar+1)>>1];
						copywindow(0,lastpageoffset+l,21600-l);
						linecompare(479);
					}
					drawmenu(304,192,menu);
					if (episode == 1) loadstory(-21);
					if (episode == 2) loadstory(-19);
					fade(27);
					_asm \
					{
						mov dx, 0x3c8
						mov al, 240
						out dx, al
						inc dx
						mov cx, 16
						mov al, 3
wingamepalette:
						out dx, al
						out dx, al
						out dx, al
						add al, 4
						loop wingamepalette
					}
					ksay(23);
					pressakey();
					drawmenu(304,192,menu);
					keystatus[1] = 0;
					keystatus[57] = 0;
					keystatus[28] = 0;
					if (episode == 1) loadstory(-20);
					if (episode == 2) loadstory(-18);
					pressakey();
				}
			}
		}
		revototclock = revtotalclock;
		revtotalclock += clockspeed;
		totalclock += clockspeed;
		animate2 = animate2 ^ 1;
		clockspeed = 0;
	}
	musicoff();
	fade(0);
	ototclock = -1;
	totalclock = 1;
	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
	linecompare(statusbar);
	fade(63);
	fadewarpval = 63;
	fadehurtval = 0;
	keystatus[57] = 0;
	keystatus[28] = 0;
	keystatus[1] = 0;
	bstatus = 0;
	lastunlock = 1;
	lastshoot = 1;
	lastbarchange = 1;
	death = 4095;
}

winallgame()
{
	unsigned l;
	int i, j, oldmute, leavewin;
	long revtotalclock, revototclock;

	revototclock = 1;
	revtotalclock = 1;
	lastpageoffset = 0;
	pageoffset = 0;
	fade(0);
	linecompare(479);
	musicoff();
	loadmusic("WINGAME2");
	musicon();
	outp(0x3d4,0xc); outp(0x3d5,0);
	outp(0x3d4,0xd);
	if (vidmode == 0)
		outp(0x3d5,5);
	else
		outp(0x3d5,0);
	outp(0x3c4,2); outp(0x3c5,15);
	_asm \
	{
		cld
		mov ax, 0xa000
		mov es, ax
		xor di, di
		mov cx, 21600
		xor al, al
		rep stosb
	}
	fade(63);
	leavewin = 0;
	bstatus = 0;
	keystatus[1] = 0;
	keystatus[57] = 0;
	keystatus[28] = 0;
	while ((leavewin == 0) || ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0)))
	{
		bstatus = 0;
		if (moustat == 0)
			_asm \
			{
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
		if (joystat == 0)
			_asm
			{
				mov dx, 0x201
				in al, dx
				and ax, 0x30
				xor al, 0x30
				or bstatus, ax
			}
		if (revtotalclock < 3260)
		{
			for(i=lside;i<rside;i++)
				height[i] = 0;
			pictur(180,halfheight,4+(((int)revtotalclock)>>2),((int)(revtotalclock<<2))&2047^2047,earth);
		}
		else
		{
			fade(0);
			drawmenu(304,192,menu);
			loadstory(-17);
			fade(27);
			_asm \
			{
				mov dx, 0x3c8
				mov al, 240
				out dx, al
				inc dx
				mov cx, 16
				mov al, 3
wingamepalette2:
				out dx, al
				out dx, al
				out dx, al
				add al, 4
				loop wingamepalette2
			}
			ksay(23);
			pressakey();
			drawmenu(304,192,menu);
			keystatus[1] = 0;
			keystatus[57] = 0;
			keystatus[28] = 0;
			loadstory(-16);
			pressakey();
			leavewin = 1;
		}
		revototclock = revtotalclock;
		revtotalclock += clockspeed;
		totalclock += clockspeed;
		clockspeed = 0;
	}
	musicoff();
	fade(0);
	ototclock = -1;
	totalclock = 1;
	if (vms == 1)
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
	linecompare(statusbar);
	fade(63);
	fadewarpval = 63;
	fadehurtval = 0;
	keystatus[57] = 0;
	keystatus[28] = 0;
	keystatus[1] = 0;
	bstatus = 0;
	lastunlock = 1;
	lastshoot = 1;
	lastbarchange = 1;
	death = 4095;
}

fade(brightness)
int brightness;
{
	if (brightness == 63)
		_asm \
		{
			mov dx, 0x3c8
			xor al, al
			out dx, al
			inc dx
			xor bx, bx
fixpalette:
			mov al, palette[bx]
			out dx, al
			inc bx
			cmp bx, 768
			jb fixpalette
		}
	else if (brightness < 64)
		_asm \
		{
			mov dx, 0x3c8
			xor al, al
			out dx, al
			inc dx
			xor bx, bx
			mov ch, byte ptr brightness
startfade:
			mov al, palette[bx]
			mul ch
			mov cl, 6
			shr ax, cl
			out dx, al
			inc bx
			cmp bx, 720
			jb startfade
		}
	else if (brightness < 128)
		_asm \
		{
			mov dx, 0x3c8
			xor al, al
			out dx, al
			inc dx
			xor bx, bx
			mov ch, byte ptr brightness
			sub ch, 64
startfade2:
			mov al, palette[bx]
			mul ch
			mov cl, 6
			shr ax, cl
			out dx, al
			inc bx
			cmp bx, 768
			jb startfade2
		}
	else
		_asm \
		{
			mov dx, 0x3c8
			xor al, al
			out dx, al
			inc dx
			xor bx, bx
			mov ch, byte ptr brightness
			sub ch, 65
startfade3:
			mov al, palette[bx+0]
			mul ch
			mov cl, 6
			shr ax, cl
			cmp al, 63
			jbe skipsetfade
			mov al, 63
skipsetfade:
			out dx, al
			mov al, palette[bx+1]
			out dx, al
			mov al, palette[bx+2]
			out dx, al
			add bx, 3
			cmp bx, 768
			jb startfade3
		}
}

emmallocate(needpages)
unsigned needpages;
{
	int snot;

	_asm \
	{
		mov al, 0x67
		mov ah, 0x35
		int 0x21
		cmp word ptr es:[10], 0x4d45
		jnz emmerror1
		cmp word ptr es:[12], 0x584d
		jnz emmerror1
		cmp word ptr es:[14], 0x5858
		jnz emmerror1
		cmp word ptr es:[16], 0x3058
		jnz emmerror1
		mov ah, 0x40
		int 0x67
		or ah, ah
		jnz emmerror1
		mov ah, 0x46
		int 0x67
		or ah, ah
		jnz emmerror1
		cmp al, 0x30
		jb emmerror1
		mov ah, 0x41
		int 0x67
		or ah, ah
		jnz emmerror1
		mov emmpageframe, bx
		mov ah, 0x42
		int 0x67
		or ah, ah
		jnz emmerror1
		or bx, bx
		jz emmerror1
		mov ah, 0x43
		mov bx, needpages
		int 0x67
		or ah, ah
		jnz emmerror1
		mov snot, dx
		jmp doneemm1
emmerror1:
		mov snot, -1
doneemm1:
	}
	return(snot);
}

emmdeallocate(emmhandlenum)
int emmhandlenum;
{
	int snot;

	_asm \
	{
		mov snot, 0
		mov dx, emmhandlenum
		mov ah, 0x45
		int 0x67
		or ah, ah
		jnz emmerror2
		jmp doneemm2
emmerror2:
		mov snot, -1
doneemm2:
	}
	return(snot);
}

emsetpages(logpage,physpage)
unsigned logpage, physpage;
{
	int snot;

	if ((logpage == oldlogpage) && (physpage == oldphyspage))
		return(0);
	oldlogpage = logpage;
	oldphyspage = physpage;
	_asm \
	{
		mov snot, 0
		mov bx, logpage
		mov al, byte ptr physpage
		mov dx, emmhandle
		mov ah, 0x44
		int 0x67
		or ah, ah
		jz doneemm3
		mov snot, -1
doneemm3:
	}
	return(snot);
}

convallocate(numparagraphs)
unsigned numparagraphs;
{
	unsigned segnum;

	_asm \
	{
		mov ah, 0x48
		mov bx, numparagraphs
		int 0x21
		mov segnum, ax
		jnc convallocated
		mov segnum, -1
		mov convavailpages, bx
convallocated:
	}
	return(segnum);
}

convdeallocate(segnum)
unsigned segnum;
{
	_asm \
	{
		mov ah, 0x49
		mov es, segnum
		int 0x21
		mov segnum, 0
		jnc convdeallocated
		mov segnum, -1
convdeallocated:
	}
	return(segnum);
}

xmsallocate(kilonum)
int kilonum;
{
	int snot;

	_asm \
	{
		mov ax, 0x4300
		int 0x2f
		cmp al, 0x80
		jne noxms
		mov ax, 0x4310
		int 0x2f
		mov dx, es
		mov ax, seg xmsvector
		mov es, ax
		mov di, offset xmsvector
		mov es:[di], bx
		mov es:[di+2], dx
		mov ah, 0
		call ds:[xmsvector]
		cmp ah, 2
		jb noxms
		mov ah, 9
		mov dx, kilonum
		call ds:[xmsvector]
		or ax, ax
		jz noxms
		mov snot, dx
		jmp endxmsallocate
noxms:
		mov snot, -1
endxmsallocate:
	}
	return(snot);
}

xmsdeallocate(xmshandlenum)
int xmshandlenum;
{
	_asm \
	{
		mov ah, 10
		mov dx, xmshandlenum
		call ds:[xmsvector]
	}
}

xmsmemmove(sourhand, souroff, desthand, destoff, numbytes)
int sourhand, desthand;
long souroff, destoff, numbytes;
{
	int snot;

	if ((sourhand == oldsourhand) && (souroff == oldsouroff) && (sourhand != 0))
		return(0);
	oldsourhand = sourhand;
	oldsouroff = souroff;
	xmstruct[0] = (int)(numbytes&65535);
	xmstruct[1] = (int)(numbytes>>16);
	xmstruct[2] = (int)sourhand;
	xmstruct[3] = (int)(souroff&65535);
	xmstruct[4] = (int)(souroff>>16);
	xmstruct[5] = (int)desthand;
	xmstruct[6] = (int)(destoff&65535);
	xmstruct[7] = (int)(destoff>>16);
	snot = 0;
	_asm \
	{
		push es
		mov ax, seg xmsvector
		mov es, ax
		mov ax, seg xmstruct
		mov si, offset xmstruct
		push ds
		mov ds, ax
		mov ah, 11
		call es:[xmsvector]
		pop ds
		pop es
		cmp ax, 0
		jne donexmsmemcopy
		mov snot, -1
donexmsmemcopy:
	}
	return(snot);
}

showcompass(compang)
int compang;
{
	int i;

	i = (((compang+64)&2047)>>7);
	statusbardraw((i&2)<<4,((i&1)<<5)+2,29,29,238,1,(i>>2)+compassplc,0);
}

kgif(filenum)
int filenum;
{
	unsigned char header[13], imagestat[10], bitcnt, numbits;
	unsigned char globalflag, localflag, backg, chunkind;
	int i, j, k, x, y, xdim, ydim;
	int currstr, bytecnt, numbitgoal;
	int numcols, numpals, dat, fil, blocklen, firstring;
	unsigned int rowpos, numlines, gifdatacnt;

	if ((fil = open("lab3d.kzp",O_BINARY|O_RDONLY,S_IREAD)) == -1)
		return(-1);
	rowpos = 0;
	if (filenum == 1)
	{
		lseek(fil,giflen1,SEEK_SET);
		rowpos = 5;
	}
	if (filenum == 2)
		lseek(fil,giflen1+giflen2,SEEK_SET);
	gifdatacnt = 0;
	read(fil,&tempbuf[0],(unsigned)gifbuflen);
	for(j=0;j<13;j++)
		header[j] = tempbuf[j+gifdatacnt];
	gifdatacnt += 13;
	if ((header[0] != 'L') || (header[1] != 'A') || (header[2] != 'B'))
		return(-1);
	if ((header[3] != '3') || (header[4] != 'D') || (header[5] != '!'))
		return(-1);
	globalflag = header[10];
	numcols = (1<<((globalflag&7)+1));
	firstring = numcols+2;
	backg = header[11];
	if (header[12] != 0)
		return(-1);
	if ((globalflag&128) > 0)
	{
		numpals = numcols+numcols+numcols;
		for(j=0;j<numpals;j++)
			_asm \
			{
				mov di, gifdatacnt
				mov bx, j
				mov al, tempbuf[bx+di]
				mov byte ptr palette[bx], al
			}
		gifdatacnt += numpals;
	}
	chunkind = tempbuf[gifdatacnt], gifdatacnt++;
	while (chunkind == '!')
	{
		gifdatacnt++;
		do
		{
			chunkind = tempbuf[gifdatacnt], gifdatacnt++;
			if (chunkind > 0)
				gifdatacnt += chunkind;
		}
		while (chunkind > 0);
		chunkind = tempbuf[gifdatacnt], gifdatacnt++;
	}
	if (chunkind == ',')
	{
		for(j=0;j<9;j++)
			imagestat[j] = tempbuf[j+gifdatacnt];
		gifdatacnt += 9;
		xdim = imagestat[4]+(imagestat[5]<<8);
		ydim = imagestat[6]+(imagestat[7]<<8);
		localflag = imagestat[8];
		if ((localflag&128) > 0)
		{
			numpals = numcols+numcols+numcols;
			for(j=0;j<numpals;j++)
				_asm \
				{
					mov di, gifdatacnt
					mov bx, j
					mov al, tempbuf[bx+di]
					mov byte ptr palette[bx], al
				}
			gifdatacnt += numpals;
		}
		gifdatacnt++;
		numlines = 200;
		_asm \
		{
			mov ax, 0x13
			int 0x10
		}
		outp(0x3c4,0x4); outp(0x3c5,inp(0x3c5)&(255-8));    //Chain 4 off
		outp(0x3ce,0x5); outp(0x3cf,64);                    //Tim's suggestion
		outp(0x3d4,0x17); outp(0x3d5,inp(0x3d5)|64);        //Word Mode
		outp(0x3d4,0x14); outp(0x3d5,inp(0x3d5)&(255-64));  //Double Word Mode
		outp(0x3d4,0x13); outp(0x3d5,45);
		outp(0x3c8,0);
		for(i=0;i<numpals;i++)
			_asm \
			{
				mov bx, i
				shr byte ptr palette[bx], 1
				shr byte ptr palette[bx], 1
				mov dx, 0x3c9
				xor al, al
				out dx, al
			}
		x = 0, y = 0;
		bitcnt = 0;
		_asm \
		{
			mov es, lzwbuf
			mov cx, numcols
			mov ax, 0
setuplzwbuf2:
			inc ax
			mov di, ax
			mov byte ptr es:[di+8200], al
			mov bx, ax
			mov word ptr es:[di+bx], ax
			loop setuplzwbuf2
			mov word ptr es:[0], 0
			mov byte ptr es:[8200], 0
		}
		currstr = firstring;
		numbits = (globalflag&7)+2;
		numbitgoal = (numcols<<1);
		blocklen = 0;
		blocklen = tempbuf[gifdatacnt], gifdatacnt++;
		for(j=0;j<blocklen;j++)
			_asm \
			{
				mov di, gifdatacnt
				mov bx, j
				mov al, tempbuf[bx+di]
				mov byte ptr lincalc[bx], al
			}
		gifdatacnt += blocklen;
		bytecnt = 0;
		_asm \
		{
			mov dx, 0x3c4
			mov al, 2
			out dx, al
		}
		while (y < ydim)
		{
			_asm \
			{
				mov es, lzwbuf
				mov dl, bitcnt
				mov dh, numbits
				mov si, bytecnt
				mov di, word ptr lincalc[si]
				mov cl, dl
				shr di, cl
				mov cl, dh
				mov bx, 1
				shl bx, cl
				dec bx
				and di, bx
				add cl, dl
				cmp cl, 16
				jbe skipextrabits
				mov al, byte ptr lincalc[si+2]
				mov bx, 1
				rol bx, cl
				dec bx
				and ax, bx
				mov cl, dl
				ror ax, cl
				add di, ax
skipextrabits:
				mov dat, di
				add dl, dh
				mov dh, 0
				mov bx, dx
				mov cl, 3
				shr bx, cl
				add bytecnt, bx
				and dl, 7
				mov bitcnt, dl
			}
			if (bytecnt > blocklen-3)
			{
				_asm \
				{
					mov bx, bytecnt
					mov ax, lincalc[bx]
					mov lincalc[0], ax
				}
				i = blocklen-bytecnt;
				_asm \
				{
					mov bx, gifdatacnt
					mov al, byte ptr tempbuf[bx]
					mov ah, 0
					mov blocklen, ax
					inc gifdatacnt
				}
				if (gifdatacnt+blocklen < gifbuflen)
				{
					_asm \
					{
						mov ax, ds
						mov es, ax
						mov di, i
						add di, offset lincalc
						mov si, gifdatacnt
						add si, offset tempbuf
						mov cx, blocklen
						inc cx
						shr cx, 1
						rep movsw
					}
					gifdatacnt += blocklen;
				}
				else
				{
					k = gifbuflen-gifdatacnt;
					_asm \
					{
						mov ax, ds
						mov es, ax
						mov di, i
						add di, offset lincalc
						mov si, gifdatacnt
						add si, offset tempbuf
						mov cx, k
						inc cx
						shr cx, 1
						rep movsw
					}
					read(fil,&tempbuf[0],(unsigned)gifbuflen);
					_asm \
					{
						mov cx, k
						mov di, i
						add di, offset lincalc
						add di, cx
						mov si, offset tempbuf
						neg cx
						add cx, blocklen
						mov gifdatacnt, cx
						inc cx
						shr cx, 1
						rep movsw
					}
				}
				bytecnt = 0;
				blocklen += i;
			}
			if (currstr == numbitgoal)
				if (numbits < 12)
				{
					numbits++;
					numbitgoal <<= 1;
				}
			if (dat == numcols)
			{
				currstr = firstring;
				numbits = (globalflag&7)+2;
				numbitgoal = (numcols<<1);
			}
			else
			{
				_asm \
				{
					mov es, lzwbuf
					mov cx, 1
					mov bx, dat
					mov di, currstr
					shl di, 1
					mov word ptr es:[di], bx
					mov dx, firstring
					cmp bx, dx
					jb endsearchtable
beginsearchtable:
					inc cx
					mov al, byte ptr es:[bx+8200]
					push ax
					shl bx, 1
					mov bx, word ptr es:[bx]
					cmp bx, dx
					jae beginsearchtable
endsearchtable:
					mov ax, bx
					mov ah, al
					shr di, 1
					mov word ptr es:[di+8199], ax
					shl bx, 1
					mov bx, word ptr es:[bx]
					push bx
					mov si, x
					mov di, si
					mov ax, 0xa000
					mov es, ax
					mov ax, cx
					mov bl, 0x11
					mov cx, di
					and cx, 3
					shl bl, cl
					shr di, 1
					shr di, 1
					add di, rowpos
					mov cx, ax
					mov dx, 0x3c5
begindrawpixel:
					mov al, bl
					out dx, al
					pop ax
					mov es:[di], al
					rol bl, 1
					cmp bl, 0x11
					jne skipincthedi
					inc di
skipincthedi:
					inc si
					cmp si, 320
					jae nextrow
					loop begindrawpixel
					jmp endrawpixel
nextrow:
					sub si, 320
					inc y
					add di, 10
					add rowpos, 90
					loop begindrawpixel
endrawpixel:
					mov x, si
					inc currstr
				}
			}
		}
	}
	close(fil);
}

setgamevideomode()
{
	_asm \
	{
		mov ax, 0x13
		int 0x10
	}
	if (vidmode == 1)
		setmode360240();
	fade(0);
	outp(0x3d4,0x13); outp(0x3d5,45);                   //Bytes per Line
	outp(0x3c4,0x4); outp(0x3c5,inp(0x3c5)&(255-8));    //Chain 4 off
	outp(0x3ce,0x5); outp(0x3cf,64);                    //Tim's suggestion
	outp(0x3d4,0x17); outp(0x3d5,inp(0x3d5)|64);        //Word Mode
	outp(0x3d4,0x14); outp(0x3d5,inp(0x3d5)&(255-64));  //Double Word Mode
	inp(0x3da); outp(0x3c0,0x30); outp(0x3c0,0x71);  //Fix for Line Compare
	fade(63);
	_asm \
	{
		mov dx, 0x3c4                 ;Check for an 8-bit VGA card
		mov ax, 0x0f02
		out dx, al
		inc dx
		mov al, ah
		out dx, al
		cld
		mov ax, 0xa000
		mov es, ax
		mov al, 0x01
		mov byte ptr es:[0], al
		mov byte ptr es:[1], al
		xor ax, ax
		mov word ptr es:[0], ax
		mov dx, 0x3ce
		mov ax, 0x0104
		out dx, al
		inc dx
		mov al, ah
		out dx, al
		mov al, byte ptr es:[1]
		mov videotype, al
	}
}

textprint(x,y,coloffs,pageoffs)
int x, y;
char coloffs;
unsigned pageoffs;
{
	unsigned char character;
	int i, charcnt, picplc, owalnum, walnume;
	unsigned int picseg, plc;

	_asm \
	{
		mov ax, 0xa000
		mov es, ax
	}
	if ((vidmode == 1) && (statusbar != 415) && (pageoffs == 0))
		return(0);
	outp(0x3c4,0x2);
	if (y < 240)
		plc = times90[y];
	else
		plc = y*90;
	plc += (x>>2)+pageoffs;
	if ((vidmode == 1) && (pageoffs == 0))
		plc += 5;
	charcnt = 0;
	while ((textbuf[charcnt] != 0) && (charcnt < 40))
	{
		character = (textbuf[charcnt])&127;
		owalnum = walnume;
		walnume = (character>>6)+textwall;
		if ((memtype[walnume-1] != 65535) && ((charcnt == 0) || (owalnum != walnume)))
		{
			if ((vms == 1) && (memtype[walnume-1] == 0))
				loadvms(walnume-1);
			if ((emswalls > 0) && ((memtype[walnume-1]&1024) > 0))
				emsetpages(memtype[walnume-1]-1024,0);
			else if ((xmswalls > 0) && ((memtype[walnume-1]&2048) > 0))
				xmsmemmove(xmshandle,((long)(memtype[walnume-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
			_asm \
			{
				mov ax, 0xa000
				mov es, ax
			}
		}
		picseg = walseg[walnume-1];
		picplc = ((character&7)<<9)+(((character&63)>>3)<<3);
		for(i=0;i<8;i++)
		{
			outp(0x3c5,1<<(x&3));
			_asm \
			{
				mov cx, 8
				mov si, picplc
				mov di, plc
				push ds
				mov ds, picseg
startlett1:
				lodsb
				cmp al, 255
				je skipitlett1
				add al, coloffs
				mov es:[di], al
skipitlett1:
				add di, 90
				loop startlett1
				pop ds
			}
			picplc += 64;
			x++;
			plc += ((x&3) == 0);
		}
		charcnt++;
	}
}

loadstory(boardnume)
int boardnume;
{
	unsigned char xordat, otextcol, textcol;
	unsigned int storyoffs[128];
	int fil, i, textbufcnt, textypos;

	ototclock = totalclock;
	if ((fil = open("story.kzp",O_BINARY|O_RDONLY,S_IREAD)) == -1)
		return(-1);
	if (vms == 1)
	{
		if (memtype[textwall-1] == 0)
			loadvms(textwall-1);
		totalclock++;
		if (memtype[textwall] == 0)
			loadvms(textwall);
	}
	read(fil,&storyoffs[0],256);
	lseek(fil,(long)(storyoffs[boardnume+34]),SEEK_SET);
	read(fil,&tempbuf[0],4096);
	i = 0;
	xordat = 0;
	if (vidmode == 0)
		textypos = 3;
	else
		textypos = 23;
	if ((boardnume >= 0) || ((boardnume >= -21) && (boardnume <= -16)))
		otextcol = 0;
	else
		otextcol = 96;
	textcol = otextcol;
	if ((boardnume >= -14) && (boardnume <= -2))
		textypos += 12;
	textbufcnt = 0;
	while ((tempbuf[i] != 0) && (textbufcnt < 40))
	{
		xordat ^= tempbuf[i];
		if (tempbuf[i] >= 32)
		{
			if ((otextcol == 96) && (textbufcnt == 0))
			{
				textcol = 96;
				if (tempbuf[i] == '!')
					textcol = 16, textbuf[textbufcnt++] = '@';
				else if (tempbuf[i] == '%')
					textcol = 80, textbuf[textbufcnt++] = '@';
				else
					textbuf[textbufcnt++] = tempbuf[i];
			}
			else
				textbuf[textbufcnt++] = tempbuf[i];
		}
		if (tempbuf[i] == 13)
		{
			textbuf[textbufcnt] = 0;
			textprint(180-(textbufcnt<<2),textypos,textcol,lastpageoffset+90);
			textypos += 12;
			textbufcnt = 0;
		}
		i++;
		tempbuf[i] ^= xordat;
	}
	textbuf[textbufcnt] = 0;
	textprint(180-(textbufcnt<<2),textypos,textcol,lastpageoffset+90);
	close(fil);
}

setupmouse()
{
	int snot;

	_asm \
	{
		mov snot, 0
		mov ax, 0x3533
		int 0x21
		mov dx, es
		or dx, bx
		jz nomouse
		cmp es:[bx], 0xcf
		je nomouse
		jmp skipnomouse
		mov ax, 0
		int 0x33
nomouse:
		mov snot, -1
		mov bstatus, 0
skipnomouse:
	}
	return(snot);
}

statusbaralldraw()
{
	int i, j;

	if (vidmode == 1)
	{
		outp(0x3c4,2); outp(0x3c5,15);
		_asm \
		{
			cld
			mov ax, 0xa000
			mov es, ax
			xor di, di
			mov cx, 1440
			xor ax, ax
			cmp vidmode, 0
			je statusbarclear1a
			shl cx, 1
			rep stosb
			jmp statusbarclear1b
statusbarclear1a:
			rep stosw
statusbarclear1b:
		}
	}
	statusbardraw(0,0,32,32,0,0,statusbarback,0);
	statusbardraw(32,0,32,32,288,0,statusbarback,0);
	for(i=32;i<288;i+=32)
		statusbardraw(16,0,32,32,i,0,statusbarback,0);
	textbuf[0] = 'S', textbuf[1] = 'C', textbuf[2] = 'O';
	textbuf[3] = 'R', textbuf[4] = 'E', textbuf[5] = 0;
	textprint(3,4,(char)176,0);
	textbuf[0] = 'T', textbuf[1] = 'I', textbuf[2] = 'M';
	textbuf[3] = 'E', textbuf[4] = 0;
	textprint(3,12,(char)176,0);
	textbuf[0] = 'B', textbuf[1] = 'O', textbuf[2] = 'A';
	textbuf[3] = 'R', textbuf[4] = 'D', textbuf[5] = 0;
	textprint(3,20,(char)176,0);
	textbuf[0] = ':', textbuf[1] = 0;
	textprint(41,4,(char)176,0);
	textprint(33,12,(char)176,0);
	textprint(41,20,(char)176,0);
	textbuf[0] = 'L', textbuf[1] = 'I', textbuf[2] = 'F';
	textbuf[3] = 'E', textbuf[4] = 0;
	textprint(96,4,(char)176,0);
	textbuf[0] = 'W', textbuf[1] = 'E', textbuf[2] = 'A';
	textbuf[3] = 'P', textbuf[4] = 'O', textbuf[5] = 'N';
	textbuf[6] = 0;
	textprint(272,3,(char)176,0);
	if (hiscorenamstat == 1)
	{
		for(i=0;i<9;i++)
			textbuf[i] = hiscorenam[i];
		textbuf[9] = 0;
		textprint(70,20,(char)176,0);
	}
	statusbardraw(50,30,7,7,282,12,statusbarinfo,0);
	statusbardraw(50,37,7,7,282,21,statusbarinfo,0);
	statusbardraw(57,30,7,7,306,12,statusbarinfo,0);
	statusbardraw(57,37,7,7,306,21,statusbarinfo,0);
	statusbardraw(36,37,7,7,104,12,statusbarinfo,0);
	statusbardraw(43,37,7,7,136,12,statusbarinfo,0);
	statusbardraw(32,28,1,8,94,4,statusbarinfo,0);
	statusbardraw(32,28,1,8,94,12,statusbarinfo,0);
	oldlife = 0;
	drawlife();
	oldlife = 4095;
	drawlife();
	drawscore(scorecount);
	drawtime(scoreclock);
	textbuf[0] = ((boardnum+1)/10)+48;
	textbuf[1] = ((boardnum+1)%10)+48;
	textbuf[2] = 0;
	if (textbuf[0] == 48)
		textbuf[0] = 32;
	textprint(46,20,(char)176,0);
	if (vidmode == 1)
	{
		statusbardraw(0,32,20,32,-20,0,statusbarback,0);
		statusbardraw(0,32,20,32,320,0,statusbarback,0);
	}
	textbuf[0] = 9, textbuf[1] = 0;
	textprint(96,12,(char)0,0);
	textbuf[0] = lifevests+48, textbuf[1] = 0;
	textprint(96,12,(char)176,0);
	textbuf[0] = 9, textbuf[1] = 0;
	textprint(296,21,(char)0,0);
	textbuf[0] = lightnings+48, textbuf[1] = 0;
	textprint(296,21,(char)176,0);
	textbuf[0] = 9, textbuf[1] = 0;
	textprint(272,12,(char)0,0);
	textbuf[0] = firepowers[0]+48, textbuf[1] = 0;
	textprint(272,12,(char)176,0);
	textbuf[0] = 9, textbuf[1] = 0;
	textprint(272,21,(char)0,0);
	textbuf[0] = firepowers[1]+48, textbuf[1] = 0;
	textprint(272,21,(char)176,0);
	textbuf[0] = 9, textbuf[1] = 0;
	textprint(296,12,(char)0,0);
	textbuf[0] = firepowers[2]+48, textbuf[1] = 0;
	textprint(296,12,(char)176,0);
	if (purpletime >= totalclock)
	{
		statusbardraw(0,0,16,15,159,13,statusbarinfo,0);
		if (purpletime < totalclock+3840)
		{
			i = ((3840-(int)(purpletime-totalclock))>>8);
			if ((i >= 0) && (i <= 15))
				statusbardraw(0,30,16,i,159,13,statusbarinfo,0);
		}
	}
	if (greentime >= totalclock)
	{
		statusbardraw(0,15,16,15,176,13,statusbarinfo,0);
		if (greentime < totalclock+3840)
		{
			i = ((3840-(int)(greentime-totalclock))>>8);
			if ((i >= 0) && (i <= 15))
				statusbardraw(16,30,16,i,176,13,statusbarinfo,0);
		}
	}
	if (capetime[0] >= totalclock)
	{
		statusbardraw(16,0,21,28,194,2,statusbarinfo,0);
		if (capetime[0] < totalclock+3072)
		{
			i = (int)((capetime[0]-totalclock)>>9);
			if ((i >= 0) && (i <= 5))
			{
				if (i == 5) statusbardraw(0,0,21,28,194,2,coatfade,0);
				if (i == 4) statusbardraw(21,0,21,28,194,2,coatfade,0);
				if (i == 3) statusbardraw(42,0,21,28,194,2,coatfade,0);
				if (i == 2) statusbardraw(0,32,21,28,194,2,coatfade,0);
				if (i == 1) statusbardraw(21,32,21,28,194,2,coatfade,0);
				if (i == 0) statusbardraw(42,32,21,28,194,2,coatfade,0);
			}
		}
	}
	if (capetime[1] >= ototclock)
	{
		statusbardraw(37,0,21,28,216,2,statusbarinfo,0);
		if (capetime[1] < totalclock+3072)
		{
			i = (int)((capetime[1]-totalclock)>>9);
			if ((i >= 0) && (i <= 5))
			{
				if (i == 5) statusbardraw(0,0,21,28,216,2,coatfade,0);
				if (i == 4) statusbardraw(21,0,21,28,216,2,coatfade,0);
				if (i == 3) statusbardraw(42,0,21,28,216,2,coatfade,0);
				if (i == 2) statusbardraw(0,32,21,28,216,2,coatfade,0);
				if (i == 1) statusbardraw(21,32,21,28,216,2,coatfade,0);
				if (i == 0) statusbardraw(42,32,21,28,216,2,coatfade,0);
			}
		}
	}
	if (keys[0] > 0)
		statusbardraw(36,44,14,6,144,13,statusbarinfo,0);
	if (keys[1] > 0)
		statusbardraw(50,44,14,6,144,21,statusbarinfo,0);
	statusbardraw(41,28,8,9,292,20,statusbarinfo,0);
	statusbardraw(43,28,6,9,300,20,statusbarinfo,0);
	statusbardraw(43,28,6,9,306,20,statusbarinfo,0);
	statusbardraw(45,28,5,9,312,20,statusbarinfo,0);
	if (bulchoose == 0) i = 268, j = 11;
	if (bulchoose == 1) i = 268, j = 20;
	if (bulchoose == 2) i = 292, j = 11;
	statusbardraw(32,28,8,9,i,j,statusbarinfo,0);
	statusbardraw(34,28,6,9,i+8,j,statusbarinfo,0);
	statusbardraw(34,28,6,9,i+14,j,statusbarinfo,0);
	statusbardraw(36,28,5,9,i+20,j,statusbarinfo,0);
	textbuf[0] = 9, textbuf[1] = 9;
	textbuf[2] = 9, textbuf[3] = 0;
	textprint(112,12,(char)0,0);
	textbuf[0] = (coins/100)+48;
	textbuf[1] = ((coins/10)%10)+48;
	textbuf[2] = (coins%10)+48;
	textbuf[3] = 0;
	if (textbuf[0] == 48)
	{
		textbuf[0] = 32;
		if (textbuf[1] == 48)
		  textbuf[1] = 32;
	}
	textprint(112,12,(char)176,0);
}

hiscorecheck()
{
	int i, j, k, m, inse, namexist, fil;
	unsigned int oldpageoffs;
	unsigned char ch;
	long hiscore[8], scorexist, templong;

	if ((fil = open("hiscore.dat",O_BINARY|O_RDWR,S_IREAD|S_IWRITE)) == -1)
		return(-1);
	if (vms == 1)
	{
		if (memtype[textwall-1] == 0)
			loadvms(textwall-1);
		totalclock++;
		if (memtype[textwall] == 0)
			loadvms(textwall);
		totalclock++;
		if (memtype[scorebox-1] == 0)
			loadvms(scorebox-1);
	}
	oldpageoffs = pageoffset;
	pageoffset = lastpageoffset;
	lseek(fil,(long)(boardnum<<7),SEEK_SET);
	read(fil,&tempbuf[0],128);
	for(i=0;i<8;i++)
	{
		_asm \
		{
			mov di, i
			mov cl, 4
			shl di, cl
			mov ax, word ptr tempbuf[di+12]
			mov dx, word ptr tempbuf[di+14]
			mov di, i
			shl di, 1
			shl di, 1
			mov hiscore[di], ax
			mov hiscore[di+2], dx
		}
	}
	for(i=lside;i<rside;i++)
		height[i] = 0;
	spridraw(20,0,320*4,scorebox);
	for(i=0;i<8;i++)
		if (tempbuf[i<<4] != 0)
		{
			textbuf[0] = i+49, textbuf[1] = '.', textbuf[2] = 32, textbuf[3] = 32;
			for(j=0;j<12;j++)
				textbuf[j+4] = tempbuf[(i<<4)+j];
			textbuf[16] = 0;
			textprint(55,40+(i<<3)+i,(char)130,lastpageoffset+90);
			setuptextbuf(hiscore[i]);
			textprint(215,40+(i<<3)+i,(char)96,lastpageoffset+90);
		}
	sprintf(&textbuf[0],"Time penalty: 10 * ");
	templong = (scoreclock*5)/12;
	textbuf[19] = (char)((templong/10000000L)%10L)+48;
	textbuf[20] = (char)((templong/1000000L)%10L)+48;
	textbuf[21] = (char)((templong/100000L)%10L)+48;
	textbuf[22] = (char)((templong/10000L)%10L)+48;
	textbuf[23] = (char)((templong/1000L)%10L)+48;
	textbuf[24] = (char)((templong/100L)%10L)+48;
	textbuf[25] = '.';
	textbuf[26] = (char)((templong/10L)%10L)+48;
	textbuf[27] = (char)(templong%10L)+48;
	textbuf[28] = 0;
	j = 19;
	while ((textbuf[j] == 48) && (j < 24))
		j++;
	for(i=19;i<28;i++)
		textbuf[i] = textbuf[i+j-19];
	textprint(180-(strlen(textbuf)<<2),135,(char)114,lastpageoffset+90);
	scorecount -= (scoreclock/24);
	if (scorecount < 0)
		scorecount = 0;
	setuptextbuf(scorecount);
	textprint(215,125,(char)35,lastpageoffset+90);
	if ((scorecount > hiscore[7]) && (cheated == 0))
	{
		if (hiscorenamstat == 0)
			getname();
		if (hiscorenamstat == 0)
		{
			for(i=0;i<22;i++)
				textbuf[i] = 8;
			textbuf[22] = 0;
			textprint(180-(strlen(textbuf)<<2),115,(char)1,lastpageoffset+90);
		}
		else
		{
			m = 0;
			while ((hiscorenam[m] != 0) && (m < 11))
				m++;
			namexist = -1;
			for(i=0;i<8;i++)
			{
				j = 0;
				for(k=0;k<=m;k++)
					if (hiscorenam[k] != tempbuf[(i<<4)+k])
						j = 1;
				if (j == 0)
					namexist = i;
			}
			scorexist = scorecount;
			if (namexist != -1)
			{
				if (hiscore[namexist] > scorexist)
					scorexist = hiscore[namexist];
				for(j=namexist;j<7;j++)
				{
					for(k=0;k<16;k++)
						tempbuf[(j<<4)+k]=tempbuf[((j+1)<<4)+k];
					hiscore[j] = hiscore[j+1];
				}
				inse = 6;
			}
			else
				inse = 7;
			while ((scorexist > hiscore[inse]) && (inse >= 0))
				inse--;
			inse++;
			for(j=7;j>inse;j--)
			{
				for(k=0;k<16;k++)
					tempbuf[(j<<4)+k]=tempbuf[((j-1)<<4)+k];
				hiscore[j] = hiscore[j-1];
			}
			for(k=0;k<12;k++)
				tempbuf[(inse<<4)+k] = hiscorenam[k];
			hiscore[inse] = scorexist;
			_asm \
			{
				mov ax, word ptr scorexist[0]
				mov dx, word ptr scorexist[2]
				mov di, inse
				mov cl, 4
				shl di, cl
				mov tempbuf[di+12], ax
				mov tempbuf[di+14], dx
			}
			spridraw(20,0,320*4,scorebox);
			if (scorecount == scorexist)
			{
				switch(inse)
				{
					case 0: strcpy(&textbuf[0],"Congratulations!!! You're #1!"); break;
					case 1: strcpy(&textbuf[0],"Good Job!! Second place!"); break;
					case 2: strcpy(&textbuf[0],"Not Bad - You got third place!"); break;
					case 3: strcpy(&textbuf[0],"Fourth place! Getting there..."); break;
					case 4: strcpy(&textbuf[0],"Fifth place! Could be improved..."); break;
					case 5: strcpy(&textbuf[0],"Sixth Place...  Keep trying..."); break;
					case 6: strcpy(&textbuf[0],"Seventh? Really want your name here?"); break;
					case 7: strcpy(&textbuf[0],"Eighth? Have a load in your pants?"); break;
				};
			}
			else
				strcpy(&textbuf[0],"Are those reflexes dying?");
			textprint(180-(strlen(textbuf)<<2),25,(char)65,lastpageoffset+90);
			for(i=0;i<8;i++)
				if (tempbuf[i<<4] != 0)
				{
					textbuf[0] = i+49, textbuf[1] = '.', textbuf[2] = 32, textbuf[3] = 32;
					for(j=0;j<12;j++)
						textbuf[j+4] = tempbuf[(i<<4)+j];
					textbuf[16] = 0;
					if (i == inse)
					{
						textprint(55,40+(i<<3)+i,(char)98,lastpageoffset+90);
						for(j=0;j<12;j++)
							textbuf[j] = textbuf[j+4];
						textprint(87,125,(char)98,lastpageoffset+90);
					}
					else
						textprint(55,40+(i<<3)+i,(char)130,lastpageoffset+90);
					setuptextbuf(hiscore[i]);
					textprint(215,40+(i<<3)+i,(char)96,lastpageoffset+90);
				}
			sprintf(&textbuf[0],"Time penalty: 10 * ");
			textbuf[19] = (char)((templong/10000000L)%10L)+48;
			textbuf[20] = (char)((templong/1000000L)%10L)+48;
			textbuf[21] = (char)((templong/100000L)%10L)+48;
			textbuf[22] = (char)((templong/10000L)%10L)+48;
			textbuf[23] = (char)((templong/1000L)%10L)+48;
			textbuf[24] = (char)((templong/100L)%10L)+48;
			textbuf[25] = '.';
			textbuf[26] = (char)((templong/10L)%10L)+48;
			textbuf[27] = (char)(templong%10L)+48;
			textbuf[28] = 0;
			j = 19;
			while ((textbuf[j] == 48) && (j < 24))
				j++;
			for(i=19;i<28;i++)
				textbuf[i] = textbuf[i+j-19];
			textprint(180-(strlen(textbuf)<<2),135,(char)114,lastpageoffset+90);
			setuptextbuf(scorecount);
			textprint(215,125,(char)35,lastpageoffset+90);
			lseek(fil,(long)(boardnum<<7),SEEK_SET);
			write(fil,&tempbuf[0],128);
		}
	}
	else
	{
		i = rand()&3;
		if (cheated == 0)
		{
			switch(i)
			{
				case 0: strcpy(&textbuf[0],"You rot if you can't beat ");
						  j = 0;
						  while (tempbuf[(i<<4)+j] != 0)
						  {
							  textbuf[j+26] = tempbuf[(i<<4)+j];
							  j++;
						  }
						  textbuf[j+26] = 0;
						  break;
				case 1: strcpy(&textbuf[0],"Try this level again if you're good."); break;
				case 2: strcpy(&textbuf[0],"Nice job, but no score like Ken's."); break;
				case 3: strcpy(&textbuf[0],"Try playing for speed next time."); break;
			};
		}
		else
		{
			switch(i)
			{
				case 0: strcpy(&textbuf[0],"Try playing for real next time."); break;
				case 1: strcpy(&textbuf[0],"Don't touch those cheat keys!"); break;
				case 2: strcpy(&textbuf[0],"Cheating doesn't pay."); break;
				case 3: strcpy(&textbuf[0],"You can't cheat a score."); break;
			};
		}
		textprint(180-(strlen(textbuf)<<2),25,(char)161,lastpageoffset+90);
	}
	close(fil);
	sprintf(&textbuf[0],"Press any key to continue.");
	textprint(180-(strlen(textbuf)<<2),115,(char)65,lastpageoffset+90);
	pageoffset = oldpageoffs;
	while ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0));
}

setuptextbuf(templong)
long templong;
{
	int i;

	textbuf[0] = (char)+48;
	textbuf[1] = (char)+48;
	textbuf[2] = (char)+48;
	textbuf[3] = (char)((templong/10000000L)%10L)+48;
	textbuf[4] = (char)((templong/1000000L)%10L)+48;
	textbuf[5] = (char)((templong/100000L)%10L)+48;
	textbuf[6] = (char)((templong/10000L)%10L)+48;
	textbuf[7] = (char)((templong/1000L)%10L)+48;
	textbuf[8] = (char)((templong/100L)%10L)+48;
	textbuf[9] = (char)((templong/10L)%10L)+48;
	textbuf[10] = (char)(templong%10L)+48;
	i = 0;
	while ((textbuf[i] == 48) && (i < 10))
		textbuf[i++] = 32;
	textbuf[11] = 0;
}

getname()
{
	char ch;
	int i, j;

	if (namrememberstat == 0)
	{
		for(j=0;j<16;j++)
			hiscorenam[j] = 0;
		for(j=0;j<12;j++)
			textbuf[j] = 8;
		textbuf[12] = 0;
		textprint(94,125,(char)0,lastpageoffset+90);
		j = 0;
	}
	else
	{
		for(j=0;j<12;j++)
			textbuf[j] = hiscorenam[j];
		textbuf[12] = 0;
		j = 12;
		while ((hiscorenam[j] == 0) && (j > 0))
			j--;
		textprint(94,125,(char)97,lastpageoffset+90);
	}
	ch = 0;
	sprintf(&textbuf[0],"Please type your name!");
	textprint(180-(strlen(textbuf)<<2),115,(char)161,lastpageoffset+90);
	_disable(); _dos_setvect(0x9, oldkeyhandler); _enable();
	while (kbhit() != 0)
		ch = getch();
	ch = 0;
	while ((ch != 13) && (ch != 27))
	{
		while (kbhit() == 0)
		{
			textbuf[0] = 95;
			textbuf[1] = 0;
			textprint(94+(j<<3),125,(char)97,lastpageoffset+90);
			textbuf[0] = 8;
			textbuf[1] = 0;
			textprint(94+(j<<3),125,(char)0,lastpageoffset+90);
		}
		ch = getch();
		if (ch == 0)
		{
			ch = getch();
			if (ch == 83)
			{
				hiscorenam[j] = ch;
				for(j=0;j<16;j++)
					hiscorenam[j] = 0;
				for(j=0;j<12;j++)
					textbuf[j] = 8;
				textbuf[12] = 0;
				textprint(94,125,(char)0,lastpageoffset+90);
				j = 0;
			}
			ch = 0;
		}
		if ((ch == 8) && (j > 0))
		{
			j--, hiscorenam[j] = 0;
			textbuf[0] = ch;
			textbuf[1] = 0;
			textprint(94+(j<<3),125,(char)0,lastpageoffset+90);
		}
		if ((ch >= 32) && (ch <= 127) && (j < 12))
		{
			textbuf[0] = ch;
			textbuf[1] = 0;
			textprint(94+(j<<3),125,(char)97,lastpageoffset+90);
			hiscorenam[j] = ch;
			if ((ch != 32) || (j > 0))
				j++;
		}
	}
	_disable(); _dos_setvect(0x9, keyhandler); _enable();
	for(i=0;i<256;i++)
		keystatus[i] = 0;
	hiscorenam[j] = 0;
	i = j-1;
	while ((hiscorenam[i] == 32) && (i >= 0))
		hiscorenam[i--] = 0;
	if ((hiscorenam[0] == 0) || (ch == 27))
	{
		hiscorenamstat = 0;
		hiscorenam[0] = 0;
	}
	else
	{
		if (hiscorenam[0] == 0)
			hiscorenamstat = 0;
		else
		{
			hiscorenamstat = 1;
			for(i=0;i<9;i++)
				textbuf[i] = hiscorenam[i];
			textbuf[9] = 0;
			textprint(70,20,(char)177,0);
		}
	}
}

drawscore(thescore)
long thescore;
{
	int i;

	for(i=0;i<6;i++)
		textbuf[i] = 9;
	textbuf[6] = 0;
	textprint(46,4,(char)0,0);
	textbuf[0] = (char)((thescore/100000L)%10L)+48;
	textbuf[1] = (char)((thescore/10000L)%10L)+48;
	textbuf[2] = (char)((thescore/1000L)%10L)+48;
	textbuf[3] = (char)((thescore/100L)%10L)+48;
	textbuf[4] = (char)((thescore/10L)%10L)+48;
	textbuf[5] = (char)(thescore%10L)+48;
	textbuf[6] = 0;
	i = 0;
	while ((textbuf[i] == 48) && (i < 5))
		textbuf[i++] = 32;
	textprint(46,4,(char)192,0);
}

drawtime(thetime)
long thetime;
{
	int i;

	for(i=0;i<7;i++)
		textbuf[i] = 9;
	textbuf[7] = 0;
	textprint(38,12,(char)0,0);
	thetime = thetime/240;
	textbuf[0] = (char)((thetime/1000000L)%10L)+48;
	textbuf[1] = (char)((thetime/100000L)%10L)+48;
	textbuf[2] = (char)((thetime/10000L)%10L)+48;
	textbuf[3] = (char)((thetime/1000L)%10L)+48;
	textbuf[4] = (char)((thetime/100L)%10L)+48;
	textbuf[5] = (char)((thetime/10L)%10L)+48;
	textbuf[6] = (char)(thetime%10L)+48;
	textbuf[7] = 0;
	i = 0;
	while ((textbuf[i] == 48) && (i < 6))
		textbuf[i++] = 32;
	textprint(38,12,(char)192,0);
}

screencapture()
{
	char filename[15];
	int i, j, k, fil, bufplc;
	unsigned int plc;

	strcpy(filename,"capturxx.bmp");
	filename[6] = (capturecount/10)+48;
	filename[7] = (capturecount%10)+48;
	capturecount++;
	if ((fil=open(filename,O_BINARY|O_CREAT|O_WRONLY,S_IWRITE))==-1)
	{
		ksay(8);
		capturecount--;
		return(-1);
	}
	tempbuf[0] = 66, tempbuf[1] = 77, tempbuf[2] = 90, tempbuf[3] = 34;
	tempbuf[4] = 0, tempbuf[5] = 0, tempbuf[6] = 0, tempbuf[7] = 0;
	tempbuf[8] = 0, tempbuf[9] = 0, tempbuf[10] = 26, tempbuf[11] = 3;
	tempbuf[12] = 0, tempbuf[13] = 0, tempbuf[14] = 12, tempbuf[15] = 0;
	tempbuf[16] = 0, tempbuf[17] = 0, tempbuf[18] = 64, tempbuf[19] = 1;
	tempbuf[20] = 200, tempbuf[21] = 0, tempbuf[22] = 1, tempbuf[23] = 0;
	tempbuf[24] = 8, tempbuf[25] = 0;
	write(fil,&tempbuf[0],26);
	outp(0x3c7,0);
	for(i=0;i<768;i++)
		tempbuf[i] = (inp(0x3c9)<<2);
	for(i=0;i<768;i+=3)
	{
		j = tempbuf[i];
		tempbuf[i] = tempbuf[i+2];
		tempbuf[i+2] = j;
	}
	if (write(fil,&tempbuf[0],768) < 768)
	{
		close(fil);
		ksay(8);
		capturecount--;
		return(-1);
	}
	k = pageoffset;
	pageoffset = lastpageoffset;
	outp(0x3ce,4);
	_asm \
	{
		mov ax, 0xa000
		mov es, ax
	}
	bufplc = 0;
	for(j=199;j>=0;j--)
	{
		plc = pageoffset+times90[j]+5;
		if (vidmode == 1)
		{
			if (statusbar == 415)
				plc += 360;
			else
				plc += 1800;
		}
		if ((vidmode == 0) && (j >= ((statusbar+1)>>1)))
			plc = times90[j-((statusbar+1)>>1)];
		_asm \
		{
			mov bx, offset tempbuf
			add bx, bufplc
			mov cx, bx
			add cx, 320
			mov di, plc
			mov dx, 0x3cf
			xor ah, ah
capturebegin:
			mov al, ah
			out dx, al
			mov al, byte ptr es:[di]
			mov byte ptr ds:[bx], al
			inc ah
			cmp ah, 4
			jne captureskip
			xor ah, ah
			inc di
captureskip:
			inc bx
			cmp bx, cx
			jl capturebegin
		}
		bufplc += 320;
		if (bufplc >= 3200)
		{
			if (write(fil,&tempbuf[0],3200) < 3200)
			{
				close(fil);
				pageoffset = k;
				ksay(8);
				capturecount--;
				return(-1);
			}
			bufplc = 0;
			outp(0x3ce,4);
			_asm \
			{
				mov ax, 0xa000
				mov es, ax
			}
		}
	}
	close(fil);
	pageoffset = k;
	ksay(7);
}

mainmenu()
{
	unsigned savepageoffset;
	int i, j, k, n, done, oldfade;

	ksay(27);
	fade(63);
	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	done = 0;
	drawmenu(192,128,menu);
	if (sortcnt != -1)
	{
		if ((vidmode == 0) && (statusbar < 399)  && (sortcnt != -1))
		{
			savepageoffset = times90[((unsigned)statusbar+1)>>1]+5;
			copywindow(0,lastpageoffset+savepageoffset,18000-savepageoffset);
			linecompare(399);
		}
		if ((vidmode == 1) && (statusbar < 479) && (sortcnt != -1))
		{
			savepageoffset = times90[((unsigned)statusbar+1)>>1];
			copywindow(0,lastpageoffset+savepageoffset,21600-savepageoffset);
			linecompare(479);
		}
	}
	strcpy(&textbuf[0],"New game");
	textprint(131,47+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"Load game");
	textprint(131,59+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"Save game");
	textprint(131,71+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"Return to game");
	textprint(131,83+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"Help");
	textprint(131,95+n,126,lastpageoffset+90);
	strcpy(&textbuf[0],"Story");
	textprint(131,107+n,126,lastpageoffset+90);
	strcpy(&textbuf[0],"Ordering Info");
	textprint(131,119+n,126,lastpageoffset+90);
	strcpy(&textbuf[0],"Credits");
	textprint(131,131+n,126,lastpageoffset+90);
	strcpy(&textbuf[0],"Exit to DOS");
	textprint(131,143+n,126,lastpageoffset+90);
	if (vidmode == 0)
	{
		if (lastpageoffset < 21312)
			savepageoffset = 39744;
		else
			savepageoffset = 2880;
	}
	else
	{
		if (statusbar == 415)
		{
			if (lastpageoffset < 21824) savepageoffset = 40768;
			if (lastpageoffset == 21824) savepageoffset = 40768+2880;
			if (lastpageoffset > 21824) savepageoffset = 2880;
		}
		else
		{
			if (lastpageoffset < 21600)
				savepageoffset = 43200;
			else
				savepageoffset = 0;
		}
	}
	j = scrsize;
	if (j < 18000)
		j = 18000;
	if (vidmode == 1)
		j = 21600;
	if (sortcnt == -1)
		copywindow(saidwelcome,(unsigned)47536,18000);
	else
		copywindow(lastpageoffset,savepageoffset,j);
	while ((mainmenuplace >= 0) && (done == 0))
	{
		if ((mainmenuplace = getselection(88,47,mainmenuplace,9)) >= 0)
		{
			if (mainmenuplace == 0)
				if ((k = newgamemenu()) >= 0)
				{
					done = 1;
					if ((k == 1) && (numboards < 20))
					{
						ksay(12);
						orderinfomenu();
						done = 0;
					}
					else if ((k == 2) && (numboards < 30))
					{
						ksay(12);
						orderinfomenu();
						done = 0;
					}
				}
			if (mainmenuplace == 1)
				if (loadsavegamemenu(1) >= 0)
					done = 1;
			if (mainmenuplace == 2)
			{
				if (sortcnt == -1)
					ksay(12);
				else
				{
					if (loadsavegamemenu(2) >= 0)
						done = 1;
				}
			}
			if (mainmenuplace == 3)
				mainmenuplace = (-mainmenuplace)-1;
			if (mainmenuplace == 4) helpmenu();
			if (mainmenuplace == 5) bigstorymenu();
			if (mainmenuplace == 6) orderinfomenu();
			if (mainmenuplace == 7) creditsmenu();
			if (mainmenuplace == 8) done = areyousure();
			if (done == 0)
			{
				if (sortcnt == -1)
					 copywindow((unsigned)47536,saidwelcome,18000);
				else
					 copywindow(savepageoffset,lastpageoffset,j);
			}
		}
	}
	if (sortcnt != -1)
		linecompare(statusbar);
	j = mainmenuplace;
	if (mainmenuplace < 0)
		mainmenuplace = (-mainmenuplace)-1;
	return(j);
}

getselection(xoffs, yoffs, nowselector, totselectors)
int xoffs, yoffs, nowselector, totselectors;
{
	int animater6, n, esckeystate, mousx, mousy, bstatus, obstatus;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	keystatus[0x48] = 0, keystatus[0xc8] = 0, keystatus[0xcb] = 0;
	keystatus[0xd0] = 0, keystatus[0x50] = 0, keystatus[0xcd] = 0;
	keystatus[0x01] = 0, keystatus[0x1c] = 0, keystatus[0x9c] = 0;
	keystatus[0x39] = 0;
	animater6 = 0;
	esckeystate = 0;
	bstatus = 1;
	obstatus = 1;
	mousx = 0;
	mousy = 0;
	while (esckeystate == 0)
	{
		animater6++;
		if (animater6 == 6)
			animater6 = 0;
		_asm \
		{
			mov dx, 0x3da
whileretracemenu:
			in al, dx
			test al, 8
			jnz whileretracemenu
whilenoretracemenu:
			in al, dx
			test al, 8
			jz whilenoretracemenu
		}
		if (animater6 < 3)
			statusbardraw(20+animater6*14,32,13,13,xoffs+20-n,nowselector*12+yoffs+n-1,statusbarback,lastpageoffset+90);
		else
			statusbardraw(20+(animater6-3)*14,46,13,13,xoffs+20-n,nowselector*12+yoffs+n-1,statusbarback,lastpageoffset+90);
		obstatus = bstatus;
		if (moustat == 0)
			_asm \
			{
				mov ax, 11
				int 0x33
				add mousx, cx
				add mousy, dx
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
		if (((keystatus[0x48]|keystatus[0xc8]|keystatus[0xcb]) != 0) || (mousy < -128))
		{
			if (mousy < -128)
				mousy += 128;
			statusbardraw(16,15,13,13,xoffs+20-n,nowselector*12+yoffs+n-1,menu,lastpageoffset+90);
			nowselector--;
			ksay(27);
			if (nowselector < 0)
				nowselector = totselectors-1;
			keystatus[0x48] = 0, keystatus[0xc8] = 0, keystatus[0xcb] = 0;
		}
		if (((keystatus[0xd0]|keystatus[0x50]|keystatus[0xcd]) != 0) || (mousy > 128))
		{
			if (mousy > 128)
				mousy -= 128;
			statusbardraw(16,15,13,13,xoffs+20-n,nowselector*12+yoffs+n-1,menu,lastpageoffset+90);
			nowselector++;
			ksay(27);
			if (nowselector == totselectors)
				nowselector = 0;
			keystatus[0xd0] = 0, keystatus[0x50] = 0, keystatus[0xcd] = 0;
		}
		esckeystate = (keystatus[1]|(keystatus[0x1c]<<1)|(keystatus[0x9c]<<1)|(keystatus[0x39]<<1));
		if ((obstatus == 0) && (bstatus > 0))
			esckeystate |= (bstatus^3);
	}
	ksay(27);
	statusbardraw(36-n,15,13,13,xoffs+20-n,nowselector*12+yoffs+n-1,menu,lastpageoffset+90);
	if ((esckeystate&2) > 0)
		return(nowselector);
	else
		return((-nowselector)-1);
}

drawmenu(xsiz, ysiz, walnume)
int xsiz, ysiz, walnume;
{
	int ycent, i, j, x1, y1, x2, y2;

	if (vidmode == 0)
		ycent = 100;
	else
		ycent = 120;
	if (vidmode == 0)
		x1 = 180-(xsiz>>1);
	else
		x1 = 160-(xsiz>>1);
	y1 = ycent-(ysiz>>1)-1;
	x2 = x1+xsiz-16;
	y2 = y1+ysiz-16;
	statusbardraw(0,0,16,16,x1,y1,walnume,lastpageoffset+90);
	statusbardraw(48,0,16,16,x2,y1,walnume,lastpageoffset+90);
	statusbardraw(0,48,16,16,x1,y2,walnume,lastpageoffset+90);
	statusbardraw(48,48,16,16,x2,y2,walnume,lastpageoffset+90);
	for(i=x1+16;i<x2;i+=16)
	{
		statusbardraw(16,0,16,16,i,y1,walnume,lastpageoffset+90);
		statusbardraw(16,48,16,16,i,y2,walnume,lastpageoffset+90);
	}
	for(j=y1+16;j<y2;j+=16)
	{
		statusbardraw(0,16,16,16,x1,j,walnume,lastpageoffset+90);
		statusbardraw(48,16,16,16,x2,j,walnume,lastpageoffset+90);
	}
	for(i=x1+16;i<x2;i+=16)
		for(j=y1+16;j<y2;j+=16)
			statusbardraw(16,16,16,16,i,j,walnume,lastpageoffset+90);
}

creditsmenu()
{
	int n;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	drawmenu(320,176,menu);
	strcpy(&textbuf[0],"Credits");
	textprint(149,20+n,32,lastpageoffset+90);
	loadstory(-1);
	pressakey();
}

bigstorymenu()
{
	int i, j, k, n, nowenterstate, lastenterstate, quitstat, bstatus, obstatus;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	if (boardnum < 10) j = -32, k = -27;
	else if (boardnum < 20) j = -26, k = -24;
	else j = -23, k = -22;
	quitstat = 0;
	bstatus = 1;
	obstatus = 1;
	i = j;
	while (quitstat == 0)
	{
		drawmenu(304,192,menu);
		loadstory(i);
		nowenterstate = 1;
		lastenterstate = 1;
		while ((nowenterstate <= lastenterstate) && (bstatus <= obstatus))
		{
			lastenterstate = nowenterstate;
			nowenterstate = keystatus[0x1c];
			nowenterstate |= keystatus[0x9c];
			nowenterstate |= keystatus[1];
			nowenterstate |= keystatus[keydefs[17]];
			nowenterstate |= keystatus[0xc9];
			nowenterstate |= keystatus[0xc8];
			nowenterstate |= keystatus[0xcb];
			nowenterstate |= keystatus[0xd1];
			nowenterstate |= keystatus[0xd0];
			nowenterstate |= keystatus[0xcd];
			obstatus = bstatus;
			if (moustat == 0)
				_asm \
				{
					mov ax, 5
					int 0x33
					mov bstatus, ax
				}
		}
		if (((keystatus[0xc9]|keystatus[0xc8]|keystatus[0xcb]) > 0) && (i > j))
			i--;
		if (((keystatus[0xd1]|keystatus[0xd0]|keystatus[0xcd]) > 0) && (i < k))
			i++;
		quitstat = (keystatus[keydefs[17]]|keystatus[1]);
		if (((keystatus[0x1c]|keystatus[0x9c]) > 0) || (bstatus > obstatus))
		{
			bstatus = 1;
			obstatus = 1;
			i++;
			if (i > k)
				quitstat = 1;
		}
		ksay(27);
	}
}

areyousure()
{
	int i, n;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	drawmenu(224,64,menu);
	strcpy(&textbuf[0],"Really want to quit?");
	textprint(99,84+n,112,lastpageoffset+90);
	strcpy(&textbuf[0],"Yes");
	textprint(105,96+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"No");
	textprint(105,108+n,32,lastpageoffset+90);
	i = getselection(60,95,0,2);
	if (i == 0)
		return(1);
	else
		return(0);
}

helpmenu()
{
	int n;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	drawmenu(256,176,menu);
	loadstory(-15);
	strcpy(&textbuf[0],"Help");
	textprint(161,18+n,32,lastpageoffset+90);
	pressakey();
}

sodamenu()
{
	long ototclocker;
	unsigned savepageoffset;
	int n, valid;

	ototclocker = totalclock;
	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	ksay(27);
	if ((vidmode == 0) && (statusbar < 399)  && (sortcnt != -1))
	{
		savepageoffset = times90[((unsigned)statusbar+1)>>1]+5;
		copywindow(0,lastpageoffset+savepageoffset,18000-savepageoffset);
		linecompare(399);
	}
	if ((vidmode == 1) && (statusbar < 479) && (sortcnt != -1))
	{
		savepageoffset = times90[((unsigned)statusbar+1)>>1];
		copywindow(0,lastpageoffset+savepageoffset,21600-savepageoffset);
		linecompare(479);
	}
	drawmenu(256,160,menu);
	if (boardnum < 10)
		loadstory(-34);
	else
		loadstory(-33);
	statusbardraw(0,0,12,36,85-n,49+n,sodapics,lastpageoffset+90);
	statusbardraw(12,0,12,36,85-n,85+n,sodapics,lastpageoffset+90);
	statusbardraw(24,0,12,36,85-n,121+n,sodapics,lastpageoffset+90);
	statusbardraw(36,0,12,12,85-n,157+n,sodapics,lastpageoffset+90);
	valid = 0;
	while (valid == 0)
	{
		sodaplace = getselection(46,49,sodaplace,10);
		if (sodaplace >= 0)
		{
			valid = 1;
			if ((sodaplace == 0) && (coins < 1)) valid = 0;
			if ((sodaplace == 1) && (coins < 2)) valid = 0;
			if ((sodaplace == 2) && (coins < 2)) valid = 0;
			if ((sodaplace == 3) && (coins < 5)) valid = 0;
			if ((sodaplace == 4) && ((coins < 5) || (boardnum < 10))) valid = 0;
			if ((sodaplace == 5) && (coins < 75)) valid = 0;
			if ((sodaplace == 6) && (coins < 100)) valid = 0;
			if ((sodaplace == 7) && (coins < 150)) valid = 0;
			if ((sodaplace == 8) && ((coins < 200) || (boardnum < 10))) valid = 0;
			if ((sodaplace == 9) && (coins < 250)) valid = 0;
			if (valid == 0)
				ksay(12);
		}
		else
			valid = 1;
	}
	if ((sodaplace >= 0) && (valid == 1))
	{
		ksay(24);
		switch(sodaplace)
		{
			case 0:
				coins--;
				life += 320;
				if (life > 4095)
					life = 4095;
				drawlife();
				break;
			case 1:
				coins -= 2;
				if (purpletime < totalclock)
					purpletime = totalclock+9600;
				else purpletime += 9600;
				statusbardraw(0,0,16,15,159,13,statusbarinfo,0);
				break;
			case 2:
				coins -= 2;
				if (greentime < totalclock)
					greentime = totalclock + 9600;
				else greentime += 9600;
				statusbardraw(0,15,16,15,176,13,statusbarinfo,0);
				break;
			case 3:
				coins -= 5;
				if (capetime[0] < totalclock)
					capetime[0] = totalclock + 7200;
				else
					capetime[0] += 7200;
				statusbardraw(16,0,21,28,194,2,statusbarinfo,0);
				break;
			case 4:
				coins -= 5;
				if (capetime[1] < totalclock)
					capetime[1] = totalclock + 4800;
				else
					capetime[1] += 4800;
				statusbardraw(37,0,21,28,216,2,statusbarinfo,0);
				break;
			case 5:
				coins -= 75;
				lightnings++;
				if (lightnings > 6)
					lightnings = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(296,21,(char)0,0);
				textbuf[0] = lightnings+48, textbuf[1] = 0;
				textprint(296,21,(char)176,0);
				break;
			case 6:
				coins -= 100;
				firepowers[0]++;
				if (firepowers[0] > 6)
					firepowers[0] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(272,12,(char)0,0);
				textbuf[0] = firepowers[0]+48, textbuf[1] = 0;
				textprint(272,12,(char)176,0);
				break;
			case 7:
				coins -= 150;
				firepowers[1]++;
				if (firepowers[1] > 6)
					firepowers[1] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(272,21,(char)0,0);
				textbuf[0] = firepowers[1]+48, textbuf[1] = 0;
				textprint(272,21,(char)176,0);
				break;
			case 8:
				coins -= 200;
				firepowers[2]++;
				if (firepowers[2] > 6)
					firepowers[2] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(296,12,(char)0,0);
				textbuf[0] = firepowers[2]+48, textbuf[1] = 0;
				textprint(296,12,(char)176,0);
				break;
			case 9:
				coins -= 250;
				compass = 1;
				break;
		}
		textbuf[0] = 9, textbuf[1] = 9;
		textbuf[2] = 9, textbuf[3] = 0;
		textprint(112,12,(char)0,0);
		textbuf[0] = (coins/100)+48;
		textbuf[1] = ((coins/10)%10)+48;
		textbuf[2] = (coins%10)+48;
		textbuf[3] = 0;
		if (textbuf[0] == 48)
		{
			textbuf[0] = 32;
			if (textbuf[1] == 48)
			textbuf[1] = 32;
		}
		textprint(112,12,(char)176,0);
	}
	if (sodaplace < 0)
	{
		sodaplace = (-sodaplace)-1;
		ksay(26);
	}
	totalclock = ototclocker;
	clockspeed = 0;
	linecompare(statusbar);
}

orderinfomenu()
{
	int i, n, nowenterstate, lastenterstate, quitstat, bstatus, obstatus;

	bstatus = 1;
	obstatus = 1;
	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	i = -14;
	quitstat = 0;
	while (quitstat == 0)
	{
		drawmenu(304,192,menu);
		loadstory(i);
		nowenterstate = 1;
		lastenterstate = 1;
		while ((nowenterstate <= lastenterstate) && (bstatus <= obstatus))
		{
			lastenterstate = nowenterstate;
			nowenterstate = keystatus[0x1c];
			nowenterstate |= keystatus[0x9c];
			nowenterstate |= keystatus[1];
			nowenterstate |= keystatus[keydefs[17]];
			nowenterstate |= keystatus[0xc9];
			nowenterstate |= keystatus[0xc8];
			nowenterstate |= keystatus[0xcb];
			nowenterstate |= keystatus[0xd1];
			nowenterstate |= keystatus[0xd0];
			nowenterstate |= keystatus[0xcd];
			obstatus = bstatus;
			if (moustat == 0)
				_asm \
				{
					mov ax, 5
					int 0x33
					mov bstatus, ax
				}
		}
		if (((keystatus[0xc9]|keystatus[0xc8]|keystatus[0xcb]) > 0) && (i > -14))
			i--;
		if (((keystatus[0xd1]|keystatus[0xd0]|keystatus[0xcd]) > 0) && (i < -2))
			i++;
		quitstat = (keystatus[keydefs[17]]|keystatus[1]);
		if (((keystatus[0x1c]|keystatus[0x9c]) > 0) || (bstatus > obstatus))
		{
			i++;
			bstatus = 1;
			obstatus = 1;
			if (i > -2)
				quitstat = 1;
		}
		ksay(27);
	}
}

loadsavegamemenu(whichmenu)
int whichmenu;
{
	char filename[20];
	int fil, i, j, k, n;
	long templong;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	drawmenu(320,160,menu);
	if (whichmenu == 1)
	{
		strcpy(&textbuf[0],"Load game");
		textprint(137,26+n,32,lastpageoffset+90);
	}
	else
	{
		strcpy(&textbuf[0],"Save game");
		textprint(137,26+n,112,lastpageoffset+90);
	}
	strcpy(&textbuf[0],"#: Name:       Board: Score: Time:");
	textprint(55,52+n,48,lastpageoffset+90);
	if (gameheadstat == 0)
	{
		for(j=0;j<8;j++)
		{
			filename[0] = 'S', filename[1] = 'A', filename[2] = 'V';
			filename[3] = 'G', filename[4] = 'A', filename[5] = 'M';
			filename[6] = 'E', filename[7] = j+48;
			filename[8] = '.', filename[9] = 'D', filename[10] = 'A';
			filename[11] = 'T', filename[12] = 0;
			if((fil=open(filename,O_BINARY|O_RDONLY,S_IREAD))!=-1)
			{
				gamexist[j] = 1;
				read(fil,&gamehead[j][0],27);
			}
			else
				gamexist[j] = 0;
		}
		gameheadstat = 1;
	}
	j = 0;
	for(i=70+n;i<166+n;i+=12)
	{
		if (gamexist[j] == 1)
		{
			textbuf[0] = j+49, textbuf[1] = 32, textbuf[2] = 32;
			for(k=0;k<12;k++)
			{
				textbuf[k+3] = gamehead[j][k];
				if (textbuf[k+3] == 0)
					textbuf[k+3] = 32;
			}
			textbuf[15] = 32;
			textbuf[16] = ((gamehead[j][17]+1)/10)+48;
			if (textbuf[16] == 48)
				textbuf[16] = 32;
			textbuf[17] = ((gamehead[j][17]+1)%10)+48;
			textbuf[18] = 32;
			textbuf[19] = 32;
			textbuf[20] = 32;
			k = j*27;
			_asm \
			{
				mov si, k
				mov ax, word ptr gamehead[si+19]
				mov dx, word ptr gamehead[si+21]
				mov word ptr templong[0], ax
				mov word ptr templong[2], dx
			}
			textbuf[21] = (char)((templong/100000L)%10L)+48;
			textbuf[22] = (char)((templong/10000L)%10L)+48;
			textbuf[23] = (char)((templong/1000L)%10L)+48;
			textbuf[24] = (char)((templong/100L)%10L)+48;
			textbuf[25] = (char)((templong/10L)%10L)+48;
			textbuf[26] = (char)(templong%10L)+48;
			textbuf[27] = 32;
			k = 21;
			while ((textbuf[k] == 48) && (k < 26))
				textbuf[k++] = 32;
			k = j*27;
			_asm \
			{
				mov si, k
				mov ax, word ptr gamehead[si+23]
				mov dx, word ptr gamehead[si+25]
				mov word ptr templong[0], ax
				mov word ptr templong[2], dx
			}
			templong /= 240;
			textbuf[28] = (char)((templong/10000L)%10L)+48;
			textbuf[29] = (char)((templong/1000L)%10L)+48;
			textbuf[30] = (char)((templong/100L)%10L)+48;
			textbuf[31] = (char)((templong/10L)%10L)+48;
			textbuf[32] = (char)(templong%10L)+48;
			textbuf[33] = 0;
			k = 28;
			while ((textbuf[k] == 48) && (k < 32))
				textbuf[k++] = 32;
			textprint(56,i-1,30,lastpageoffset+90);
			textprint(55,i-1,32,lastpageoffset+90);
			close(fil);
		}
		else
		{
			textbuf[0] = j+49;
			textbuf[1] = 0;
			textprint(56,i-1,28,lastpageoffset+90);
			textprint(55,i-1,30,lastpageoffset+90);
		}
		j++;
	}
	if (whichmenu == 1)
	{
		do
		{
			loadsavegameplace = getselection(16,67,loadsavegameplace,8);
		}
		while ((loadsavegameplace >= 0) && (gamexist[loadsavegameplace] == 0));
		j = loadsavegameplace;
		if ((loadsavegameplace < 0) || (gamexist[loadsavegameplace] == 0))
			loadsavegameplace = (-loadsavegameplace)-1;
	}
	else
	{
		loadsavegameplace = getselection(16,67,loadsavegameplace,8);
		j = loadsavegameplace;
		if (loadsavegameplace < 0)
			loadsavegameplace = (-loadsavegameplace)-1;
	}
	return(j);
}

newgamemenu()
{
	int j, n;

	if (vidmode == 0)
		n = 0;
	else
		n = 20;
	drawmenu(288,64,menu);
	strcpy(&textbuf[0],"New game");
	textprint(137,74+n,112,lastpageoffset+90);
	strcpy(&textbuf[0],"Episode 1: Search for Sparky");
	textprint(67,88+n,32,lastpageoffset+90);
	if (numboards >= 20) j = 32; else j = 28;
	strcpy(&textbuf[0],"Episode 2: Sparky's Revenge");
	textprint(67,100+n,((char)j),lastpageoffset+90);
	if (numboards >= 30) j = 32; else j = 28;
	strcpy(&textbuf[0],"Episode 3: Find the Way Home");
	textprint(67,112+n,((char)j),lastpageoffset+90);
	if (newgameplace < 0) newgameplace = 0;
	if (newgameplace > 2) newgameplace = 2;
	newgameplace = getselection(28,87,newgameplace,3);
	if ((newgameplace == 1) && (numboards < 20))
		return(newgameplace);
	if ((newgameplace == 2) && (numboards < 30))
		return(newgameplace);
	drawmenu(288,64,menu);
	strcpy(&textbuf[0],"New game");
	textprint(137,74+n,112,lastpageoffset+90);
	strcpy(&textbuf[0],"Easy: Don't touch me.");
	textprint(67,92+n,32,lastpageoffset+90);
	strcpy(&textbuf[0],"Hard: OUCH!");
	textprint(67,104+n,32,lastpageoffset+90);
	if (skilevel < 0) skilevel = 0;
	if (skilevel > 1) skilevel = 1;
	skilevel = getselection(28,91,skilevel,2);
	j = newgameplace;
	if (newgameplace < 0)
		newgameplace = (-newgameplace)-1;
	return(j);
}

pressakey()
{
	int bstatus, obstatus;

	bstatus = 1;
	obstatus = 1;
	keystatus[0x1c] = 0, keystatus[0x9c] = 0, keystatus[0x1] = 0, keystatus[0x39] = 0;
	while (((keystatus[0x1c]|keystatus[0x9c]|keystatus[1]|keystatus[0x39]) == 0) && (bstatus <= obstatus))
	{
		obstatus = bstatus;
		if (moustat == 0)
			_asm \
			{
				mov ax, 5
				int 0x33
				mov bstatus, ax
			}
	}
	ksay(27);
}

loadvms(walnume)
int walnume;
{
	unsigned char bitcnt, numbits;
	int i, j, fil, bytecnt1, bytecnt2;
	int currstr, strtot, compleng, dat, goalstr;
	unsigned int walsegg;
	long templong;

	templong = 0x7fffffff;
	j = 0;
	for(i=0;i<numwalls;i++)
		if (memtype[i] > 0)
			if (walrec[i] < templong)
				if ((i != map-1) && (i != textwall-1) && (i != textwall) && (i != slotto))
				{
					templong = walrec[i];
					j = i;
				}
	walrec[walnume] = totalclock;
	memtype[walnume] = memtype[j];
	walseg[walnume] = walseg[j];
	memtype[j] = 0;
	if ((fil = open("walls.kzp",O_BINARY|O_RDWR,S_IREAD)) != -1)
	{
		lseek(fil,tioffs[walnume],SEEK_SET);
		compleng = tileng[walnume];
		read(fil,&strtot,2);
		_asm \
		{
			mov bx, 0
			mov di, 0
starttempbufsave:
			mov ax, word ptr tempbuf[bx]
			and ax, 0x0101
			shl ah, 1
			or al, ah
			mov dl, al
			mov ax, word ptr tempbuf[bx+2]
			and ax, 0x0101
			shl ah, 1
			or al, ah
			shl al, 1
			shl al, 1
			or dl, al
			mov ax, word ptr tempbuf[bx+4]
			and ax, 0x0101
			shl ah, 1
			or al, ah
			mov cl, 4
			shl al, cl
			or dl, al
			mov ax, word ptr tempbuf[bx+6]
			and ax, 0x0101
			shl ah, 1
			or al, ah
			mov cl, 6
			shl al, cl
			or dl, al
			mov byte ptr linbits[di], dl
			inc di
			add bx, 8
			cmp bx, 4096
			jb starttempbufsave
		}
		read(fil,&tempbuf[0],compleng);
		_asm \
		{
			mov bx, walnume
			shl bx, 1
			mov ax, walseg[bx]
			mov walsegg, ax
		}
		if ((emswalls > 0) && ((memtype[walnume]&1024) > 0))
			emsetpages(memtype[walnume]-1024,0);
		if (strtot > 0)
		{
			tempbuf[compleng] = 0;
			tempbuf[compleng+1] = 0;
			tempbuf[compleng+2] = 0;
			bytecnt2 = 0;
			bytecnt1 = 0;
			bitcnt = 0;
			currstr = 256;
			goalstr = 512;
			numbits = 9;
			do
			{
				_asm \
				{
					mov es, lzwbuf
					mov dl, bitcnt
					mov dh, numbits
					mov si, bytecnt2
					mov di, word ptr tempbuf[si]
					mov cl, dl
					shr di, cl
					mov cl, dh
					mov bx, 1
					shl bx, cl
					dec bx
					and di, bx
					add cl, dl
					cmp cl, 16
					jbe skipextrabits2x
					mov al, byte ptr tempbuf[si+2]
					mov bx, 1
					rol bx, cl
					dec bx
					and ax, bx
					mov cl, dl
					ror ax, cl
					add di, ax
skipextrabits2x:
					mov dat, di
					add dl, dh
					mov dh, 0
					mov bx, dx
					mov cl, 3
					shr bx, cl
					add bytecnt2, bx
					and dl, 7
					mov bitcnt, dl

					mov cx, 1
					mov bx, dat
					mov di, currstr
					shl di, 1
					mov es:[di], bx
					cmp bx, 256
					jb endsearchtable2x
beginsearchtable2x:
					inc cx
					mov al, es:[bx+8200]
					push ax
					shl bx, 1
					mov bx, es:[bx]
					cmp bx, 256
					jae beginsearchtable2x
endsearchtable2x:
					mov ax, bx
					mov ah, al
					shr di, 1
					mov es:[di+8199], ax
					shl bx, 1
					mov bx, es:[bx]
					push bx
					mov es, walsegg
					mov di, bytecnt1
reversedatax:
					pop ax
					cmp di, 4096
					jae skipstoredatax
					stosb
skipstoredatax:
					loop reversedatax
					mov bytecnt1, di
				}
				currstr++;
				if (currstr == goalstr)
				{
					numbits++;
					goalstr = (goalstr<<1);
				}
			}
			while (currstr <= strtot);
		}
		else
			_asm \
			{
				mov cx, 2048
				mov es, walsegg
				mov di, 0
				mov si, offset tempbuf
				mov ax, seg tempbuf
				push ds
				mov ds, ax
				rep movsw
				pop ds
			}
		if ((xmswalls > 0) && (memtype[walnume] != 65535) && ((memtype[walnume]&2048) > 0))
			xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[walnume]-2048))<<12,4096L);
		close(fil);
		_asm \
		{
			mov bx, 0
			mov di, 0
starttempbufrestore:
			mov dl, byte ptr linbits[di]
			mov dh, dl
			shr dh, 1
			mov ax, dx
			and ax, 0x0101
			mov word ptr tempbuf[bx], ax
			mov ax, dx
			shr ax, 1
			shr ax, 1
			and ax, 0x0101
			mov word ptr tempbuf[bx+2], ax
			mov ax, dx
			mov cl, 4
			shr ax, cl
			and ax, 0x0101
			mov word ptr tempbuf[bx+4], ax
			mov ax, dx
			mov cl, 6
			shr ax, cl
			and ax, 0x0101
			mov word ptr tempbuf[bx+6], ax
			inc di
			add bx, 8
			cmp bx, 4096
			jb starttempbufrestore
		}
	}
}

copyslots(slotnum)
int slotnum;
{
	int i, j, k;
	unsigned l;

	if (memtype[slotinfo-1] != 65535)
	{
		if (vms == 1)
			if (memtype[slotinfo-1] == 0)
				loadvms(slotinfo-1);
		if ((emswalls > 0) && ((memtype[slotinfo-1]&1024) > 0))
			emsetpages(memtype[slotinfo-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[slotinfo-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[slotinfo-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	l = walseg[slotinfo-1];
	for(i=0;i<3;i++)
		for(j=0;j<8;j++)
		{
			k = ((slotpos[i]+j)&63)+(((slotpos[i]+j)&64)<<3)+(i<<10);
			_asm \
			{
				mov es, l
				mov bx, offset tempbuf
				add bh, byte ptr i[0]
				mov ax, j
				mov cl, 3
				shl ax, cl
				add bx, ax
				mov di, k
				mov al, byte ptr es:[di]
				mov ah, byte ptr es:[di+64]
				mov word ptr ds:[bx], ax
				mov al, byte ptr es:[di+128]
				mov ah, byte ptr es:[di+192]
				mov byte ptr ds:[bx+2], ax
				mov al, byte ptr es:[di+256]
				mov ah, byte ptr es:[di+320]
				mov word ptr ds:[bx+4], ax
				mov al, byte ptr es:[di+384]
				mov ah, byte ptr es:[di+448]
				mov word ptr ds:[bx+6], ax
			}
		}
	if (memtype[slotnum-1] != 65535)
	{
		if (vms == 1)
			if (memtype[slotnum-1] == 0)
				loadvms(slotnum-1);
		if ((emswalls > 0) && ((memtype[slotnum-1]&1024) > 0))
			emsetpages(memtype[slotnum-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[slotnum-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[slotnum-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	l = walseg[slotnum-1];
	for(i=0;i<3;i++)
		for(j=0;j<8;j++)
		{
			k = 1296+(((i<<3)+i)<<6)+j;
			_asm \
			{
				mov es, l
				mov bx, offset tempbuf
				add bh, byte ptr i[0]
				mov ax, j
				mov cl, 3
				shl ax, cl
				add bx, ax
				mov di, k
				mov ax, word ptr ds:[bx]
				mov byte ptr es:[di], al
				mov byte ptr es:[di+64], ah
				mov ax, word ptr ds:[bx+2]
				mov byte ptr es:[di+128], al
				mov byte ptr es:[di+192], ah
				mov ax, word ptr ds:[bx+4]
				mov byte ptr es:[di+256], al
				mov byte ptr es:[di+320], ah
				mov ax, word ptr ds:[bx+6]
				mov byte ptr es:[di+384], al
				mov byte ptr es:[di+448], ah
			}
		}
	if ((xmswalls > 0) && (memtype[slotnum-1] != 65535) && ((memtype[slotnum-1]&2048) > 0))
		xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[slotnum-1]-2048))<<12,4096L);
}

youarehere()
{
	unsigned l;

	if (memtype[map-1] != 65535)
	{
		if (vms == 1)
			if (memtype[map-1] == 0)
				loadvms(map-1);
		if ((emswalls > 0) && ((memtype[map-1]&1024) > 0))
			emsetpages(memtype[map-1]-1024,0);
		else if ((xmswalls > 0) && ((memtype[map-1]&2048) > 0))
			xmsmemmove(xmshandle,((long)(memtype[map-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
	}
	_asm \
	{
		mov es, walseg[(map-1)+(map-1)]
		mov di, yourhereoldpos
		mov bx, di
		mov al, byte ptr board[di+bx]
		mov byte ptr es:[di], al
	}
	yourhereoldpos = ((posx>>10)<<6)+(posy>>10);
	_asm \
	{
		mov es, walseg[(map-1)+(map-1)]
		mov di, yourhereoldpos
		mov al, 255
		mov byte ptr es:[di], al
	}
	if ((xmswalls > 0) && ((memtype[map-1]&2048) >= 0) && (memtype[map-1] != 65535))
		xmsmemmove(0,((long)xmspageframe)<<16,xmshandle,((long)(memtype[map-1]-2048))<<12,4096L);
}

copywindow(souroffs,destoffs,numblocks)
unsigned souroffs, destoffs, numblocks;
{
	_asm \
	{
		mov dx, 0x3ce
		mov al, 0x5
		out dx, al
		inc dx
		in al, dx
		and al, 252
		or al, 1
		out dx, al
		mov dx, 0x3c4
		mov ax, 0x0f02
		out dx, al
		inc dx
		mov al, ah
		out dx, al
		cld
		mov ax, 0xa000
		mov es, ax
		mov si, souroffs
		mov di, destoffs
		mov cx, numblocks
		push ds
		mov ds, ax
		rep movsb
		pop ds
		mov dx, 0x3ce
		mov al, 0x5
		out dx, al
		inc dx
		in al, dx
		and al, 252
		out dx, al
	}
}

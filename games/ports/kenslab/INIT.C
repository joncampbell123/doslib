#include "lab3d.h"

initialize()
{
	char ksmfile[15];
	int i, j, k, walcounter, oclockspeed;
	unsigned int l;
	time_t tnow;

	loadtables();
	vidmode = option[0];
	mute = 0;
	speechstatus = option[1];
	musicsource = (option[2]-1);
	moustat = ((3-option[3])&1);
	joystat = ((3-option[3])>>1);
	lastsoundnum = 65535;
	if ((joyx1 == joyx2) || (joyx2 == joyx3) || (joyy1 == joyy2) || (joyy2 == joyy3))
		joystat = 1;
	time(&tnow);
	srand((unsigned)tnow);
	if ((note = convallocate(16384>>4)) == -1)
	{
		printf("Error #1:  Memory allocation failed.");
		exit(-1);
	}
	if ((snd = convallocate(4096)) == -1)
	{
		printf("Error #2:  Memory allocation failed.");
		exit(-1);
	}
	if (speechstatus >= 2)
	{
		reset_dsp();
		_asm \
		{
			mov dx, 0x20c
			mov ax, speechstatus
			mov cl, 4
			shl ax, cl
			add dx, ax
ksbsetwait1:
			in al, dx
			test al, 0x80
			jnz ksbsetwait1
			mov al, 0xd1
			out dx, al
ksbsetwait2:
			in al, dx
			test al, 0x80
			jnz ksbsetwait2
			mov al, 0x40
			out dx, al
ksbsetwait3:
			in al, dx
			test al, 0x80
			jnz ksbsetwait3
			mov al, 165
			out dx, al
		}
	}
	sndaddr = (((long)snd)<<4);
	if ((sndaddr&32768) > 0)
	{
		l = (unsigned)(sndaddr>>4);
		for(i=0;i<8;i++)
		{
			memtype[i] = 65535;
			walseg[i] = l;
			l += 256;
		}
	}
	else
	{
		l = (unsigned)((sndaddr+32768)>>4);
		for(i=0;i<8;i++)
		{
			memtype[i] = 65535;
			walseg[i] = l;
			l += 256;
		}
	}
	if ((lzwbuf = convallocate(12304>>4)) == -1)
	{
		printf("Error #3:  Memory allocation failed.");
		exit(-1);
	}
	oldlogpage = -1;
	oldphyspage = -1;
	oldsourhand = -1;
	oldsouroff = -1;
	convwalls = numwalls;
	emswalls = 0;
	xmswalls = 0;
	if ((pic = convallocate((160-initialwalls)<<8)) == -1)
	{
		convwalls = ((convavailpages&0xff00)>>8)+initialwalls;
		emswalls = (numwalls-convwalls);
		pic = convallocate((convwalls-initialwalls)<<8);
		if ((emmhandle = emmallocate((emswalls+3)>>2)) == -1)
		{
			convwalls--;
			xmspageframe = pic+((convwalls-initialwalls)<<8);
			xmswalls = (numwalls-convwalls);
			emswalls = 0;
			if ((xmshandle = xmsallocate(xmswalls<<2)) == -1)
			{
				if (convwalls < 32)
				{
					printf("Error #4:  This computer does not have enough memory.\n");
					printf(" - Ken's Labyrinth needs about 365K in conventional memory to run.\n");
					exit(-1);
				}
				xmswalls = 0;
				vms = 1;
			}
		}
	}
	walcounter = initialwalls;
	if (convwalls > initialwalls)
	{
		l = pic;
		for(i=0;i<convwalls-initialwalls;i++)
		{
			memtype[walcounter] = 65535;
			walseg[walcounter] = l;
			walcounter++;
			l += 256;
		}
	}
	if (emswalls > 0)
		for(i=0;i<emswalls;i++)
		{
			memtype[walcounter] = (i>>2)+1024;
			walseg[walcounter] = emmpageframe+((i&3)<<8);
			walcounter++;
		}
	if (xmswalls > 0)
		for(i=0;i<xmswalls;i++)
		{
			memtype[walcounter] = i+2048;
			walseg[walcounter] = xmspageframe;
			walcounter++;
		}
	if (vms == 1)
	{
		numcells = walcounter;
		for(i=0;i<numwalls;i++)
			walrec[i] = 0;
		for(i=numcells;i<numwalls;i++)
		{
			memtype[i] = 0;
			walseg[i] = walseg[0];
		}
	}
	l = 0;
	for(i=0;i<240;i++)
	{
		times90[i] = l;
		l += 90;
	}
	less64inc[0] = 16384;
	for(i=1;i<64;i++)
		less64inc[i] = 16384 / i;
	speed = 240;
	for(i=0;i<256;i++)
		keystatus[i] = 0;
	oldkeyhandler = _dos_getvect(0x9);
	_disable(); _dos_setvect(0x9, keyhandler); _enable();
	olddivzerohandler = _dos_getvect(0x0);
	_disable(); _dos_setvect(0, divzerohandler); _enable();
	oldhand = _dos_getvect(0x8);
	saidwelcome = 0;
	loadmusic("BEGIN");
	clockspeed = 0;
	slottime = 0;
	owecoins = 0;
	owecoinwait = 0;
	slotpos[0] = 0;
	slotpos[1] = 0;
	slotpos[2] = 0;
	clockspeed = 0;
	skilevel = 0;
	musicon();
	fade(0);
	kgif(0);
	loadwalls();
	kgif(1);
	outp(0x3d4,0xc), outp(0x3d5,5>>8);
	outp(0x3d4,0xd), outp(0x3d5,5&255);
	_asm \
	{
		mov dx, 0x3da
retraceholdfadeinitsnotxmea:
		in al, dx
		test al, 8
		jz retraceholdfadeinitsnotxmea
retraceholdfadeinitsnotxmeb:
		in al, dx
		test al, 8
		jnz retraceholdfadeinitsnotxmeb
	}
	fade(63);
	if (moustat == 0)
		moustat = setupmouse();
	oclockspeed = clockspeed;
	while ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0) && (clockspeed < oclockspeed+960))
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
	}
	fade(0);
	kgif(2);
	l = 25200;
	i = 1;
	outp(0x3d4,0xc), outp(0x3d5,l>>8);
	outp(0x3d4,0xd), outp(0x3d5,l&255);
	fade(63);
	while ((keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0) && (clockspeed < 7680))
	{
		if (i == 0)
		{
			l += 90;
			if (l >= 25200)
			{
				i = 1;
				l = 25200;
			}
		}
		if (i == 1)
		{
			l -= 90;
			if (l > 32768)
			{
				l = 0;
				i = 0;
			}
		}
		_asm \
		{
			mov dx, 0x3da
retraceholdfadeinita:
			in al, dx
			test al, 8
			jz retraceholdfadeinita
retraceholdfadeinitb:
			in al, dx
			test al, 8
			jnz retraceholdfadeinitb
			mov dx, 0x3da
retraceholdfadeinitc:
			in al, dx
			test al, 8
			jz retraceholdfadeinitc
retraceholdfadeinitd:
			in al, dx
			test al, 8
			jnz retraceholdfadeinitd
retraceholdfadeinite:
			in al, dx
			test al, 8
			jz retraceholdfadeinite
retraceholdfadeinitf:
			in al, dx
			test al, 8
			jnz retraceholdfadeinitf
		}
		outp(0x3d4,0xc), outp(0x3d5,l>>8);
		outp(0x3d4,0xd), outp(0x3d5,l&255);
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
	}
	for(i=63;i>=0;i-=4)
	{
	  _asm \
	  {
		  mov dx, 0x3da
retraceholdfade9:
		  in al, dx
		  test al, 8
		  jz retraceholdfade9
retraceholdfade10:
		  in al, dx
		  test al, 8
		  jnz retraceholdfade10
	  }
	  fade(64+i);
	}
	k = 0;
	for(i=0;i<16;i++)
		for(j=1;j<17;j++)
		{
			palette[k++] = (paldef[i][0]*j)/17;
			palette[k++] = (paldef[i][1]*j)/17;
			palette[k++] = (paldef[i][2]*j)/17;
		}
	lastunlock = 1;
	lastshoot = 1;
	lastbarchange = 1;
	cheatenable = 0;
	hiscorenamstat = 0;
	if ((i = open("boards.kzp",O_BINARY|O_RDWR,S_IREAD)) != 1)
	{
		read(i,&boleng[0],30*4);
		numboards = 30;
		if ((boleng[40]|boleng[41]) == 0)
			numboards = 20;
		if ((boleng[20]|boleng[21]) == 0)
				numboards = 10;
		close(i);
	}
	musicoff();
}

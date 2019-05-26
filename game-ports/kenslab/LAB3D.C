#define MAIN
#include "lab3d.h"
unsigned char slotable[3][16] =
{
	5,2,4,5,3,0,4,1,2,4,5,3,5,4,1,3,
	4,2,5,4,3,1,5,0,3,5,4,2,1,4,5,3,
	4,2,0,4,5,4,1,3,1,5,4,3,2,5,3,5
};

main(int argc,char **argv)
{
	char ksmfile[15], hitnet, cheatkeysdown, won;
	int i, j, jj, k, m, n, x, y, brd0, brd1, brd2, brd3, incenter;
	unsigned int l, newx, newy, oposx, oposy, plcx, plcy;
	long templong;

	initialize();
	if (argc >= 2)
	{
		for(i=0;i<8;i++)
			ksmfile[i] = argv[argc-1][i];
		i = 0;
		while ((ksmfile[i] != 0) && (i < 8))
		{
			ksmfile[i] = (ksmfile[i]^(i<<1))+7;
			i++;
		}
		if (i == 7)
		{
			ksmfile[7] = 0;
			if (strcmp(ksmfile,"zslz‚…|") == 0)
				cheatenable = 1;
		}
	}
	kgif(1);
	introduction(0);
	won = 0;
	while (quitgame == 0)
	{
		if (death < 4095)
		{
			fade(death>>6);
			posz+=2;
			if (posz > 64)
				posz = 64;
			if (angvel < 0)
			{
				angvel -= 8;
				if (angvel < -32)
					angvel = -32;
			}
			if (angvel > 0)
			{
				angvel += 8;
				if (angvel > 32)
					angvel = 32;
			}
			ang = (ang+angvel)&2047;
			death -= (((((int)(totalclock-ototclock))<<6)&0xffc0)+64);
			if (death <= 0)
				death = 0;
			if (lifevests == 0)
			{
				for(i=lside;i<rside;i++)
					height[i] = 0;
				i = pageoffset;
				pageoffset = lastpageoffset;
				x = 180 + ((int)(sintable[(2048-death)&2047]>>10));
				y = halfheight + ((int)(sintable[death&2047]>>11));
				pictur(x,y,(144-(death>>5))<<2,death&2047^2047,gameover);
				pageoffset = i;
			}
			if (death == 0)
			{
				if (lifevests > 0)
				{
					death = 4095;
					life = 4095;
					purpletime = totalclock-1;
					greentime = totalclock-1;
					capetime[0] = totalclock-1;
					capetime[1] = totalclock-1;
					statusbardraw(15,13,16,15,159,13,statusbarback,0);
					statusbardraw(16,13,16,15,176,13,statusbarback,0);
					statusbardraw(16,2,21,28,194,2,statusbarback,0);
					statusbardraw(16,2,21,28,216,2,statusbarback,0);
					drawlife();
					lifevests--;
					textbuf[0] = 9, textbuf[1] = 0;
					textprint(96,12,(char)0,0);
					textbuf[0] = lifevests+48, textbuf[1] = 0;
					textprint(96,12,(char)176,0);
					musicon();
					posz = 32;
					posx = startx;
					posy = starty;
					ang = startang;
					angvel = 0;
					vel = 0;
					mxvel = 0;
					myvel = 0;
					svel = 0;
					hvel = 0;
					fade(63);
				}
				else
				{
					for(i=0;i<60;i++)
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
					for(i=fadewarpval;i>=0;i-=2)
					{
						_asm \
						{
							mov dx, 0x3da
retraceholdfade7:
							in al, dx
							test al, 8
							jz retraceholdfade7
retraceholdfade8:
							in al, dx
							test al, 8
							jnz retraceholdfade8

							mov dx, 0x3c8
							mov al, 240
							out dx, al
							inc dx
							mov bx, 720
							mov ch, byte ptr i[0]
startfadegameover:
							mov al, palette[bx]
							mul ch
							mov cl, 6
							shr ax, cl
							out dx, al
							inc bx
							cmp bx, 768
							jb startfadegameover
						}
					}
					fade(0);
					kgif(1);
					introduction(1);
				}
			}
		}
		for(i=0;i<bulnum;i++)
		{
			switch(bulkind[i])
			{
				case 1: case 18:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul1fly+animate3);
					break;
				case 2: case 19:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul2fly+animate2);
					break;
				case 3: case 20:
					k = bul3fly+animate2+2;
					j = (1024+bulang[i]-ang)&2047;
					if (j < 960)
						k -= 2;
					if (j > 1088)
						k += 2;
					checkobj(bulx[i],buly[i],posx,posy,ang,k);
					break;
				case 4: case 21:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul3halfly+animate2);
					break;
				case 5: case 6:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul4fly);
					break;
				case 7: case 8:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul6fly+animate2);
					break;
				case 9: case 10:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul5fly+animate2);
					break;
				case 11: case 12:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul9fly);
					break;
				case 13: case 14:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul8fly);
					break;
				case 15: case 16: case 17:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul7fly+bulkind[i]-15);
					break;
				case 22: case 23:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul10fly);
					break;
				case 24: case 25:
					checkobj(bulx[i],buly[i],posx,posy,ang,bul11fly+animate7);
					break;
			}
			if (bulstat[i] < totalclock)
			{
				bulnum--;
				bulx[i] = bulx[bulnum];
				buly[i] = buly[bulnum];
				bulang[i] = bulang[bulnum];
				bulstat[i] = bulstat[bulnum];
				bulkind[i] = bulkind[bulnum];
			}
		}
		picrot(posx,posy,posz,ang);
		sortcnt = 0;
		if ((musicstatus == 1) && (clockspeed >= 0) && (clockspeed < 3))
			_asm \
			{
limitframerate:
				cmp clockspeed, 3
				jb limitframerate
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
		if (musicsource == 1)
		{
			if (keystatus[51] > 0)
			{
				midiinst = (midiinst + 29) % 30;
				setmidiinsts();
				keystatus[51] = 0;
			}
			if (keystatus[52] > 0)
			{
				midiinst = (midiinst + 1) % 30;
				setmidiinsts();
				keystatus[52] = 0;
			}
		}
		if (statusbar == statusbargoal)
		{
			if (keystatus[keydefs[14]] > 0)
			{
				if (lastbarchange == 0)
				{
					if (vidmode == 0)
						statusbargoal = (399+335)-statusbar;
					else
						statusbargoal = (479+415)-statusbar;
					if (statusbargoal > statusbar)
						scrsize += 2880;
					if (vidmode == 1)
					{
						statusbar = statusbargoal;
						if (statusbar == 415)
						{
							scrsize = 18720;
							statusbaralldraw();
							if (pageoffset == 0)
								pageoffset = 2880;
							if (pageoffset == 43200)
								pageoffset = 40320;
							if (lastpageoffset == 0)
								lastpageoffset = 2880;
							if (lastpageoffset == 43200)
								lastpageoffset = 40320;
						}
						else
							scrsize = 21600;
						linecompare(statusbar);
					}
				}
				lastbarchange = 1;
			}
			else
				lastbarchange = 0;
		}
		else
		{
			if (statusbargoal < statusbar)
			{
				statusbar -= (clockspeed>>1);
				if (statusbar <= statusbargoal)
				{
					statusbar = statusbargoal;
					scrsize -= 2880;
				}
			}
			else
			{
				statusbar += (clockspeed>>1);
				if (statusbar >= statusbargoal)
					statusbar = statusbargoal;
			}
			linecompare(statusbar);
		}
		if ((totalclock^ototclock) > 15)
		{
			animate2 = (animate2^1);
			animate3++;
			if (animate3 == 3)
				animate3 = 0;
			animate4 = (animate4+1)&3;
			switch(oscillate3)
			{
				case 0: oscillate3 = 1; break;
				case 1: oscillate3 = 2; break;
				case 2: oscillate3 = 1025; break;
				case 1025: oscillate3 = 0; break;
			}
			switch(oscillate5)
			{
				case 0: oscillate5 = 1; break;
				case 1: oscillate5 = 2; break;
				case 2: oscillate5 = 3; break;
				case 3: oscillate5 = 4; break;
				case 4: oscillate5 = 1027; break;
				case 1027: oscillate5 = 1026; break;
				case 1026: oscillate5 = 1025; break;
				case 1025: oscillate5 = 0; break;
			}
			animate8++;
			if (animate8 == 8)
				animate8 = 0;
			animate10++;
			if (animate10 == 10)
				animate10 = 0;
			animate11++;
			if (animate11 == 11)
				animate11 = 0;
			animate15++;
			if (animate15 == 15)
				animate15 = 0;
		}
		animate6++;
		if (animate6 == 6)
			animate6 = 0;
		animate7++;
		if (animate7 == 6)
			animate7 = 0;
		if (death == 4095)
		{
			if ((keystatus[keydefs[9]]|keystatus[keydefs[10]]|keystatus[keydefs[11]]) > 0)
			{
				j = bulchoose;
				if (keystatus[keydefs[9]] > 0) bulchoose = 0;
				if (keystatus[keydefs[10]] > 0) bulchoose = 1;
				if (keystatus[keydefs[11]] > 0) bulchoose = 2;
				if (firepowers[bulchoose] == 0)
				{
					ksay(12);
					bulchoose = j;
				}
				else
				{
					if (j == 0) x = 268, y = 11;
					if (j == 1) x = 268, y = 20;
					if (j == 2) x = 292, y = 11;
					statusbardraw(16,y,25,1,x,y,statusbarback,0);
					statusbardraw(16,y+8,25,1,x,y+8,statusbarback,0);
					statusbardraw(16,y+1,1,7,x,y+1,statusbarback,0);
					statusbardraw(16,y+1,1,7,x+24,y+1,statusbarback,0);
					if (j == 1)
						statusbardraw(41,28,1,9,292,20,statusbarinfo,0);
					if (bulchoose == 0) x = 268, y = 11;
					if (bulchoose == 1) x = 268, y = 20;
					if (bulchoose == 2) x = 292, y = 11;
					statusbardraw(32,28,8,9,x,y,statusbarinfo,0);
					statusbardraw(34,28,6,9,x+8,y,statusbarinfo,0);
					statusbardraw(34,28,6,9,x+14,y,statusbarinfo,0);
					statusbardraw(36,28,5,9,x+20,y,statusbarinfo,0);
				}
			}
			if (((keystatus[keydefs[8]] > 0) || ((bstatus&1) > 0)))
			{
				if (lastbulshoot+240-(firepowers[bulchoose]<<5) < totalclock)
				{
					if ((firepowers[bulchoose] > 0) && (bulnum < 64))
					{
						bulx[bulnum] = posx;
						buly[bulnum] = posy;
						bulang[bulnum] = ang;
						bulstat[bulnum] = totalclock+120+(lightnings<<5);
						bulkind[bulnum] = bulchoose+1;
						lastbulshoot = totalclock;
						bulnum++;
						if (bulchoose == 2)
							ksay(22);
						else
							ksay(15);
					}
					else
					{
						if ((lastshoot == 0) && (firepowers[bulchoose] == 0))
							ksay(12);
					}
				}
				lastshoot = 1;
			}
			else
				lastshoot = 0;
		}
		for(i=0;i<bulnum;i++)
		{
			_asm \
			{
						;x = (int)((clockspeed*sintable[(bulang[i]+512)&2047])>>13);
				mov bx, i
				shl bx, 1
				mov bx, word ptr bulang[bx]
				add bx, 512
				mov di, bx
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov ax, word ptr sintable[bx]
				mov bx, clockspeed
				mul bx
				and ax, 0xe000
				and dx, 0x1fff
				or ax, dx
				mov cl, 3
				rol ax, cl
				test di, 1024
				jz skipnegatebulxval
				neg ax
skipnegatebulxval:
				mov x, ax
									  ;y = (int)((clockspeed*sintable[bulang[i]])>>13);
				mov bx, i
				shl bx, 1
				mov bx, word ptr bulang[bx]
				mov di, bx
				and bx, 1023
				shl bx, 1
				shl bx, 1
				mov ax, word ptr sintable[bx]
				mov bx, clockspeed
				mul bx
				and ax, 0xe000
				and dx, 0x1fff
				or ax, dx
				mov cl, 3
				rol ax, cl
				test di, 1024
				jz skipnegatebulyval
				neg ax
skipnegatebulyval:
				mov y, ax
			}
			if (bulkind[i] == 15)
			{
				x -= (x>>1);
				y -= (y>>1);
			}
			if (bulkind[i] == 17)
			{
				x += (x>>1);
				y += (y>>1);
			}
			for(m=0;m<4;m++)
				if (bulstat[i] > 0)
				{
					bulx[i] += x;
					buly[i] += y;
					if ((bultype[bulkind[i]] == 2) && (death == 4095) && (m < 4))
					{
						l = labs((long)bulx[i]-(long)posx)+labs((long)buly[i]-(long)posy);
						if (l < 768)
						{
							if (greentime < totalclock)
							{
								if (capetime[0] < totalclock)
								{
									n = 128;
									if (bulkind[i] == 7) n = 320;
									if (bulkind[i] == 9) n = 192;
									if ((bulkind[i] >= 15) && (bulkind[i] <= 17)) n = 320;
									if (bulkind[i] == 13) n = 320;
									if (purpletime >= totalclock)
										n >>= 1;
									life -= n;
									if (life <= 0)
									{
										life = 0;
										drawlife();
										death = 4094;
										angvel = (rand()&32)-16;
										ksay(5);
										musicoff();
									}
									else
									{
										drawlife();
										angvel = (rand()&32)-16;
										fadehurtval += 16;
										ksay(14);
									}
								}
								bulnum--;
								bulx[i] = bulx[bulnum];
								buly[i] = buly[bulnum];
								bulang[i] = bulang[bulnum];
								bulstat[i] = bulstat[bulnum];
								bulkind[i] = bulkind[bulnum];
								m = 4;
							}
							else
							{
								do
								{
									l = labs((long)bulx[i]-(long)posx)+labs((long)buly[i]-(long)posy);
									bulx[i] -= x;
									buly[i] -= y;
								}
								while (l < 768);
								bulang[i] = ((bulang[i]+1024)&2047);
								m = 4;
								switch(bulkind[i])
								{
									case 5: case 7: case 9: case 11: case 13:
									case 22: case 24: bulkind[i]++; break;
								}
								bulstat[i] = totalclock+240;
							}
						}
					}
					j = board[bulx[i]>>10][buly[i]>>10]&16384;
					if ((j > 0) && (bultype[bulkind[i]] == 1) && (m < 4))
					{
						l = 0;
						for(k=0;k<mnum;k++)
							if (labs((long)bulx[i]-(long)mposx[k])+labs((long)buly[i]-(long)mposy[k]) < 768)
							{
								if ((mstat[k] == monan2) && (mshock[k] > 0) && (mshot[k] < 32))
								{
									while (abs((int)(bulx-mposx[k]))+abs((int)(buly[i]-mposy[k])) < 768)
									{
										bulx[i] -= x;
										buly[i] -= y;
									}
									bulang[i] = ((bulang[i]+1024)&2047);
									if (bulkind[i] < 5)
										bulkind[i] += 17;
									ksay(2);
									m = 4;
								}
								else if ((mstat[k] != monhol) && (((mstat[k] != monke2) && (mstat[k] != monzor) && (mstat[k] != monan3)) || (mshock[k] == 0)))
								{
									mshot[k]--;
									if ((purpletime >= totalclock) && (mshot[k] > 0))
										mshot[k]--;
									if (bulkind[i] == 3)
									{
										if (mshot[k] > 0)
											mshot[k]--;
										else
											l |= 4;
									}
									if (mshot[k] > 0)
									{
										j = 0;
										if ((mstat[k] == monan2) || (mstat[k] == monke2))
										{
											if (mshot[k] < 32)
												j = 64+(rand()&255);
											else
												j = 45;
										}
										switch(mstat[k])
										{
											case monrob: j = 0; break;
											case monzor: j = 45; break;
											case monand: case monali: j = 60; break;
											case mongre: case mongr2:
											case monwit: j = 80; break;
										}
										if ((mstat[k] != monbal) && (mstat[k] != mongho))
											l |= 2;
										if ((mshock[k] >= 0) && (mshock[k] < 8192))
										{
											mshock[k] += j;
											if (mshock[k] >= 8192)
												mshock[k] = 8191;
										}
									}
									if (mshot[k] == 0)
									{
										if (mstat[k] == hive)
											mshock[k] = 16384;
										if ((mstat[k] == monbal) || (mstat[k] == mongho) || (mstat[k] == hive) || (mstat[k] == mondog))
										{
											if (mstat[k] != hive)
												mshot[k] = 1;
											else
												ksay(30);
											if (mstat[k] != mondog)
												l |= 4;
											else
												ksay(32);
											bulnum--;
											bulx[i] = bulx[bulnum];
											buly[i] = buly[bulnum];
											bulang[i] = bulang[bulnum];
											bulstat[i] = bulstat[bulnum];
											bulkind[i] = bulkind[bulnum];
											m = 4;
										}
										else
										{
											l |= 1;
											if ((mstat[k] == monan3) || (mstat[k] == monhol))
												checkobj(mposx[k],mposy[k],posx,posy,ang,miniexplosion);
											else
												checkobj(mposx[k],mposy[k],posx,posy,ang,explosion);
											if (mstat[k] == monzor)
											{
												if (boardnum == 9)
												{
													xwarp[2] = xwarp[1];
													ywarp[2] = ywarp[1];
													j = (mposx[k]>>10);
													jj = (mposy[k]>>10);
													l = 0;
													do
													{
														xwarp[3] = (char)j+(rand()&31)-16;
														ywarp[3] = (char)jj+(rand()&31)-16;
														l++;
													}
													while ((board[xwarp[3]][ywarp[3]] != 1024) || ((abs(((int)xwarp[3])-j)+abs(((int)ywarp[3])-jj) < 16) && (l < 16)));
													board[xwarp[3]][ywarp[3]] = warp+1024;
													xwarp[1] = (char)j;
													ywarp[1] = (char)jj;
													board[j][jj] = warp+1024;
													numwarps = 4;
													mposx[mnum] = ((mposx[k]>>10)<<10)+512;
													mposy[mnum] = ((mposy[k]>>10)<<10)+512;
													mgolx[mnum] = mposx[mnum];
													mgoly[mnum] = mposy[mnum];
													moldx[mnum] = mposx[mnum];
													moldy[mnum] = mposy[mnum];
													mstat[mnum] = mondog;
													mshock[mnum] = 0;
													mshot[mnum] = 1;
													mnum++;
													board[mposx[k]>>10][mposy[k]>>10] |= 0x4000;
													justwarped = 1;
													if (cheated != 0)
														board[54][20] = youcheated;
												}
												if (boardnum == 29)
												{
													j = (mposx[k]>>10);
													jj = (mposy[k]>>10);
													xwarp[3] = (char)j;
													ywarp[3] = (char)jj;
													numwarps = 4;
													board[j][jj] = warp+1024;
													justwarped = 1;
												}
											}
											if (mstat[k] == monan2)
											{
												j = (mposx[k]>>10);
												jj = (mposy[k]>>10);
												board[j][jj] = silverkey+1024;
											}
											if (mstat[k] == monke2)
											{
												j = (mposx[k]>>10);
												jj = (mposy[k]>>10);
												board[j][jj] = goldkey+1024;
											}
											switch(mstat[k])
											{
												case monbat:
												case monear:
												case monbee: scorecount += 50; l |= 16; break;
												case monspi: scorecount += 50; l |= 8; break;
												case monken:
												case monro2: scorecount += 100; break;
												case monske:
												case monmum: scorecount += 100; l |= 8; break;
												case mongre:
												case mongr2:
												case monrob:
												case monwit:
												case monbal:
												case monan3: scorecount += 250; break;
												case monand: scorecount += 500; break;
												case monali: scorecount += 1000; break;
												case monhol: scorecount += 1500; break;
												case mongho: scorecount += 2500; break;
												case monzor:
												case monke2:
												case monan2: scorecount += 5000; break;
												case mondog: scorecount >>= 1; break;
											}
											drawscore(scorecount);
											mnum--;
											board[moldx[k]>>10][moldy[k]>>10] &= 0xbfff;
											board[mgolx[k]>>10][mgoly[k]>>10] &= 0xbfff;
											moldx[k] = moldx[mnum];
											moldy[k] = moldy[mnum];
											mposx[k] = mposx[mnum];
											mposy[k] = mposy[mnum];
											mgolx[k] = mgolx[mnum];
											mgoly[k] = mgoly[mnum];
											mstat[k] = mstat[mnum];
											mshot[k] = mshot[mnum];
											mshock[k] = mshock[mnum];
										}
									}
								}
							}
						if ((l > 0) && (m != 4))
						{
							if ((l&4) == 0)
							{
								bulnum--;
								bulx[i] = bulx[bulnum];
								buly[i] = buly[bulnum];
								bulang[i] = bulang[bulnum];
								bulstat[i] = bulstat[bulnum];
								bulkind[i] = bulkind[bulnum];
							}
							else
								bulkind[i] = 4;
							m = 4;
							if (death == 4095)
							{
								if ((l&1) > 0)
								{
									if ((l&8) == 0)
									{
										if ((l&16) == 0)
											ksay(1);
										else
											ksay(0);
									}
									else
										ksay(30);
								}
								if ((l&2) > 0)
									ksay(8);
							}
						}
					}
					j = board[bulx[i]>>10][buly[i]>>10];
					jj = (j&1023);
					if ((bmpkind[jj] != 0) && (((bulx[i]|1023) != (posx|1023)) || ((buly[i]|1023) != (posy|1023))) && (death == 4095))
					{
						if ((bultype[bulkind[i]] == 1) && ((jj == kenface) || (jj == kenfaceouch)) && (m < 4))
						{
							if (jj == kenface)
								board[bulx[i]>>10][buly[i]>>10] = kenfaceouch;
							life -= 64;
							if (life <= 0)
							{
								life = 0;
								drawlife();
								death = 4094;
								angvel = (rand()&32)-16;
								ksay(5);
								musicoff();
							}
							else
							{
								drawlife();
								angvel = (rand()&32)-16;
								fadehurtval += 16;
								ksay(14);
							}
							bulnum--;
							bulx[i] = bulx[bulnum];
							buly[i] = buly[bulnum];
							bulang[i] = bulang[bulnum];
							bulstat[i] = bulstat[bulnum];
							bulkind[i] = bulkind[bulnum];
							m = 4;
						}
						if ((bultype[bulkind[i]] == 1) && (jj == andy) && (m < 4))
						{
							if (mnum < 512)
							{
								mposx[mnum] = ((bulx[i]>>10)<<10)+512;
								mposy[mnum] = ((buly[i]>>10)<<10)+512;
								mgolx[mnum] = mposx[mnum];
								mgoly[mnum] = mposy[mnum];
								moldx[mnum] = mposx[mnum];
								moldy[mnum] = mposy[mnum];
								mstat[mnum] = monand;
								mshock[mnum] = 0;
								mshot[mnum] = 10;
								mnum++;
								board[bulx[i]>>10][buly[i]>>10] |= 0x4000;
								if ((rand()&7) == 0)
								{
									board[bulx[i]>>10][buly[i]>>10] &= 0xfc00;
									board[bulx[i]>>10][buly[i]>>10] |= andygone;
								}
							}
							bulnum--;
							bulx[i] = bulx[bulnum];
							buly[i] = buly[bulnum];
							bulang[i] = bulang[bulnum];
							bulstat[i] = bulstat[bulnum];
							bulkind[i] = bulkind[bulnum];
							m = 4;
						}
						if ((bultype[bulkind[i]] == 1) && (((j&0xfff) == target) || ((j&0xfff) == clock)))
						{
							if ((j&0xfff) == target) ksay(3);
							if ((j&0xfff) == clock) ksay(31);
							bulnum--;
							bulx[i] = bulx[bulnum];
							buly[i] = buly[bulnum];
							bulang[i] = bulang[bulnum];
							bulstat[i] = bulstat[bulnum];
							bulkind[i] = bulkind[bulnum];
							m = 4;
						}
						if (((wallheader[jj]&8) == 0) && (jj != net) && (m < 4))
						{
							n = 0;
							if ((bmpkind[jj] == 1) || (bmpkind[jj] == 4))
								n = 1;
							if ((n == 1) || ((((bulx[i]&1023)<<2) > lborder[jj]) && (((bulx[i]&1023)<<2) < (4096-lborder[jj])) && (((buly[i]&1023)<<2) > lborder[jj]) && (((buly[i]&1023)<<2) < (4096-lborder[jj]))))
							{
								if (((j&2048) > 0) && (bultype[bulkind[i]] == 1))
								{
									ksay(10);
									checkobj(bulx[i],buly[i],posx,posy,ang,explosion);
									if (jj == bricksfull)
										board[bulx[i]>>10][buly[i]>>10] = brickshalf+(j&0xfc00);
									else if ((j&1024) == 0)
										board[bulx[i]>>10][buly[i]>>10] = 1024;
									else
										board[bulx[i]>>10][buly[i]>>10] ^= 2048;
									scorecount += 750;
									drawscore(scorecount);
									bulnum--;
									bulx[i] = bulx[bulnum];
									buly[i] = buly[bulnum];
									bulang[i] = bulang[bulnum];
									bulstat[i] = bulstat[bulnum];
									bulkind[i] = bulkind[bulnum];
									m = 4;
								}
								if (((purpletime < totalclock) || (bulkind[i] == 2) || (bulkind[i] >= 5)) && (m < 4))
								{
									if ((bulkind[i] == 2) || (bulkind[i] == 22))
									{
										while ((wallheader[board[bulx[i]>>10][buly[i]>>10]&1023]&8) == 0)
										{
											bulx[i] -= x;
											buly[i] -= y;
										}
										newx = bulx[i]+x;
										newy = buly[i]+y;
										oposx = bulx[i];
										oposy = buly[i];
										k = 0;
										if ((newx&0xfc00) < (oposx&0xfc00))
										{
											plcx = (oposx&0xfc00);
											plcy = oposy+(int)((((long)oposx-(long)plcx)*tantable[bulang[i]&1023])>>16);
											if ((wallheader[board[(plcx>>10)-1][plcy>>10]&1023]&8) == 0)
												k |= 1;
											if ((wallheader[board[(plcx>>10)-1][plcy>>10]&1023]&8) == 0)
												k |= 1;
										}
										if ((newx&0xfc00) > (oposx&0xfc00))
										{
											plcx = (oposx&0xfc00)+1023;
											plcy = oposy+(int)((((long)oposx-(long)plcx)*tantable[bulang[i]&1023])>>16);
											if ((wallheader[board[(plcx>>10)+1][plcy>>10]&1023]&8) == 0)
												k |= 1;
											if ((wallheader[board[(plcx>>10)+1][plcy>>10]&1023]&8) == 0)
												k |= 1;
										}
										if ((newy&0xfc00) < (oposy&0xfc00))
										{
											plcy = (oposy&0xfc00);
											plcx = oposx+(int)((((long)oposy-(long)plcy)*tantable[(2560-bulang[i])&1023])>>16);
											if ((wallheader[board[plcx>>10][(plcy>>10)-1]&1023]&8) == 0)
												k |= 2;
											if ((wallheader[board[plcx>>10][(plcy>>10)-1]&1023]&8) == 0)
												k |= 2;
										}
										if ((newy&0xfc00) > (oposy&0xfc00))
										{
											plcy = (oposy&0xfc00)+1023;
											plcx = oposx+(int)((((long)oposy-(long)plcy)*tantable[(2560-bulang[i])&1023])>>16);
											if ((wallheader[board[plcx>>10][(plcy>>10)+1]&1023]&8) == 0)
												k |= 2;
											if ((wallheader[board[plcx>>10][(plcy>>10)+1]&1023]&8) == 0)
												k |= 2;
										}
										if ((k&1) > 0)
											bulang[i] = ((3072-bulang[i])&2047);
										if ((k&2) > 0)
											bulang[i] = ((2048-bulang[i])&2047);
										m = 4;
									}
									else
									{
										bulnum--;
										bulx[i] = bulx[bulnum];
										buly[i] = buly[bulnum];
										bulang[i] = bulang[bulnum];
										bulstat[i] = bulstat[bulnum];
										bulkind[i] = bulkind[bulnum];
										m = 4;
									}
								}
							}
						}
						if ((jj == net) && (m < 4))
						{
							while ((board[bulx[i]>>10][buly[i]>>10]&1023) == net)
							{
								bulx[i] -= x;
								buly[i] -= y;
							}
							bulang[i] = ((bulang[i]+1024)&2047);
							m = 4;
							if (bulkind[i] < 5)
							{
								if (death == 4095)
									ksay(2);
								bulkind[i] += 17;
								bulstat[i] = totalclock+240;
							}
						}
					}
				}
		}
		i = 0;                               //scan vicinity
		mrotbuf[0] = ((posx>>10)<<6)+(posy>>10);
		tempbuf[mrotbuf[0]] = 20;
		j = 1;                               //j is stop (end of nodes)
		_asm \
		{
			mov bx, i
			shl bx, 1
			mov bx, word ptr mrotbuf[bx]
begscan:
			mov si, bx
			sub si, 64
			cmp byte ptr tempbuf[si], 0
			jne skipscan1
			mov di, si
			shl di, 1
			mov ax, word ptr board[di]
			and ax, 0x0c00
			cmp ax, 1024
			jne skipscan1
			mov di, j
			shl di, 1
			mov word ptr mrotbuf[di], si
			mov al, byte ptr tempbuf[bx]
			dec al
			mov byte ptr tempbuf[si], al
			inc byte ptr j[0]
skipscan1:
			mov si, bx
			add si, 64
			cmp byte ptr tempbuf[si], 0
			jne skipscan2
			mov di, si
			shl di, 1
			mov ax, word ptr board[di]
			and ax, 0x0c00
			cmp ax, 1024
			jne skipscan2
			mov di, j
			shl di, 1
			mov word ptr mrotbuf[di], si
			mov al, byte ptr tempbuf[bx]
			dec al
			mov byte ptr tempbuf[si], al
			inc byte ptr j[0]
skipscan2:
			mov si, bx
			dec si
			cmp byte ptr tempbuf[si], 0
			jne skipscan3
			mov di, si
			shl di, 1
			mov ax, word ptr board[di]
			and ax, 0x0c00
			cmp ax, 1024
			jne skipscan3
			mov di, j
			shl di, 1
			mov word ptr mrotbuf[di], si
			mov al, byte ptr tempbuf[bx]
			dec al
			mov byte ptr tempbuf[si], al
			inc byte ptr j[0]
skipscan3:
			mov si, bx
			inc si
			cmp byte ptr tempbuf[si], 0
			jne skipscan4
			mov di, si
			shl di, 1
			mov ax, word ptr board[di]
			and ax, 0x0c00
			cmp ax, 1024
			jne skipscan4
			mov di, j
			shl di, 1
			mov word ptr mrotbuf[di], si
			mov al, byte ptr tempbuf[bx]
			dec al
			mov byte ptr tempbuf[si], al
			inc byte ptr j[0]
skipscan4:
			inc byte ptr i[0]
			mov bx, i
			cmp bx, j
			je endscan
			shl bx, 1
			mov bx, word ptr mrotbuf[bx]
			cmp byte ptr tempbuf[bx], 0
			jle endscan
			jmp begscan
endscan:
		}
		for(i=0;i<mnum;i++)
		{
			if (mposx[i] > posx) templong = (long)(mposx[i]-posx);
			else templong = (long)(posx-mposx[i]);
			if (mposy[i] > posy) templong += (long)(mposy[i]-posy);
			else templong += (long)(posy-mposy[i]);
			if (templong < 16384)
			{
				x = (mposx[i]>>10);
				y = (mposy[i]>>10);
				switch(mstat[i])
				{
					case mongho: j = clockspeed; break;
					case monhol:
					case monro2: j = (clockspeed<<2); break;
					case mondog:
						if (templong < 3072)
							j = (clockspeed<<2);
						else if (templong < 6144)
							j = (clockspeed<<4);
						else
							j = (clockspeed<<5)+(clockspeed<<3);
						break;
					case monken:
					case monmum:
					case monske: j = (clockspeed<<2)+(clockspeed<<1); break;
					case monbal:
					case monke2:
					case monan2: j = (clockspeed<<3); break;
					case mongre:
					case mongr2:
					case monrob:
					case monwit:
					case monear: j = (clockspeed<<3)+(clockspeed<<1); break;
					case monand:
					case monbat:
					case monbee:
					case monspi: j = (clockspeed<<3)+(clockspeed<<2); break;
					case monali:
					case monan3: j = (clockspeed<<4); break;
					case monzor:
						j = (clockspeed<<3);
						if ((mshock[i]&8192) > 0)
							j += (clockspeed<<4);
						break;
					case hive:
						j = 0;
						if ((mshock[i] == 0) && (mnum < 512) && ((rand()&1023) <= clockspeed))
						{
							mposx[mnum] = (mposx[i]&0xfc00)+512;
							mposy[mnum] = (mposy[i]&0xfc00)+512;
							mgolx[mnum] = mposx[mnum];
							mgoly[mnum] = mposy[mnum];
							moldx[mnum] = mposx[mnum];
							moldy[mnum] = mposy[mnum];
							mstat[mnum] = monbee;
							mshock[mnum] = 0;
							mshot[mnum] = 1;
							board[x][y] |= 0x4000;
							mnum++;
						}
						break;
				}
				if ((mshock[i]&8192) > 0)
					if (mshock[i] > 8192)
						mshock[i]--;
				if ((mshock[i]&16384) > 0)
					if (mshock[i] < 24575)
						mshock[i]++;
				if ((mshock[i] > 0) && (mshock[i] < 8192))
				{
					mshock[i] -= clockspeed;
					if (mshock[i] < 0)
						mshock[i] = 0;
				}
				if ((mshock[i] == 0) || (mstat[i] == monke2) || (mstat[i] == monzor) || (mstat[i] == monan3))
				{
					if (mgolx[i] > mposx[i])
					{
						mposx[i] += j;
						if (mposx[i] > mgolx[i]) mposx[i] = mgolx[i];
					}
					else if (mgolx[i] < mposx[i])
					{
						mposx[i] -= j;
						if (mposx[i] < mgolx[i]) mposx[i] = mgolx[i];
					}
					if (mgoly[i] > mposy[i])
					{
						mposy[i] += j;
						if (mposy[i] > mgoly[i]) mposy[i] = mgoly[i];
					}
					else if (mgoly[i] < mposy[i])
					{
						mposy[i] -= j;
						if (mposy[i] < mgoly[i]) mposy[i] = mgoly[i];
					}
				}
				if ((templong < 768) && (death == 4095) && (mstat[i] != mondog))
				{
					if ((mstat[i] == monhol) && (mshock[i] > 0))
					{
						life = 0;
						drawlife();
						death = 4094;
						angvel = 0;
						ksay(6);
						musicoff();
					}
					else if (capetime[1] < totalclock)
					{
						if (capetime[0] < totalclock)
						{
							if ((mstat[i] == monan2) && (mshock[i] > 0) && (mshock[i] < 8192) && (mshot[i] < 32))
							{
								life = 0;
								drawlife();
								death = 4094;
								angvel = (rand()&32)-16;
								ksay(5);
								musicoff();
							}
							else if (mshock[i] == 0)
							{
								life -= 16+(clockspeed<<2);
								if (life <= 0)
								{
									life = 0;
									drawlife();
									death = 4094;
									angvel = (rand()&32)-16;
									ksay(5);
									musicoff();
								}
								else
								{
									drawlife();
									angvel = (rand()&32)-16;
									fadehurtval += 16;
									ksay(14);
								}
							}
						}
					}
					else
					{
						if ((mstat[i] != monzor) && (mstat[i] != monke2) && (mstat[i] != monan2) && (mstat[i] != monan3))
						{
							if ((mstat[i] == monan3) || (mstat[i] == monhol))
								checkobj(mposx[i],mposy[i],posx,posy,ang,miniexplosion);
							else
								checkobj(mposx[i],mposy[i],posx,posy,ang,explosion);
							mnum--;
							board[moldx[i]>>10][moldy[i]>>10] &= 0xbfff;
							board[mgolx[i]>>10][mgoly[i]>>10] &= 0xbfff;
							moldx[i] = moldx[mnum];
							moldy[i] = moldy[mnum];
							mposx[i] = mposx[mnum];
							mposy[i] = mposy[mnum];
							mgolx[i] = mgolx[mnum];
							mgoly[i] = mgoly[mnum];
							mstat[i] = mstat[mnum];
							mshot[i] = mshot[mnum];
							mshock[i] = mshock[mnum];
							ksay(30);
						}
					}
				}
				if ((mposx[i] == mgolx[i]) && (mposy[i] == mgoly[i]) && ((mshock[i] == 0) || (mstat[i] == monke2) || (mstat[i] == monzor) || (mstat[i] == monan3)))
				{
					if ((board[mposx[i]>>10][mposy[i]>>10]&1023) == hole)
					{
						if (mstat[i] == monbal)
							checkobj(mposx[i],mposy[i],posx,posy,ang,monbal+4);
						mnum--;
						board[moldx[i]>>10][moldy[i]>>10] &= 0xbfff;
						board[mgolx[i]>>10][mgoly[i]>>10] &= 0xbfff;
						moldx[i] = moldx[mnum];
						moldy[i] = moldy[mnum];
						mposx[i] = mposx[mnum];
						mposy[i] = mposy[mnum];
						mgolx[i] = mgolx[mnum];
						mgoly[i] = mgoly[mnum];
						mstat[i] = mstat[mnum];
						mshot[i] = mshot[mnum];
						mshock[i] = mshock[mnum];
						ksay(6);
					}
					else
					{
						if (mstat[i] == monhol)
						{
							k = 4;
							do
							{
								j = (rand()&3);
								if ((j == 0) && ((board[x-1][y]&0x4c00) == 1024))
									mgolx[i] = mposx[i]-1024;
								if ((j == 1) && ((board[x+1][y]&0x4c00) == 1024))
									mgolx[i] = mposx[i]+1024;
								if ((j == 2) && ((board[x][y-1]&0x4c00) == 1024))
									mgoly[i] = mposy[i]-1024;
								if ((j == 3) && ((board[x][y+1]&0x4c00) == 1024))
									mgoly[i] = mposy[i]+1024;
								k--;
							}
							while ((k > 0) && (mposx[i] == mgolx[i]) && (mposy[i] == mgoly[i]));
							mshock[i] = 480;
						}
						else if (mstat[i] != hive)
						{
							m = (x<<6)+y;
							n = tempbuf[m];
							k = 4;
							do
							{
								j = rand()&3;
								switch(j)
								{
									case 0:
										if ((tempbuf[m-64] > n) && ((board[x-1][y]&0x4c00) == 1024))
											mgolx[i] = mposx[i]-1024;
										break;
									case 1:
										if ((tempbuf[m+64] > n) && ((board[x+1][y]&0x4c00) == 1024))
											mgolx[i] = mposx[i]+1024;
										break;
									case 2:
										if ((tempbuf[m-1] > n) && ((board[x][y-1]&0x4c00) == 1024))
											mgoly[i] = mposy[i]-1024;
										break;
									case 3:
										if ((tempbuf[m+1] > n) && ((board[x][y+1]&0x4c00) == 1024))
											mgoly[i] = mposy[i]+1024;
										break;
								}
								k--;
							}
							while ((k > 0) && (mposx[i] == mgolx[i]) && (mposy[i] == mgoly[i]));
						}
						if ((mstat[i] == monzor) || (mstat[i] == monke2) || (mstat[i] == monan2))
						{
							if (mstat[i] == monzor) j = 5000;
							if (mstat[i] == monke2) j = 4000;
							if (mstat[i] == monan2) j = 3000;
							if (labs((long)mposx[i]-(long)posx)+labs((long)mposy[i]-(long)posy) < j)
							{
								if ((rand()&1) == 0)
								{
									if ((posx > mposx[i]) && ((board[x-1][y]&0x4c00) == 1024))
										mgolx[i] = mposx[i]-1024, mgoly[i] = mposy[i];
									if ((posx < mposx[i]) && ((board[x+1][y]&0x4c00) == 1024))
										mgolx[i] = mposx[i]+1024, mgoly[i] = mposy[i];
								}
								else
								{
									if ((posy > mposy[i]) && ((board[x][y-1]&0x4c00) == 1024))
										mgoly[i] = mposy[i]-1024, mgolx[i] = mposx[i];
									if ((posy < mposy[i]) && ((board[x][y+1]&0x4c00) == 1024))
										mgoly[i] = mposy[i]+1024, mgolx[i] = mposx[i];
								}
							}
						}
						if (((mgolx[i]&0xfc00) == (posx&0xfc00)) && ((mgoly[i]&0xfc00) == (posy&0xfc00)) && ((skilevel == 0) || (mstat[i] == mondog)))
						{
							mgolx[i] = moldx[i];
							mgoly[i] = moldy[i];
						}
						board[moldx[i]>>10][moldy[i]>>10] &= 0xbfff;
						if (mstat[i] == 0)
						{
							board[mposx[i]>>10][mposy[i]>>10] &= 0xbfff;
							board[mgolx[i]>>10][mgoly[i]>>10] &= 0xbfff;
						}
						else
						{
							board[mposx[i]>>10][mposy[i]>>10] |= 0x4000;
							board[mgolx[i]>>10][mgoly[i]>>10] |= 0x4000;
						}
						moldx[i] = mposx[i];
						moldy[i] = mposy[i];
					}
				}
				j = rand()&2047;
				switch(mstat[i])
				{
					case mongre:
					case mongr2:
					case monro2: j &= 1023; break;
					case monand:
					case monrob:
					case monwit: j &= 511; break;
					case monali:
					case monzor:
					case mongho:
					case monan2:
					case monke2:
					case monan3: j &= 127; break;
				}
				if ((j < clockspeed) && (bulnum < 64) && (mshock[i] == 0))
				{
					if ((mstat[i] != monbal) && (mstat[i] != monhol) && (mstat[i] != monbat) && (mstat[i] != monbee) && (mstat[i] != monspi) && (mstat[i] != hive) && (mstat[i] != mondog))
					{
						bulx[bulnum] = mposx[i];
						buly[bulnum] = mposy[i];
						k = 512;
						if (mposx[i] != posx)
						{
							templong = (((((long)mposy[i]-(long)posy)<<12)/((long)mposx[i]-(long)posx))<<4);
							if (templong < 0)
								k = 768;
							else
								k = 256;
							for (m=128;m>0;m>>=1)
							{
								if (tantable[k] < templong)
									k += m;
								else
									k -= m;
							}
						}
						if (mposy[i] > posy)
							k += 1024;
						bulang[bulnum] = ((k+2016+(rand()&63))&2047);
						bulstat[bulnum] = totalclock+240;
						if (mstat[i] == monand) bulstat[bulnum] += 240;
						if (mstat[i] == monali) bulstat[bulnum] += 480;
						bulkind[bulnum] = 5;
						if (mstat[i] == monzor) bulkind[bulnum] = 7;
						if (mstat[i] == monali) bulkind[bulnum] = 9;
						if (mstat[i] == monke2)
						{
							if (mshot[i] >= 32)
								bulkind[bulnum] = 17;
							else if (mshot[i] >= 16)
								bulkind[bulnum] = 16+(rand()&1);
							else
								bulkind[bulnum] = 15+(rand()%3);
						}
						if (mstat[i] == monan2) bulkind[bulnum] = 13;
						if (mstat[i] == monan3) bulkind[bulnum] = 11;
						if (mstat[i] == monwit) bulkind[bulnum] = 22;
						if ((mstat[i] == monrob) || (mstat[i] == monro2))
							bulkind[bulnum] = 24;
						bulnum++;
					}
				}
				if (mstat[i] == monzor)
					if ((mshot[i] < 24) && (mshock[i] == 0) && ((rand()&1023) <= clockspeed))
						mshock[i] = 16384+monzor+6;
				if (mstat[i] == monke2)
					if ((mshot[i] < 16) && (mshock[i] == 0) && ((rand()&255) <= clockspeed))
						mshock[i] = 16384+monke2+7;
				if (mstat[i] == monan3)
					if ((mshock[i] == 0) && ((rand()&511) <= clockspeed))
						mshock[i] = 16384+monan3+3;
				if (mstat[i] == monan2)
					if ((mshot[i] < 24) && (mnum < 512) && ((rand()&1023) <= clockspeed))
					{
						mposx[mnum] = (mposx[i]&0xfc00)+512;
						mposy[mnum] = (mposy[i]&0xfc00)+512;
						mgolx[mnum] = mposx[mnum];
						mgoly[mnum] = mposy[mnum];
						moldx[mnum] = mposx[mnum];
						moldy[mnum] = mposy[mnum];
						mstat[mnum] = monan3;
						mshock[mnum] = monan3+8192+8;
						mshot[mnum] = 1;
						board[mposx[mnum]>>10][mposy[mnum]>>10] |= 16384;
						mnum++;
					}
			}
		}
		if (death == 4095)
		{
			mousx = 0;
			mousy = 0;
			bstatus = 0;
			if (joystat == 0)
			{
				_asm \
				{
					mov dx, 0x201
					xor al, al
					out dx, al
					xor si, si
					xor di, di
					mov cx, 0xffff
joystickloop:
					in al, dx
					mov bl, al
					mov bh, al
					and bx, 0x0201
					add bl, 0xff
					adc si, 0
					add bh, 0xff
					adc di, 0
					and al, 3
					loopnz joystickloop
endjoystickloop:
					mov mousx, si
					mov mousy, di
					in al, dx
					mov cl, 4
					shr al, cl
					and ax, 3
					xor al, 3
					mov bstatus, ax
				}
				if ((mousx == -1) && (mousy == -1))
					joystat = 1;
				else
				{
					if (mousx < joyx2)
						mousx = (int)((((long)(mousx-joyx2))<<6)/((long)(joyx2-joyx1)));
					else
						mousx = (int)((((long)(mousx-joyx2))<<6)/((long)(joyx3-joyx2)));
					if (mousy < joyy2)
						mousy = (int)((((long)(mousy-joyy2))<<5)/((long)(joyy2-joyy1)));
					else
						mousy = (int)((((long)(mousy-joyy2))<<5)/((long)(joyy3-joyy2)));
				}
			}
			if (moustat == 0)
				_asm \
				{
					mov ax, 11
					int 0x33
					add mousx, cx
					add mousy, dx
					mov ax, 5
					int 0x33
					or bstatus, ax
				}
			if (keystatus[keydefs[4]] == 0)
			{
				j = clockspeed;
				if (j > 16)
					j = 16;
				if (keystatus[keydefs[2]] > 0)
				{
					angvel -= j;
					if (angvel < -36)
						angvel = -36;
				}
				else
				{
					if (angvel < 0)
					{
						angvel += clockspeed;
						if (angvel > 0)
							angvel = 0;
					}
				}
				if (keystatus[keydefs[3]] > 0)
				{
					angvel += j;
					if (angvel > 36)
						angvel = 36;
				}
				else
				{
					if (angvel > 0)
					{
						angvel -= clockspeed;
						if (angvel < 0)
							angvel = 0;
					}
				}
				if (svel < 0)
				{
					svel += ((clockspeed<<1)+clockspeed);
					if (svel > 0)
						svel = 0;
				}
				if (svel > 0)
				{
					svel -= ((clockspeed<<1)+clockspeed);
					if (svel < 0)
						svel = 0;
				}
			}
			else
			{
				if (keystatus[keydefs[2]] > 0)
				{
					svel += (clockspeed<<2);
					if (svel > maxvel)
						svel = maxvel;
				}
				else
				{
					if (svel > 0)
					{
						svel -= ((clockspeed<<1)+clockspeed);
						if (svel < 0)
							svel = 0;
					}
				}
				if (keystatus[keydefs[3]])
				{
					svel -= (clockspeed<<2);
					if (svel < -maxvel)
						svel = -maxvel;
				}
				else
				{
					if (svel < 0)
					{
						svel += ((clockspeed<<1)+clockspeed);
						if (svel > 0)
							svel = 0;
					}
				}
				if (angvel < 0)
				{
					angvel += clockspeed;
					if (angvel > 0)
						angvel = 0;
				}
				if (angvel > 0)
				{
					angvel -= clockspeed;
					if (angvel < 0)
						angvel = 0;
				}
			}
		}
		if ((moustat == 0) || (joystat == 0))
		{
			if ((mousx < -2) || (mousx > 2))
			{
				if (mousx < -24)
					mousx = -24;
				if (mousx > 24)
					mousx = 24;
				mxvel = mousx;
				if (mxvel+angvel < -32)
					mxvel = -32-angvel;
				if (mxvel+angvel > 32)
					mxvel = 32-angvel;
			}
			else
			{
				if (mxvel < 0)
				{
					mxvel += (clockspeed<<1);
					if (mxvel > 0)
						mxvel = 0;
				}
				if (mxvel > 0)
				{
					mxvel -= (clockspeed<<1);
					if (mxvel < 0)
						mxvel = 0;
				}
			}
		}
		ang = (ang+2048+(((angvel+mxvel)*clockspeed)>>3))&2047;
		if (compass > 0)
			showcompass(ang);
		if ((keystatus[keydefs[5]] > 0) && (death == 4095))
		{
			hvel -= (clockspeed>>1);
			if (hvel < -3)
				hvel = -3;
		}
		else
		{
			if (hvel < 0)
			{
				hvel += (clockspeed>>2);
				if (hvel > 0)
					hvel = 0;
			}
		}
		if ((keystatus[keydefs[6]] > 0) && (death == 4095))
		{
			hvel += (clockspeed>>1);
			if (hvel > 3)
				hvel = 3;
		}
		else
		{
			if (hvel > 0)
			{
				hvel -= (clockspeed>>2);
				if (hvel < 0)
					hvel = 0;
			}
		}
		posz += ((hvel*clockspeed)>>3);
		if (posz < 8)
		{
			posz = 8;
			hvel = 0;
		}
		if (posz > 56)
		{
			posz = 56;
			hvel = 0;
		}
		waterstat = 0;
		if (((keystatus[keydefs[12]] > 0) || ((bstatus&2) > 0)) && (death == 4095))
		{
			x = (posx>>10);
			y = (posy>>10);
			brd0 = board[x-1][y]&1023;
			brd1 = board[x+1][y]&1023;
			brd2 = board[x][y-1]&1023;
			brd3 = board[x][y+1]&1023;
			if (lastunlock == 0)
			{
				i = 0;
				if ((brd0 == safe+1) && (((ang+512)&1024) > 0))
					i = 1, m = x-1, n = y, brd0 = safe+2;
				if ((brd1 == safe+1) && (((ang+512)&1024) == 0))
					i = 1, m = x+1, n = y, brd1 = safe+2;
				if ((brd2 == safe+1) && ((ang&1024) > 0))
					i = 1, m = x, n = y-1, brd2 = safe+2;
				if ((brd3 == safe+1) && ((ang&1024) == 0))
					i = 1, m = x, n = y+1, brd3 = safe+2;
				if (i == 1)
				{
					board[m][n] = (board[m][n]&0xfc00)+(safe+2);
					coins+=2;
					if (coins > 999)
						coins = 999;
					ksay(7);
					scorecount += 250;
					drawscore(scorecount);
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
					lastunlock = 1;
				}
				i = 0;
				if ((brd0 == safe) && (((ang+512)&1024) > 0))
					i = 1, m = x-1, n = y;
				if ((brd1 == safe) && (((ang+512)&1024) == 0))
					i = 1, m = x+1, n = y;
				if ((brd2 == safe) && ((ang&1024) > 0))
					i = 1, m = x, n = y-1;
				if ((brd3 == safe) && ((ang&1024) == 0))
					i = 1, m = x, n = y+1;
				if (i == 1)
				{
					ksay(16);
					i = rand()&3;
					if (i == 0)
						board[m][n] = (board[m][n]&0xfc00)+(safe+1);
					else
						board[m][n] = (board[m][n]&0xfc00)+(safe+2);
					lastunlock = 1;
				}
			}
			i = waterstat;
			if ((brd0 == fountain) && (((ang+512)&1024) > 0))
				waterstat = 1, lastunlock = 1;
			if ((brd1 == fountain) && (((ang+512)&1024) == 0))
				waterstat = 1, lastunlock = 1;
			if ((brd2 == fountain) && ((ang&1024) > 0))
				waterstat = 1, lastunlock = 1;
			if ((brd3 == fountain) && ((ang&1024) == 0))
				waterstat = 1, lastunlock = 1;
			if (waterstat == 1)
				if ((totalclock^ototclock) > 127)
				{
					if (i == 0)
						ksay(28);
					life += 32;
					if (life > 4095)
						life = 4095;
					drawlife();
				}
			if ((coins > 0) && (lastunlock == 0))
			{
				i = 0;
				if (((ang+512)&1024) > 0)
					if (brd0 == soda)
					i = 1;
				if (((ang+512)&1024) == 0)
					if (brd1 == soda)
						i = 1;
				if ((ang&1024) > 0)
					if (brd2 == soda)
						i = 1;
				if ((ang&1024) == 0)
					if (brd3 == soda)
						i = 1;
				if (i == 1)
				{
					sodamenu();
					lastunlock = 1;
					lastbarchange = 1;
					lastshoot = 1;
					keystatus[1] = 0;
				}
				if (slottime == 0)
				{
					i = 0;
					if (((ang+512)&1024) > 0)
						if (brd0 == slotto)
							i = 1;
					if (((ang+512)&1024) == 0)
						if (brd1 == slotto)
							i = 1;
					if ((ang&1024) > 0)
						if (brd2 == slotto)
							i = 1;
					if ((ang&1024) == 0)
						if (brd3 == slotto)
							i = 1;
					if (i == 1)
					{
						ksay(24);
						coins--;
						lastunlock = 1;
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
						slottime = 512+(rand()&255);
						slotpos[0] = (rand()&127);
						slotpos[1] = (rand()&127);
						slotpos[2] = (rand()&127);
					}
				}
			}
			i = 0;
			if ((keys[0] > 0) || (keys[1] > 0))
			{
				if (((ang+512)&1024) > 0)
				{
					if ((brd0 == goldlock) && (keys[0] > 0))
						board[x-1][y] = 1024, i = 1;
					if ((brd0 == silverlock) && (keys[1] > 0))
						board[x-1][y] = 1024, i = 2;
				}
				if (((ang+512)&1024) == 0)
				{
					if ((brd1 == goldlock) && (keys[0] > 0))
						board[x+1][y] = 1024, i = 1;
					if ((brd1 == silverlock) && (keys[1] > 0))
						board[x+1][y] = 1024, i = 2;
				}
				if ((ang&1024) > 0)
				{
					if ((brd2 == goldlock) && (keys[0] > 0))
						board[x][y-1] = 1024, i = 1;
					if ((brd2 == silverlock) && (keys[1] > 0))
						board[x][y-1] = 1024, i = 2;
				}
				if ((ang&1024) == 0)
				{
					if ((brd3 == goldlock) && (keys[0] > 0))
						board[x][y+1] = 1024, i = 1;
					if ((brd3 == silverlock) && (keys[1] > 0))
						board[x][y+1] = 1024, i = 2;
				}
				if (i > 0)
				{
					ksay(16);
					if ((boardnum == 19) && (i == 2))
					{
						if (cheated == 0)
						{
							wingame(2);
							if (numboards <= 20)
							{
								fade(0);
								kgif(1);
								introduction(1);
							}
							else
								won = 1;
						}
						else
						{
							oldposx += 8192;
							posx += 8192;
						}
					}
					if ((boardnum == 29) && (i == 1) && (cheated != 0))
					{
						oldposx += 16384;
						posx += 16384;
					}
				}
			}
			if (i == 0)
			{
				if (doorstat == 0)
				{
					k = board[x-1][y];
					if (((k&16384) == 0) && (((ang+512)&1024) > 0))
					{
						if (((k&1023) == door1) || ((k&1023) == door1+5))
						{
							doorx = x-1, doory = y, doorstat = door1;
							if ((posx&1023) < 256)
								posx = (posx&0xfc00)+256;
						}
						if (((k&1023) == door2) || ((k&1023) == door2+5))
						{
							doorx = x-1, doory = y, doorstat = door2;
							if ((posx&1023) < 256)
								posx = (posx&0xfc00)+256;
						}
						if (((k&1023) == door3) || ((k&1023) == door3+7))
						{
							doorx = x-1, doory = y, doorstat = door3;
							if ((posx&1023) < 256)
								posx = (posx&0xfc00)+256;
						}
						if ((((k&1023) == door4) && (coins >= 10)) || ((k&1023) == door4+6))
						{
							doorx = x-1, doory = y, doorstat = door4;
							if ((posx&1023) < 256)
								posx = (posx&0xfc00)+256;
						}
						if (((k&1023) == door5) || ((k&1023) == door5+7))
						{
							doorx = x-1, doory = y, doorstat = door5;
							if ((posx&1023) < 256)
								posx = (posx&0xfc00)+256;
						}
					}
					k = board[x+1][y];
					if (((k&16384) == 0) && (((ang+512)&1024) == 0))
					{
						if (((k&1023) == door1) || ((k&1023) == door1+5))
						{
							doorx = x+1, doory = y, doorstat = door1;
							if ((posx&1023) > 767)
								posx = (posx&0xfc00)+767;
						}
						if (((k&1023) == door2) || ((k&1023) == door2+5))
						{
							doorx = x+1, doory = y, doorstat = door2;
							if ((posx&1023) > 767)
								posx = (posx&0xfc00)+767;
						}
						if (((k&1023) == door3) || ((k&1023) == door3+7))
						{
							doorx = x+1, doory = y, doorstat = door3;
							if ((posx&1023) > 767)
								posx = (posx&0xfc00)+767;
						}
						if ((((k&1023) == door4) && (coins >= 10)) || ((k&1023) == door4+6))
						{
							doorx = x+1, doory = y, doorstat = door4;
							if ((posx&1023) > 767)
								posx = (posx&0xfc00)+767;
						}
						if (((k&1023) == door5) || ((k&1023) == door5+7))
						{
							doorx = x+1, doory = y, doorstat = door5;
							if ((posx&1023) > 767)
								posx = (posx&0xfc00)+767;
						}
					}
					k = board[x][y-1];
					if (((k&16384) == 0) && ((ang&1024) > 0))
					{
						if (((k&1023) == door1) || ((k&1023) == door1+5))
						{
							doorx = x, doory = y-1, doorstat = door1;
							if ((posy&1023) < 256)
								posy = (posy&0xfc00)+256;
						}
						if (((k&1023) == door2) || ((k&1023) == door2+5))
						{
							doorx = x, doory = y-1, doorstat = door2;
							if ((posy&1023) < 256)
								posy = (posy&0xfc00)+256;
						}
						if (((k&1023) == door3) || ((k&1023) == door3+7))
						{
							doorx = x, doory = y-1, doorstat = door3;
							if ((posy&1023) < 256)
								posy = (posy&0xfc00)+256;
						}
						if ((((k&1023) == door4) && (coins >= 10)) || ((k&1023) == door4+6))
						{
							doorx = x, doory = y-1, doorstat = door4;
							if ((posy&1023) < 256)
								posy = (posy&0xfc00)+256;
						}
						if (((k&1023) == door5) || ((k&1023) == door5+7))
						{
							doorx = x, doory = y-1, doorstat = door5;
							if ((posy&1023) < 256)
								posy = (posy&0xfc00)+256;
						}
					}
					k = board[x][y+1];
					if (((k&16384) == 0) && ((ang&1024) == 0))
					{
						if (((k&1023) == door1) || ((k&1023) == door1+5))
						{
							doorx = x, doory = y+1, doorstat = door1;
							if ((posy&1023) > 767)
								posy = (posy&0xfc00)+767;
						}
						if (((k&1023) == door2) || ((k&1023) == door2+5))
						{
							doorx = x, doory = y+1, doorstat = door2;
							if ((posy&1023) > 767)
								posy = (posy&0xfc00)+767;
						}
						if (((k&1023) == door3) || ((k&1023) == door3+7))
						{
							doorx = x, doory = y+1, doorstat = door3;
							if ((posy&1023) > 767)
								posy = (posy&0xfc00)+767;
						}
						if ((((k&1023) == door4) && (coins >= 10)) || ((k&1023) == door4+6))
						{
							doorx = x, doory = y+1, doorstat = door4;
							if ((posy&1023) > 767)
								posy = (posy&0xfc00)+767;
						}
						if (((k&1023) == door5) || ((k&1023) == door5+7))
						{
							doorx = x, doory = y+1, doorstat = door5;
							if ((posy&1023) > 767)
								posy = (posy&0xfc00)+767;
						}
					}
					if (doorstat > 0)
					{
						if ((doorstat == door1) && ((board[doorx][doory]&1023) == door1+5))
							doorstat = door1+5+1024;
						if ((doorstat == door2) && ((board[doorx][doory]&1023) == door2+5))
							doorstat = door2+5+1024;
						if ((doorstat == door3) && ((board[doorx][doory]&1023) == door3+7))
							doorstat = door3+7+1024;
						if (doorstat == door4)
						{
							if ((board[doorx][doory]&1023) == door4+6)
								doorstat = door4+6+1024;
							else
							{
								coins -= 10;
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
						}
						if ((doorstat == door5) && ((board[doorx][doory]&1023) == door5+7))
							doorstat = door5+7+1024;
						if ((doorstat == door3) || (doorstat == door3+7+1024))
							ksay(29);
						else
						{
							if (doorstat < 1024)
								ksay(13);
							else
								ksay(4);
						}
					}
					if ((lastunlock == 0) && (doorstat == 0))
						ksay(12);
				}
			}
			lastunlock = 1;
		}
		else
			lastunlock = 0;
		if (slottime > 0)
		{
			i = slottime;
			slottime -= clockspeed;
			if (slottime >= 304)
				slotpos[0] += (clockspeed>>1);
			if (slottime >= 184)
				slotpos[1] += (clockspeed>>1);
			if (slottime >= 64)
				slotpos[2] += (clockspeed>>1);
			if ((i >= 304) && (slottime < 304))
			{
				ksay(8);
				slotpos[0] = ((slotpos[0]+4)&0x78);
			}
			if ((i >= 184) && (slottime < 184))
			{
				ksay(8);
				slotpos[1] = ((slotpos[1]+4)&0x78);
			}
			if ((i >= 64) && (slottime < 64))
			{
				ksay(8);
				slotpos[2] = ((slotpos[2]+4)&0x78);
			}
			if (slottime <= 0)
			{
				copyslots(slotto);
				slottime = 0;
				i = slotable[0][slotpos[0]>>3];
				j = slotable[1][slotpos[1]>>3];
				k = slotable[2][slotpos[2]>>3];
				m = 0;
				if ((i == j) && (i == k))
				{
					switch(i)
					{
						case 0: m = 200; break;
						case 1: m = 25; break;
						case 2: m = 25; break;
						case 3: m = 8; break;
						case 4: m = 4; break;
						case 5: m = 4; break;
					}
				}
				else if ((i == j) || (j == k) || (i == k))
				{
					if (j == k)
						i = j;
					switch(i)
					{
						case 0: m = 5; break;
						case 1: m = 2; break;
						case 2: m = 2; break;
						case 3: m = 2; break;
						case 4: m = 1; break;
						case 5: m = 1; break;
					}
				}
				if (m > 0)
				{
					owecoins += m;
					owecoinwait = 0;
				}
				else if (death == 4095)
					ksay(26);
			}
			copyslots(slotto+1);
		}
		if (owecoins > 0)
		{
			if (owecoinwait >= 0)
			{
				owecoinwait -= clockspeed;
				if (owecoinwait <= 0)
				{
					owecoinwait = 60;
					coins++;
					owecoins--;
					if (coins > 999)
						coins = 999;
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
					if (death == 4095)
						 ksay(25);
				}
			}
		}
		if (doorstat > 0)
		{
			if (doorstat < 1024)
			{
				doorstat++;
				if (doorstat == door1+6) doorstat = 0;
				if (doorstat == door2+6) doorstat = 0;
				if (doorstat == door3+8) doorstat = 0;
				if (doorstat == door4+7) doorstat = 0;
				if (doorstat == door5+8) doorstat = 0;
			}
			else
			{
				doorstat--;
				if (doorstat == 1024+door1-1) doorstat = 0;
				if (doorstat == 1024+door2-1) doorstat = 0;
				if (doorstat == 1024+door3-1) doorstat = 0;
				if (doorstat == 1024+door4-1) doorstat = 0;
				if (doorstat == 1024+door5-1) doorstat = 0;
			}
			if (doorstat != 0)
			{
				board[doorx][doory] &= 8192;
				board[doorx][doory] |= (doorstat&1023);
				if ((wallheader[doorstat&1023]&8) > 0)
					board[doorx][doory] |= 1024;
			}
		}
		maxvel = 128;
		if (keystatus[keydefs[7]] > 0)
			maxvel = 256;
		if (death == 4095)
		{
			if (keystatus[keydefs[0]])
			{
				vel += (clockspeed<<2);
				if (vel > maxvel)
					vel = maxvel;
			}
			else
			{
				if (vel > 0)
				{
					vel -= ((clockspeed<<1)+clockspeed);
					if (vel < 0)
						vel = 0;
				}
			}
			if (keystatus[keydefs[1]])
			{
				vel -= (clockspeed<<2);
				if (vel < -maxvel)
					vel = -maxvel;
			}
			else
			{
				if (vel < 0)
				{
					vel += ((clockspeed<<1)+clockspeed);
					if (vel > 0)
						vel = 0;
				}
			}
		}
		if (myvel < 0)
		{
			myvel += (clockspeed<<1);
			if (myvel > 0)
				myvel = 0;
		}
		if (myvel > 0)
		{
			myvel -= (clockspeed<<1);
			if (myvel < 0)
				myvel = 0;
		}
		if (mousy != 0)
		{
			myvel = -(mousy<<3);
			if (myvel < -300)
				myvel = -300;
			if (myvel > 300)
				myvel = 300;
			if (myvel+vel < -300)
				myvel = -300-vel;
			if (myvel+vel > 300)
				myvel = 300-vel;
		}
		if (((vel+myvel) != 0) || (svel != 0))
		{
			oldposx = posx;
			oldposy = posy;
			if ((vel+myvel) != 0)
			{
				posx += (int)(((long)(vel+myvel)*clockspeed*sintable[(ang+512)&2047])>>19);
				posy += (int)(((long)(vel+myvel)*clockspeed*sintable[ang])>>19);
				checkhitwall(oldposx,oldposy,posx,posy);
			}
			if (svel != 0)
			{
				posx += (int)(((long)svel*clockspeed*sintable[ang])>>19);
				posy += (int)(((long)svel*clockspeed*sintable[(ang+1536)&2047])>>19);
				checkhitwall(oldposx,oldposy,posx,posy);
			}
		}
		_asm \
		{
			mov dx, 0x3c8
			mov al, 255
			out dx, al
			inc dx
			mov al, byte ptr totalclock[0]
			shr al, 1
			and al, 63
			out dx, al
			add al, 32
			and al, 63
			out dx, al
			xor al, 63
			out dx, al
		}
		x = (posx>>10);
		y = (posy>>10);
		if (boardnum == 29)
			if ((x == 32) && (y == 29))
			{
				winallgame();
				fade(0);
				kgif(1);
				introduction(0);
			}
		if ((vel|svel) > 0)
		{
			hitnet = 0;
			if ((posx < oldposx) && ((board[(posx-260)>>10][y]&1023) == net)) hitnet = 1;
			if ((posx > oldposx) && ((board[(posx+260)>>10][y]&1023) == net)) hitnet = 1;
			if ((posy < oldposy) && ((board[x][(posy-260)>>10]&1023) == net)) hitnet = 1;
			if ((posy > oldposy) && ((board[x][(posy+260)>>10]&1023) == net)) hitnet = 1;
			if ((hitnet == 1) && (abs(vel)+abs(svel) > 64))
			{
				svel = -svel-svel;
				vel = -vel-vel;
				myvel = -myvel-myvel;
				if (death == 4095)
					ksay(2);
			}
		}
		i = (board[x][y]&1023);
		if (i != 0)
		{
			incenter = 1;
			j = abs(512-(posx&1023));
			k = abs(512-(posy&1023));
			if ((j >= 240) || (k >= 240))
				incenter = 0;
			if ((j+k) >= 340)
				incenter = 0;
		}
		if ((keystatus[42] > 0) && (keystatus[54] > 0) && (cheatenable == 1))
			cheatkeysdown = 1;
		else
			cheatkeysdown = 0;
		if ((i == extralife) || ((keystatus[18] > 0) && (cheatkeysdown == 1)))
		{
			lifevests++;
			if (lifevests > 9)
				lifevests = 9;
			if (i == extralife)
			{
				board[x][y] = 1024|(board[x][y]&16384);
				scorecount += 7500;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			textbuf[0] = 9, textbuf[1] = 0;
			textprint(96,12,(char)0,0);
			textbuf[0] = lifevests+48, textbuf[1] = 0;
			textprint(96,12,(char)176,0);
		}
		if ((i == lightning) || ((keystatus[38] > 0) && (cheatkeysdown == 1)))
		{
			lightnings++;
			if (lightnings > 6)
				lightnings = 6;
			if (i == lightning)
			{
				board[x][y] = 1024|(board[x][y]&16384);
				scorecount += 5000;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			textbuf[0] = 9, textbuf[1] = 0;
			textprint(296,21,(char)0,0);
			textbuf[0] = lightnings+48, textbuf[1] = 0;
			textprint(296,21,(char)176,0);
		}
		if ((i == getcompass) || ((keystatus[32] > 0) && (cheatkeysdown == 1)))
		{
			compass = 1;
			if (i == getcompass)
			{
				board[x][y] = 1024|(board[x][y]&16384);
				scorecount += 15000;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
		}
		if (((i == bul1get) || (i == bul2get) || (i == bul3get)) || (((keystatus[33] > 0) || (keystatus[34] > 0) || (keystatus[35] > 0)) && (cheatkeysdown == 1)))
		{
			if ((i == bul1get) || ((keystatus[33] > 0) && (cheatkeysdown == 1)))
			{
				firepowers[0]++;
				if (firepowers[0] > 6)
					firepowers[0] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(272,12,(char)0,0);
				textbuf[0] = firepowers[0]+48, textbuf[1] = 0;
				textprint(272,12,(char)176,0);
			}
			if ((i == bul2get) || ((keystatus[34] > 0) && (cheatkeysdown == 1)))
			{
				firepowers[1]++;
				if (firepowers[1] > 6)
					firepowers[1] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(272,21,(char)0,0);
				textbuf[0] = firepowers[1]+48, textbuf[1] = 0;
				textprint(272,21,(char)176,0);
			}
			if ((i == bul3get) || ((keystatus[35] > 0) && (cheatkeysdown == 1)))
			{
				firepowers[2]++;
				if (firepowers[2] > 6)
					firepowers[2] = 6;
				textbuf[0] = 9, textbuf[1] = 0;
				textprint(296,12,(char)0,0);
				textbuf[0] = firepowers[2]+48, textbuf[1] = 0;
				textprint(296,12,(char)176,0);
			}
			if ((i == bul1get) || (i == bul2get) || (i == bul3get))
			{
				board[x][y] = (emptybulstand+1024)|(board[x][y]&16384);
				scorecount += 2500;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
		}
		if ((i == diamond) || (i == diamonds3))
		{
			if (death == 4095)
				ksay(7);
			board[x][y] = 1024|(board[x][y]&16384);
			if (i == diamond)
			{
				scorecount += 500;
				coins += 5;
			}
			if (i == diamonds3)
			{
				scorecount += 1500;
				coins += 15;
			}
			if (coins > 999)
				coins = 999;
			drawscore(scorecount);
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
		if ((i == coin) || ((keystatus[16] > 0) && (cheatkeysdown == 1)))
		{
			coins++;
			if (coins > 999)
				coins = 999;
			if (death == 4095)
				ksay(7);
			if (i == coin)
			{
				board[x][y] = 1024|(board[x][y]&16384);
				scorecount += 100;
				drawscore(scorecount);
			}
			else
				cheated++;
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
		if ((i == goldkey) || ((keystatus[37] > 0) && (cheatkeysdown == 1)))
		{
			keys[0]++;
			if (i == goldkey)
			{
				board[x][y] = (emptykey+1024)|(board[x][y]&16384);
				scorecount += 500;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			statusbardraw(36,44,14,6,144,13,statusbarinfo,0);
		}
		if ((i == silverkey) || ((keystatus[37] > 0) && (cheatkeysdown == 1)))
		{
			keys[1]++;
			if (i == silverkey)
			{
				board[x][y] = (emptykey+1024)|(board[x][y]&16384);
				scorecount += 500;
				drawscore(scorecount);
				if (death == 4095)
					ksay(7);
			}
			else
				cheated++;
			statusbardraw(50,44,14,6,144,21,statusbarinfo,0);
		}
		if (i == warp)
		{
			if (justwarped == 0)
			{
				k = -1;
				for(j=0;j<numwarps;j++)
					if ((xwarp[j] == (char)x) && (ywarp[j] == (char)y))
						k = j;
				if (k != -1)
				{
					k++;
					if (k >= numwarps)
						k = 0;
					if (boardnum == 29)
						if ((k&1) == 0)
							k ^= 2;
					for(i=0;i<mnum;i++)
						if (mstat[i] == mondog)
						{
							if (mposx[i] > posx) templong = (long)(mposx[i]-posx);
							else templong = (long)(posx-mposx[i]);
							if (mposy[i] > posy) templong += (long)(mposy[i]-posy);
							else templong += (long)(posy-mposy[i]);
							if (templong < 4096)
							{
								board[moldx[i]>>10][moldy[i]>>10] &= 0xbfff;
								board[mgolx[i]>>10][mgoly[i]>>10] &= 0xbfff;
								mposx[i] = (((unsigned)xwarp[k])<<10)+512;
								mposy[i] = (((unsigned)ywarp[k])<<10)+512;
								moldx[i] = mposx[i], moldy[i] = mposy[i];
								mgolx[i] = mposx[i], mgoly[i] = mposy[i];
								board[mposx[i]>>10][mposy[i]>>10] |= 0x4000;
							}
						}
					posx = (((unsigned)xwarp[k])<<10)+512;
					posy = (((unsigned)ywarp[k])<<10)+512;
					x = xwarp[k];
					y = ywarp[k];
					i = (board[x][y]&1023);
					if (death == 4095)
						ksay(17);
					fadewarpval = 8;
				}
				justwarped = 1;
				if ((boardnum == 9) && (k == 2))
				{
					if (cheated == 0)
					{
						wingame(1);
						if (numboards <= 10)
						{
							fade(0);
							kgif(1);
							introduction(1);
						}
						else
							won = 1;
					}
					else
					{
						posx = (((unsigned)xwarp[0])<<10)+512;
						posy = (((unsigned)ywarp[0])<<10)+512;
					}
				}
			}
		}
		else
			justwarped = 0;
		if (death == 4095)
		{
			if (fadewarpval < 63)
			{
				fadewarpval += (clockspeed>>1);
				if (fadewarpval > 63)
					fadewarpval = 63;
				fade(fadewarpval);
				fadehurtval = 0;
			}
			if (fadehurtval > 0)
			{
				fadehurtval -= (clockspeed>>1);
				if (fadehurtval > 63)
					fadehurtval = 63;
				if (fadehurtval < 0)
					fadehurtval = 0;
				fade(fadehurtval+128);
			}
		}
		if (purpletime >= ototclock)
		{
			if (purpletime >= totalclock)
			{
				if (purpletime < totalclock+3840)
				{
					k = ((3840-(int)(purpletime-totalclock))>>8);
					if ((k >= 0) && (k <= 15))
						statusbardraw(0,30+k,16,1,159,13+k,statusbarinfo,0);
				}
			}
			else
				statusbardraw(15,13,16,15,159,13,statusbarback,0);
		}
		if (greentime >= ototclock)
		{
			if (greentime >= totalclock)
			{
				if (greentime < totalclock+3840)
				{
					k = ((3840-(int)(greentime-totalclock))>>8);
					if ((k >= 0) && (k <= 15))
						statusbardraw(16,30+k,16,1,176,13+k,statusbarinfo,0);
				}
			}
			else
				statusbardraw(16,13,16,15,176,13,statusbarback,0);
		}
		if (capetime[0] >= ototclock)
		{
			if (capetime[0] >= totalclock)
			{
				if (capetime[0] < totalclock+3072)
				{
					k = (int)((capetime[0]-totalclock)>>9);
					if ((k >= 0) && (k <= 5))
					{
						if (k == 5) statusbardraw(0,0,21,28,194,2,coatfade,0);
						if (k == 4) statusbardraw(21,0,21,28,194,2,coatfade,0);
						if (k == 3) statusbardraw(42,0,21,28,194,2,coatfade,0);
						if (k == 2) statusbardraw(0,32,21,28,194,2,coatfade,0);
						if (k == 1) statusbardraw(21,32,21,28,194,2,coatfade,0);
						if (k == 0) statusbardraw(42,32,21,28,194,2,coatfade,0);
					}
				}
			}
			else
				statusbardraw(16,2,21,28,194,2,statusbarback,0);
		}
		if (capetime[1] >= ototclock)
		{
			if (capetime[1] >= totalclock)
			{
				if (capetime[1] < totalclock+3072)
				{
					k = (int)((capetime[1]-totalclock)>>9);
					if ((k >= 0) && (k <= 5))
					{
						if (k == 5) statusbardraw(0,0,21,28,216,2,coatfade,0);
						if (k == 4) statusbardraw(21,0,21,28,216,2,coatfade,0);
						if (k == 3) statusbardraw(42,0,21,28,216,2,coatfade,0);
						if (k == 2) statusbardraw(0,32,21,28,216,2,coatfade,0);
						if (k == 1) statusbardraw(21,32,21,28,216,2,coatfade,0);
						if (k == 0) statusbardraw(42,32,21,28,216,2,coatfade,0);
					}
				}
			}
			else
				statusbardraw(16,2,21,28,216,2,statusbarback,0);
		}
		if ((i == purple) || ((keystatus[39] > 0) && (cheatkeysdown == 1)))
		{
			if (purpletime < totalclock)
				purpletime = totalclock + 4800;
			else
				purpletime += 4800;
			if (i == purple)
			{
				board[x][y] = (emptypurple+1024)|(board[x][y]&16384);
				scorecount += 1000;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			statusbardraw(0,0,16,15,159,13,statusbarinfo,0);
		}
		if ((i == green) || ((keystatus[40] > 0) && (cheatkeysdown == 1)))
		{
			if (greentime < totalclock)
				greentime = totalclock + 4800;
			else
				greentime += 4800;
			if (i == green)
			{
				board[x][y] = (emptygreen+1024)|(board[x][y]&16384);
				scorecount += 1500;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			statusbardraw(0,15,16,15,176,13,statusbarinfo,0);
		}
		if ((i == gray) || ((keystatus[36] > 0) && (cheatkeysdown == 1)))
		{
			if (capetime[0] < totalclock)
				capetime[0] = totalclock + 7200;
			else
				capetime[0] += 7200;
			if (i == gray)
			{
				board[x][y] = (emptycoat+1024)|(board[x][y]&16384);
				scorecount += 2500;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			statusbardraw(16,0,21,28,194,2,statusbarinfo,0);
		}
		if ((i == blue) || ((keystatus[30] > 0) && (cheatkeysdown == 1)))
		{
			if (capetime[1] < totalclock)
				capetime[1] = totalclock + 4800;
			else
				capetime[1] += 4800;
			if (i == blue)
			{
				board[x][y] = (emptycoat+1024)|(board[x][y]&16384);
				scorecount += 5000;
				drawscore(scorecount);
			}
			else
				cheated++;
			if (death == 4095)
				ksay(7);
			statusbardraw(37,0,21,28,216,2,statusbarinfo,0);
		}
		if (i == grayblue)
		{
			if (capetime[0] < totalclock)
				capetime[0] = totalclock + 7200;
			else
				capetime[0] += 7200;
			if (capetime[1] < totalclock)
				capetime[1] = totalclock + 4800;
			else
				capetime[1] += 4800;
			board[x][y] = (emptycoat+1024)|(board[x][y]&16384);
			if (death == 4095)
				ksay(7);
			scorecount += 7500;
			drawscore(scorecount);
			statusbardraw(16,0,21,28,194,2,statusbarinfo,0);
			statusbardraw(37,0,21,28,216,2,statusbarinfo,0);
		}
		if (i == all4coats)
		{
			if (capetime[0] < totalclock)
				capetime[0] = totalclock + 7200;
			else
				capetime[0] += 7200;
			if (capetime[1] < totalclock)
				capetime[1] = totalclock + 4800;
			else
				capetime[1] += 4800;
			if (purpletime < totalclock)
				purpletime = totalclock + 4800;
			else
				purpletime += 4800;
			if (greentime < totalclock)
				greentime = totalclock + 4800;
			else
				greentime += 4800;
			board[x][y] = (emptyall4+1024)|(board[x][y]&16384);
			if (death == 4095)
				ksay(7);
			scorecount += 10000;
			drawscore(scorecount);
			statusbardraw(0,0,16,15,159,13,statusbarinfo,0);
			statusbardraw(0,15,16,15,176,13,statusbarinfo,0);
			statusbardraw(16,0,21,28,194,2,statusbarinfo,0);
			statusbardraw(37,0,21,28,216,2,statusbarinfo,0);
		}
		if (life < 4095)
			if (((i == meal) || (i == honey) || (i == fries) || (i == firstaid)) || (keystatus[keydefs[13]] > 0) || ((keystatus[31] > 0) && (cheatkeysdown == 1)))
			{
				if ((keystatus[31] > 0) && (cheatkeysdown == 1))
					life += 320, cheated++;
				if (keystatus[keydefs[13]] > 0)
				{
					life += 320;
					scorecount >>= 1;
					drawscore(scorecount);
				}
				if (i == fries)
					life += 320, board[x][y] = (emptyfries+1024)|(board[x][y]&16384);
				if (i == meal)
					life += 640, board[x][y] = (emptymeal+1024)|(board[x][y]&16384);
				if (i == honey)
					life += 640, board[x][y] = ((board[x][y]&16384)|1024);
				if (i == firstaid)
					life += 1280, board[x][y] = (emptyfirst+1024)|(board[x][y]&16384);
				if (life > 4095)
					life = 4095;
				drawlife();
				if (death == 4095)
					ksay(7);
			}
		if ((i == stairs) || (won == 1) || ((keystatus[48] > 0) && (cheatkeysdown == 1)))
		{
			won = 0;
			musicoff();
			if (boardnum < numboards-1)
			{
				if (i == stairs)
				{
					if (cheated == 0)
						ksay(11);
					else
						ksay(19);
				}
				if ((boardnum&1) == 0)
					loadmusic("CONGRAT1");
				else
					loadmusic("CONGRAT0");
				musicon();
				if (i == stairs)
				{
					hiscorecheck();
					boardnum++;
					//reviewboard();
				}
				else
					boardnum++;
				if ((keystatus[48] > 0) && (cheatkeysdown == 1))
					cheated++;
				musicoff();
				loadboard();
				ksmfile[0] = 'L', ksmfile[1] = 'A', ksmfile[2] = 'B';
				ksmfile[3] = 'S', ksmfile[4] = 'N', ksmfile[5] = 'G';
				ksmfile[6] = (boardnum/10)+48, ksmfile[7] = (boardnum%10)+48;
				ksmfile[8] = 0;
				statusbardraw(16,13,14,15,144,13,statusbarback,0);
				textbuf[0] = 9, textbuf[1] = 9, textbuf[2] = 0;
				textprint(46,20,(char)0,0);
				textbuf[0] = ((boardnum+1)/10)+48;
				textbuf[1] = ((boardnum+1)%10)+48;
				textbuf[2] = 0;
				if (textbuf[0] == 48)
					textbuf[0] = 32;
				textprint(46,20,(char)176,0);
				lastunlock = 1;
				lastshoot = 1;
				lastbarchange = 1;
				angvel = 0;
				vel = 0;
				mxvel = 0;
				myvel = 0;
				svel = 0;
				hvel = 0;
				ototclock = -1;
				scoreclock = 0;
				scorecount = 0;
				drawscore(scorecount);
				drawtime(scoreclock);
				clockspeed = 0;
			}
			else
			{
				fade(0);
				kgif(1);
				introduction(0);
			}
		}
		if (death == 4095)
		{
			if ((i == fan) && (capetime[0] < totalclock) && (capetime[1] < totalclock))
			{
				life -= (clockspeed<<3);
				if (life <= 0)
				{
					life = 0;
					drawlife();
					death = 4094;
					angvel = (rand()&32)-16;
					ksay(5);
					musicoff();
				}
				else
				{
					drawlife();
					angvel = (rand()&32)-16;
					fadehurtval += 16;
					ksay(9);
				}
			}
			if (((i&1023) == hole) && (incenter == 1))
			{
				life = 0;
				drawlife();
				death = 4094;
				angvel = 0;
				ksay(6);
				musicoff();
			}
		}
		if (keystatus[keydefs[15]] > 0)
		{
			if (vidmode == 0)
				n = 17;
			else
				n = 37;
			sprintf(textbuf,"GAME PAUSED");
			textprint(180-(strlen(textbuf)<<2),n,(char)0,lastpageoffset+90);
			setuptextbuf((scoreclock*5)/12);
			textbuf[12] = textbuf[11];
			textbuf[11] = textbuf[10];
			textbuf[10] = textbuf[9];
			textbuf[9] = '.';
			k = 0;
			while ((textbuf[k] == 8) && (k < 12))
				k++;
			for(n=0;n<13;n++)
				textbuf[n] = textbuf[n+k];
			for(n=strlen(textbuf);n>=0;n--)
				textbuf[n+6] = textbuf[n];
			textbuf[strlen(textbuf)+6] = 0;
			textbuf[0] = 'T';
			textbuf[1] = 'i';
			textbuf[2] = 'm';
			textbuf[3] = 'e';
			textbuf[4] = ':';
			textbuf[5] = ' ';
			if (vidmode == 0)
				n = 27;
			else
				n = 47;
			textprint(180-(strlen(textbuf)<<2),n,(char)0,lastpageoffset+90);
			ototclock = totalclock;
			if (fadewarpval > 16)
				for(i=63;i>=16;i-=2)
				{
					_asm \
					{
						mov dx, 0x3da
retraceholdfade3:
						in al, dx
						test al, 8
						jz retraceholdfade3
retraceholdfade4:
						in al, dx
						test al, 8
						jnz retraceholdfade4
					}
					fade(i+64);
				}
			x = keystatus[keydefs[15]];
			y = 1;
			while ((x <= y) && (keystatus[1] == 0) && (keystatus[57] == 0) && (keystatus[28] == 0) && (bstatus == 0))
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
				y = x;
				x = keystatus[keydefs[15]];
			}
			if (fadewarpval > 16)
				for(i=16;i<=fadewarpval;i+=2)
				{
					_asm \
					{
						mov dx, 0x3da
retraceholdfade5:
						in al, dx
						test al, 8
						jz retraceholdfade5
retraceholdfade6:
						in al, dx
						test al, 8
						jnz retraceholdfade6
					}
					fade(i+64);
				}
			totalclock = ototclock;
			clockspeed = 0;
			lastunlock = 1;
			lastshoot = 1;
			lastbarchange = 1;
		}
		if (keystatus[88] > 0)
			screencapture();
		if (ototclock == 0)
		{
			fade(27);
			loadmusic("DRUMSONG");
			musicon();
			loadstory(boardnum);
			keystatus[1] = 0;
			keystatus[57] = 0;
			keystatus[28] = 0;
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
				totalclock += clockspeed;
				clockspeed = 0;
				_asm \
				{
					mov dx, 0x3c8
					mov al, 240
					out dx, al
				}
				j = 63-(((int)labs((totalclock%120)-60))>>3);
				for(i=0;i<16;i++)
				{
					tempbuf[i*3] = (i*j)>>4;
					tempbuf[i*3+1] = (i*j)>>4;
					tempbuf[i*3+2] = (i*j)>>4;
				}
				_asm \
				{
					mov dx, 0x3da
retrace3:
					in al, dx
					test al, 8
					jnz retrace3
retrace4:
					in al, dx
					test al, 8
					jz retrace4
				}
				for(i=0;i<48;i++)
					_asm \
					{
						mov dx, 0x3c9
						mov bx, i
						mov al, byte ptr tempbuf[bx]
						out dx, al
					}
			}
			lastunlock = 1;
			lastshoot = 1;
			lastbarchange = 1;
			clockspeed = 0;
			scoreclock = 0;
			scorecount = 0;
			drawscore(scorecount);
			drawtime(scoreclock);
			musicoff();
			ksmfile[0] = 'L', ksmfile[1] = 'A', ksmfile[2] = 'B';
			ksmfile[3] = 'S', ksmfile[4] = 'N', ksmfile[5] = 'G';
			ksmfile[6] = (boardnum/10)+48, ksmfile[7] = (boardnum%10)+48;
			ksmfile[8] = 0;
			loadmusic(ksmfile);
			for(i=27;i<=63;i+=2)
			{
				_asm \
				{
					mov dx, 0x3da
retraceholdfade1:
					in al, dx
					test al, 8
					jz retraceholdfade1
retraceholdfade2:
					in al, dx
					test al, 8
					jnz retraceholdfade2
				}
				fade(i);
			}
			musicon();
			clockspeed = 0;
			totalclock = ototclock;
			ototclock = 1;
		}
		if (keystatus[keydefs[16]] > 0)
		{
			mute = 1 - mute;
			if ((mute == 1) && (musicsource == 0))
				outp(97,inp(97)&252);
			keystatus[keydefs[16]] = 0;
		}
		if (keystatus[keydefs[17]] == 1)
		{
			if (ototclock > 1)
			{
				j = mainmenu();
				if (j < 7)
				{
					if (j == 0)
					{
						musicoff();
						fade(0);
						keystatus[57] = 0;
						keystatus[28] = 0;
						keystatus[1] = 0;
						if (newgameplace == 0) boardnum = 0;
						if (newgameplace == 1) boardnum = 10;
						if (newgameplace == 2) boardnum = 20;
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
						statusbaralldraw();
					}
					if ((j == 1) && (loadsavegameplace >= 0))
					{
						fade(0);
						loadgame(loadsavegameplace);
						fade(63);
					}
					if ((j == 2) && (loadsavegameplace >= 0))
					{
						if (hiscorenamstat == 0)
						{
							for(i=lside;i<rside;i++)
								height[i] = 0;
							i = pageoffset;
							pageoffset = lastpageoffset;
							spridraw(84,107,64*4,scorebox);
							for(k=134;k<141;k++)
								height[k] = 400;
							spridraw(134,107,64*4,scorebox);
							for(k=173;k<180;k++)
								height[k] = 400;
							spridraw(173,107,64*4,scorebox);
							for(k=212;k<219;k++)
								height[k] = 400;
							spridraw(212,107,64*4,scorebox);
							for(k=0;k<22;k++)
								textbuf[k] = 8;
							textbuf[22] = 0;
							textprint(91,125,(char)0,lastpageoffset+90);
							textbuf[1] = 0;
							textprint(261,125,(char)0,lastpageoffset+90);
							pageoffset = i;
							getname();
						}
						if (hiscorenamstat > 0)
							savegame(loadsavegameplace);
					}
					keystatus[keydefs[17]] = 0;
					lastunlock = 1;
					lastshoot = 1;
					lastbarchange = 1;
				}
				else
					quitgame = 1;
				if (ototclock > 0)
					totalclock = ototclock;
				if (vidmode == 0)
					linecompare(statusbar);
				clockspeed = 0;
			}
			else
			{
				keystatus[keydefs[17]] = 0;
				lastunlock = 1;
				lastshoot = 1;
				lastbarchange = 1;
			}
		}
		if (ototclock <= 0)
			ototclock++;
		else
			ototclock = totalclock;
		totalclock += clockspeed;
		scoreclock += clockspeed;
		if ((scoreclock%240) < clockspeed)
			drawtime(scoreclock);
		clockspeed = 0;
	}
	_disable(); _dos_setvect(0x9, oldkeyhandler); _enable();
	_disable(); _dos_setvect(0, olddivzerohandler); _enable();
	musicoff();
	if (musicsource == 1)
	{
		midiscrap = 256;
		while ((midiscrap > 0) && ((inp(statport) & 0x40) != 0))
			midiscrap--;
		_asm \
		{
			mov dx, statport
			mov al, 0xff
			out dx, al
		}
		midiscrap = 256;
		while ((midiscrap > 0) && ((inp(dataport) & 0xfe) != 0))
			midiscrap--;
	}
	outp(0x43,54); outp(0x40,255); outp(0x40,255);
	_disable(); _dos_setvect(0x8, oldhand); _enable();
	outp(0x97,inp(97)&252);
	_asm \
	{
		mov ax, 0x3
		int 0x10
	}
	if (quitgame == 2)
		printf("Error #3:  Invalid saved game.");
	if (numboards < 30)
	{
		if (memtype[endtext-1] != 65535)
		{
			if (vms == 1)
				if (memtype[endtext-1] == 0)
					loadvms(endtext-1);
			if ((emswalls > 0) && ((memtype[endtext-1]&1024) > 0))
				emsetpages(memtype[endtext-1]-1024,0);
			else if ((xmswalls > 0) && ((memtype[endtext-1]&2048) > 0))
				xmsmemmove(xmshandle,((long)(memtype[endtext-1]-2048))<<12,0,((long)xmspageframe)<<16,4096L);
		}
		i = walseg[endtext-1];
		_asm \
		{
			cld
			mov ax, 0xb800
			mov es, ax
			xor di, di
			mov cx, 4000
			xor si, si
			mov ax, i
			push ds
			mov ds, ax
			rep movsb
			pop ds

			mov ah, 2
			mov bx, 0
			mov dx, 0x1600
			int 0x10
		}
	}
	/*
	printf("%d tiles were in conventional memory.\n",convwalls);
	printf("%d tiles were in expanded EMS memory.\n",emswalls);
	printf("%d tiles were in extended XMS memory.\n",xmswalls);
	if (vms == 0)
		i = 0;
	else
		i = numwalls-(convwalls+emswalls+xmswalls);
	printf("%d tiles were not really in memory.\n",i);
	printf("Video card seemed to have %d-bit data path.\n",16-(videotype<<3));
	*/
	convdeallocate(note);
	convdeallocate(snd);
	convdeallocate(lzwbuf);
	if (convwalls > 0) convdeallocate(pic);
	if (emswalls > 0) emmdeallocate(emmhandle);
	if (xmswalls > 0) xmsdeallocate(xmshandle);
	if (speechstatus >= 2) reset_dsp();
}

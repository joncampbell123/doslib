#include <string.h>
#include <stdio.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>

#define dataport 0x330
#define statport 0x331
#define numwalls 448
#define numoptions 4
#define numkeys 18
#define initialwalls 8
#define gifbuflen 4096

//SORTWALLSTART
#define fountain 12        //l = 3
#define safe 16            //l = 3
#define clock 20
#define introstoryback 32
#define kenface 74
#define kenfaceouch 75
#define andy 76
#define andygone 77
#define map 78
#define invisible 79
#define goldlock 80
#define silverlock 81
#define doorside1 84
#define doorside2 84
#define doorside5 85
#define doorside3 86
#define doorside4 87
#define door1 88           //l = 6
#define door2 94           //l = 6
#define door3 100           //l = 8
#define door4 108          //l = 7
#define door5 115          //l = 8
#define bricksfull 130
#define brickshalf 131
#define slotto 134
#define slotup 135
#define slotinfo 137
#define soda 138
#define stairs 141
#define exitsign 143       //l = 2
#define tentacles 145      //l = 2
#define stairtop 151
#define minicolumn 152     //l = 4
#define tablecandle 163    //l = 2
#define target 166
#define net 167
#define fan 168            //l = 3
#define warp 170           //l = 2
#define coin 171
#define diamond 172
#define diamonds3 173
#define fries 174
#define emptyfries 175
#define meal 176
#define emptymeal 177
#define firstaid 178
#define emptyfirst 179
#define purple 180
#define emptypurple 181
#define green 182
#define emptygreen 183
#define gray 184
#define blue 185
#define grayblue 186
#define emptycoat 187
#define all4coats 188
#define emptyall4 189
#define goldkey 190
#define silverkey 191
#define emptykey 192
#define bul1get 193
#define bul1fly 194
#define bul2get 197
#define bul2fly 198
#define bul3get 200
#define bul3fly 201        //l = 6
#define bul3halfly 207     //l = 2
#define emptybulstand 209
#define lightning 210
#define extralife 211
#define getcompass 212
#define miniexplosion 213
#define explosion 214
#define hive 215
#define hivetohoney 216    //l = 6
#define honey 222
#define monbat 223         //l = 3
#define monske 226         //l = 3
#define monken 229         //l = 5
#define monwit 234
#define monbee 239         //l = 10
#define monspi 245         //l = 10
#define mongr2 251         //l = 18
#define mongre 262         //l = 6
#define monear 268         //l = 5
#define monand 273         //l = 5
#define monali 278         //l = 2
#define monbal 280         //l = 5
#define hole 285
#define monhol 286         //l = 4
#define mongho 290         //l = 3
#define monrob 293         //l = 10
#define monro2 303         //l = 8
#define mondog 311         //l = 15
#define monzor 341         //l = 10
#define monke2 356         //l = 14
#define monan2 370         //l = 9
#define monan3 376         //l = 9
#define bul4fly 385
#define bul5fly 386        //l = 2
#define bul6fly 388        //l = 4
#define bul7fly 390        //l = 3
#define bul8fly 393        //l = 4
#define bul10fly 395       //l = 3
#define bul9fly 397
#define bul11fly 398       //l = 7
#define monmum 407         //l = 8
#define skilblank 415
#define skilevel1 416
#define skilevel2 417
#define sodapics 418
#define copyright 419
#define labysign 420
#define congratsign 423
#define earth 424
#define youcheated 428
#define gameover 429
#define sharewaremessage 430
#define compassplc 431     //l = 4
#define statusbarback 435
#define statusbarinfo 436
#define coatfade 437
#define scorebox 438
#define menu 439
#define textwall 440       //l = 2
#define episodesign1off 442
#define episodesign2off 443
#define episodesign3off 444
#define episodesign1on 445
#define episodesign2on 446
#define episodesign3on 447
#define endtext 448
//SORTWALLEND

#ifdef MAIN
#define EXTERN
#else
#define EXTERN extern
#endif

#ifdef MAIN
EXTERN unsigned char near bultype[26] =
	{0,1,1,1,1,2,1,2,1,2,1,2,1,2,1,2,2,2,2,2,2,2,2,1,2,1};
EXTERN unsigned char near paldef[16][3] =
{
	0,30,63,28,34,60,0,50,20,41,52,28,63,63,25,63,63,63,63,20,20,63,0,63,
	63,48,27,63,40,25,63,48,48,45,63,45,55,55,63,63,40,63,63,30,20,55,25,30
};
EXTERN unsigned near pcfreq[63] =
{
	0,
	65,69,73,78,82,87,92,98,104,110,117,123,
	131,139,147,156,165,175,185,196,208,220,233,247,
	262,277,294,311,330,349,370,392,415,440,466,494,
	523,554,587,622,659,698,740,784,831,880,932,988,
	1047,1109,1175,1245,1319,1397,1480,1568,1661,1760,1864,1976,
	2094
};
EXTERN unsigned near adlibfreq[63] =
{
	0,
	2390,2411,2434,2456,2480,2506,2533,2562,2592,2625,2659,2695,
	3414,3435,3458,3480,3504,3530,3557,3586,3616,3649,3683,3719,
	4438,4459,4482,4504,4528,4554,4581,4610,4640,4673,4707,4743,
	5462,5483,5506,5528,5552,5578,5605,5634,5664,5697,5731,5767,
	6486,6507,6530,6552,6576,6602,6629,6658,6688,6721,6755,6791,
	7510
};
EXTERN unsigned near firstime = 1, quitgame = 0;
EXTERN int near midiinst = 0, cheatenable = 0, capturecount = 0;
EXTERN int near mainmenuplace = 0, loadsavegameplace = 0, gameheadstat = 0;
EXTERN int near newgameplace = 0, sodaplace = 0;
EXTERN int near vms = 0;
#else
EXTERN unsigned char near paldef[16][3];
EXTERN unsigned near pcfreq[63];
EXTERN unsigned near adlibfreq[63];
EXTERN unsigned near firstime, quitgame;
EXTERN int near midiinst, cheatenable, capturecount;
EXTERN int near mainmenuplace, loadsavegameplace, gameheadstat;
EXTERN int near newgameplace, sodaplace;
EXTERN int near vms;
#endif

EXTERN unsigned near lzwbuf, pic, walseg[numwalls+1];
EXTERN int near lborder[numwalls+1], rborder[numwalls+1];
EXTERN int near boleng[30*2], tileng[numwalls+1];
EXTERN long near tioffs[numwalls+1], walrec[numwalls+1];
EXTERN int near numcells, totalsortcnt, sortcnt;
EXTERN int near sortang[512], sorti[512], sortbnum[512];
EXTERN char near wallheader[numwalls+1], bmpkind[numwalls+1];
EXTERN unsigned near mposx[512], mposy[512], mnum;
EXTERN unsigned near mgolx[512], mgoly[512], mrotbuf[256];
EXTERN unsigned near moldx[512], moldy[512];
EXTERN int near mshock[512], mstat[512], vidmode, lside, rside, dside, scrsize;
EXTERN int near board[64][64], videotype, mute, numboards, skilevel;
EXTERN unsigned char near option[numoptions], keydefs[numkeys];
EXTERN unsigned char near tempbuf[4096], palette[768], mshot[512];
EXTERN char near hiscorenam[16], hiscorenamstat, namrememberstat;
EXTERN unsigned near height[360], linum[360], walnum[360], sprinum[360];
EXTERN int near lincalc[360], numline, lastline, linplc[360], linbits[360];
EXTERN long near tantable[1025], sintable[2050];
EXTERN int near radarang[360];
EXTERN unsigned int near oldsndleng, lastsoundnum;
EXTERN long near oldsndfiloffs;
EXTERN unsigned near pageoffset, lastpageoffset, halfheight;
EXTERN unsigned near times90[240], less64inc[64];
EXTERN int near moustat, mousx, mousy, bstatus;
EXTERN int near joyx1, joyy1, joyx2, joyy2, joyx3, joyy3, joystat;
EXTERN unsigned char near readch, oldreadch, keystatus[256], extended;
EXTERN int near clockspeed, slottime, slotpos[3], owecoins, owecoinwait;
EXTERN int near statusbar, statusbargoal, doorx, doory, doorstat;
EXTERN int fadehurtval, fadewarpval;
EXTERN long near ototclock, totalclock, purpletime, greentime, capetime[2];
EXTERN long near scoreclock, scorecount;
EXTERN unsigned char near textbuf[41];
EXTERN int near musicsource, midiscrap;
EXTERN unsigned long near musicstatus, count, countstop;
EXTERN unsigned near numnotes, speed, drumstat, numchans, nownote;
EXTERN unsigned int near note;
EXTERN unsigned long near chanage[18];
EXTERN unsigned char near inst[256][11], databuf[512];
EXTERN unsigned char near chanfreq[18], chantrack[18];
EXTERN unsigned char near trinst[16], trquant[16], trchan[16];
EXTERN unsigned char near trprio[16], trvol[16];
EXTERN unsigned int near snd, ksayfreq;
//EXTERN unsigned char near speechstatus; //07/03/2008: Sound Blaster crash fix. (old line)
EXTERN unsigned int near speechstatus; //07/03/2008: Sound Blaster crash fix. (new line)
EXTERN unsigned long near sndaddr;
EXTERN int near boardnum, oldlife, life, death;
EXTERN int near lifevests, lightnings, firepowers[3], keys[2];
EXTERN int near compass, cheated, coins, waterstat;
EXTERN int near animate2, animate3, animate4, oscillate3, oscillate5;
EXTERN int near animate6, animate7, animate8, animate10;
EXTERN int near animate11, animate15;
EXTERN char near xwarp[16], ywarp[16], numwarps, justwarped;
EXTERN int near bulnum, bulang[64], bulkind[64], bulchoose;
EXTERN unsigned near bulx[64], buly[64];
EXTERN unsigned long near bulstat[64], lastbulshoot, lastbarchange;
EXTERN unsigned near startx, starty, posx, posy, oldposx, oldposy;
EXTERN int near ang, startang, angvel, yourhereoldpos;
EXTERN int near vel, mxvel, myvel, svel, maxvel;
EXTERN int near posz, hvel, lastunlock, lastshoot, saidwelcome;
EXTERN unsigned near memtype[numwalls+1];
EXTERN unsigned near convavailpages, convwalls;
EXTERN unsigned near oldlogpage, oldphyspage, oldsourhand;
EXTERN unsigned near emmhandle, emmpageframe, emswalls;
EXTERN unsigned near xmshandle, xmspageframe, xmswalls, xmstruct[8];
EXTERN unsigned long near xmsvector, oldsouroff;
EXTERN char near gamehead[8][27], gamexist[8];

void interrupt far keyhandler(void);
void (interrupt far *oldkeyhandler)();
void interrupt far ksmhandler(void);
void (interrupt far *oldhand)();
void interrupt far divzerohandler(void);
void (interrupt far *olddivzerohandler)();
int initialize();
int vline(int,int,int,int,int);
int picrot(unsigned, unsigned, int, int);
int spridraw(int, int, int, int);
int pictur(int, int, int, int, int);
int doordraw(unsigned, unsigned, unsigned, unsigned, int, int, int);
int statusbardraw(unsigned, unsigned, unsigned, unsigned, unsigned,
	 unsigned, int, unsigned);
int loadboard();
int loadtables();
int ksay(unsigned);
int checkobj(unsigned, unsigned, unsigned, unsigned, int, int);
int linecompare(unsigned);
int drawlife();
int setmode360240();
int loadwalls();
int loadgame(int);
int savegame(int);
int introduction(int);
int loadmusic(char*);
int outdata(unsigned char, unsigned char, unsigned char);
int musicon();
int musicoff();
int setinst(unsigned char, int, unsigned char, unsigned char, unsigned char,
	unsigned char, unsigned char, unsigned char, unsigned char, unsigned char,
	unsigned char, unsigned char, unsigned char);
int setmidiinsts();
int checkhitwall(unsigned, unsigned, unsigned, unsigned);
int fade(int);
int emmallocate(unsigned);
int emmdeallocate(int);
int emsetpages(unsigned, unsigned);
int convallocate(unsigned);
int convdeallocate(unsigned);
int xmsallocate(int);
int xmsdeallocate(int);
int xmsmemmove(int, long, int, long, long);
int showcompass(int);
int kgif(int);
int setgamevidcomode();
int textprint(int, int, char, unsigned);
int loadstory();
int setupmouse();
int statusbaralldraw();
int hiscorecheck();
int setuptextbuf(long);
int getname();
int drawscore(long);
int drawtime(long);
int screencapture();
int mainmenu();
int getselection(int, int, int, int);
int drawmenu(int, int, int);
int creditsmenu();
int helpmenu();
int orderinfomenu();
int loadsavegamemenu(int);
int newgamemenu();
int pressakey();
int loadvms(int);
int wingame(int);
int winallgame();
int areyousure();
int copyslots(int);
int youarehere();
int copywindow(unsigned, unsigned, unsigned);

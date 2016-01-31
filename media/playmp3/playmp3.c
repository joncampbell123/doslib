/* FIXME: Quitting the program without stopping playback under Windows 3.1 Win32s causes a crash
 *        in WINMM16.DLL and causes the program manager to crash. Why? */
/* FIXME: Win32 builds crash when attempting to use the Speex decoder. Why? */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#if defined(TARGET_WINDOWS)
# include <windows.h>
# include <windows/apihelp.h>
# include <mmsystem.h>
# include "resource.h"
#endif

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/dos/doswin.h>

#if TARGET_MSDOS == 16
# error Sorry, this code does not compile 16-bit
#endif

#if defined(TARGET_WINDOWS)
/* ------------------------------------------ Windows version --------------------------------------- */
#include "playcom.h"

const char*	WndProcClass = "PLAYMP3";
HWND		hwndMain;
HINSTANCE	myInstance;
HMENU		FileMenu;
HMENU		HelpMenu;
HMENU		SysMenu;
HMENU		AppMenu;
HICON		AppIcon;
int		sc_idx = -1;
signed long	last_file_offset = -1;
HWAVEOUT	waveOut = NULL;
WAVEFORMATEX	waveOutFmt;

#define WAVEOUT_RINGSIZE	16
WAVEHDR		waveOutHdr[WAVEOUT_RINGSIZE];
int		waveOutHdrIndex = 0;

static void update_cfg();

void load_audio(WAVEHDR *hdr) {
	unsigned int patience=2000;
	signed long samp;
	uint32_t how,max,ofs;
	int rd;

	if (mp3_fd < 0) return;
	max = hdr->dwBufferLength;
	ofs = 0;

	while (ofs < max) {
		how = max - ofs;
		if (how == 0UL)
			break;

		rd = 0;
		if ((mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) && mp3_data_read < mp3_data_write) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (wav_stereo) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = (((int)(((int16_t*)mp3_data_read)[0])) + ((int)(((int16_t*)mp3_data_read)[1])) >> 1L);
							mp3_data_read += 4;
						}

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (wav_stereo) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
					else {
						/* stereo -> mono */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = (((int)(((uint8_t*)mp3_data_read)[0])) + ((int)(((uint8_t*)mp3_data_read)[1])) >> 1L);
							mp3_data_read += 2;
						}
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
					else {
						/* mono -> mono */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
				}
			}

			assert(mp3_data_read >= mp3_data);
			assert(mp3_data_read <= mp3_data_write);
		}
		if (mp3_file_type == TYPE_MP3 && mad_synth_ready && mad_synth_readofs < mad_synth.pcm.length) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (mad_synth.pcm.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							rd++;

							samp = mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							rd++;

							mad_synth_readofs++;
						}

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L)) + 1L) >> 1L;
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							mad_synth_readofs++;
							rd++;
						}

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) {
								buffer[rd] = buffer[rd+1] = -32768L;
							}
							else if (samp > 32767L) {
								buffer[rd] = buffer[rd+1] = 32767L;
							}
							else {
								buffer[rd] = buffer[rd+1] = (int16_t)samp;
							}
							mad_synth_readofs++;
							rd += 2;
						}

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							mad_synth_readofs++;
							rd++;
						}

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (mad_synth.pcm.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							rd++;

							samp = mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							rd++;

							mad_synth_readofs++;
						}
					}
					else {
						/* stereo -> mono */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + 1L) >> 1L;
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							mad_synth_readofs++;
							rd++;
						}
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) {
								buffer[rd] = buffer[rd+1] = 0;
							}
							else if (samp > 127L) {
								buffer[rd] = buffer[rd+1] = 255;
							}
							else {
								buffer[rd] = buffer[rd+1] = (uint8_t)(samp+0x80);
							}
							mad_synth_readofs++;
							rd += 2;
						}
					}
					else {
						/* mono -> mono */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							mad_synth_readofs++;
							rd++;
						}
					}
				}
			}
		}
		if (mp3_file_type == TYPE_AAC && aac_dec_last_audio != NULL && aac_dec_last_audio < aac_dec_last_audio_fence) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (aac_dec_last_info.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (int16_t)((*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 1L);

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(hdr->lpData + ofs);

				/* 16-bit out */
				if (aac_dec_last_info.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
					else {
						/* stereo -> mono */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = ((*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 9L) + 0x80;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
					else {
						/* mono -> mono */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
				}
			}
		}

		if (!mad_more()) {
			if (--patience == 0) {
				mp3_data_read++;
				break;
			}
		}

		ofs += rd;
		assert(ofs <= max);
	}
}

static void mp3_idle() {
	int patience;
	MMRESULT res;
	WAVEHDR *hdr;

	if (!mp3_playing || mp3_fd < 0)
		return;

	patience = WAVEOUT_RINGSIZE/2;
	do {
		/* when the buffer finishes playing, refill it and resubmit */
		hdr = &waveOutHdr[waveOutHdrIndex];
		if (hdr->dwFlags & WHDR_PREPARED) {
			if ((hdr->dwFlags & WHDR_DONE) && !(hdr->dwFlags & WHDR_INQUEUE)) {
				hdr->dwFlags &= ~WHDR_DONE;
				load_audio(hdr);
				if ((res=waveOutWrite(waveOut,hdr,sizeof(WAVEHDR))) != 0) {
					char tmp[256],tm2[64];

					stop_play();

					tmp[0]=0;
					waveOutGetErrorText(res,tmp,sizeof(tmp)-1);
					sprintf(tm2," %u/%u",waveOutHdrIndex,WAVEOUT_RINGSIZE);
					strcat(tmp,tm2);
					MessageBox(hwndMain,tmp,"Writing failed",MB_OK);
				}
				else {
					if (++waveOutHdrIndex >= WAVEOUT_RINGSIZE)
						waveOutHdrIndex = 0;
				}
			}
			else {
				break;
			}
		}
		else {
			if (++waveOutHdrIndex >= WAVEOUT_RINGSIZE)
				waveOutHdrIndex = 0;
		}
	} while (--patience > 0);
}

void ui_anim(int force) {
	unsigned long mp3_position = lseek(mp3_fd,0,SEEK_CUR);
	char tmp[128];
	HDC hDC;

	mp3_idle();

	if (!force && last_file_offset == mp3_position)
		return;

	hDC = GetDC(hwndMain);
	SetBkMode(hDC,OPAQUE);
	SetBkColor(hDC,RGB(255,255,255));
	SetTextColor(hDC,RGB(0,0,0));

	sprintf(tmp,"%s %uHz %s %u-bit @%lu                     ",
		mp3_playing ? "Playing" : "Stopped",
		mp3_sample_rate,
		mp3_stereo?"stereo":"mono",
		mp3_16bit?16:18,
		mp3_position);
	TextOut(hDC,0,0,tmp,strlen(tmp));

	ReleaseDC(hwndMain,hDC);
	last_file_offset = mp3_position;
}

void CALLBACK waveOutCallback(HWAVEOUT hwo,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2) {
	if (uMsg == WOM_DONE) {
		WAVEHDR *hdr = (WAVEHDR*)dwParam1;
		hdr->dwFlags |= WHDR_DONE; /* Perhaps Windows does this already, but just make sure */
	}
}

void begin_play() {
	MMRESULT res;
	int blksiz;
	int i;

	if (mp3_playing || mp3_fd < 0) return;
	if (waveOut != NULL) return;

	waveOutHdrIndex = 0;
	memset(&waveOutFmt,0,sizeof(waveOutFmt));
	waveOutFmt.wFormatTag = WAVE_FORMAT_PCM;
	waveOutFmt.nChannels = mp3_stereo ? 2 : 1;
	waveOutFmt.nSamplesPerSec = mp3_sample_rate;
	waveOutFmt.nBlockAlign = waveOutFmt.nChannels * (mp3_16bit ? 2 : 1);
	waveOutFmt.nAvgBytesPerSec = waveOutFmt.nSamplesPerSec * waveOutFmt.nBlockAlign;
	waveOutFmt.wBitsPerSample = (mp3_16bit ? 16 : 8);
	waveOutFmt.cbSize = 0;
	if ((res=waveOutOpen(&waveOut,WAVE_MAPPER,&waveOutFmt,(DWORD)waveOutCallback,0,CALLBACK_FUNCTION|WAVE_ALLOWSYNC)) != 0) {
		char tmp[256];
		tmp[0]=0;
		waveOutGetErrorText(res,tmp,sizeof(tmp)-1);
		MessageBox(hwndMain,tmp,"Unable to open wave device",MB_OK);
		return;
	}
	waveOutRestart(waveOut);
	waveOutReset(waveOut);

	mp3_data_clear();
	mad_reset_decoder();

	blksiz = waveOutFmt.nBlockAlign * (mp3_sample_rate / 16);
	for (i=0;i < WAVEOUT_RINGSIZE;i++) {
		memset(&waveOutHdr[i],0,sizeof(WAVEHDR));
		waveOutHdr[i].dwLoops = 1;
		waveOutHdr[i].dwBufferLength = blksiz;
		waveOutHdr[i].lpData = malloc(blksiz);
		if (waveOutHdr[i].lpData != NULL) {
			if (waveOutPrepareHeader(waveOut,&waveOutHdr[i],sizeof(WAVEHDR)) != 0) {
				char tmp[256];
				tmp[0]=0;
				waveOutGetErrorText(res,tmp,sizeof(tmp)-1);
				MessageBox(hwndMain,tmp,"Preparation failed",MB_OK);
			}
			else {
				load_audio(&waveOutHdr[i]);
				if (waveOutWrite(waveOut,&waveOutHdr[i],sizeof(WAVEHDR)) != 0) {
					char tmp[256];
					tmp[0]=0;
					waveOutGetErrorText(res,tmp,sizeof(tmp)-1);
					MessageBox(hwndMain,tmp,"Writing failed",MB_OK);
				}
			}
		}
	}

	mp3_playing = 1;
}

void stop_play() {
	MSG msg;
	int i;

	if (!mp3_playing) return;

	if (waveOut != NULL) {
		waveOutPause(waveOut);
		waveOutReset(waveOut);
		for (i=0;i < WAVEOUT_RINGSIZE;i++) {
			/* Microsoft Windows 3.1 + Win32s: Apparently we have to wait for WINMM16.DLL
			 * to actually stop, or our code continues execution past termination and
			 * Program Manager crashes. Wait... what? */
			while (waveOutHdr[i].dwFlags & WHDR_INQUEUE) {
				Yield();
				if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			if (waveOutHdr[i].dwFlags & WHDR_PREPARED)
				waveOutUnprepareHeader(waveOut,&waveOutHdr[i],sizeof(WAVEHDR));

			if (waveOutHdr[i].lpData)
				free(waveOutHdr[i].lpData);

			memset(&waveOutHdr[i],0,sizeof(WAVEHDR));
		}
		waveOutRestart(waveOut);
		waveOutClose(waveOut);
		waveOut = NULL;
	}

	waveOutHdrIndex = 0;
	mp3_playing = 0;
}

DialogProcType HelpAboutProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_INITDIALOG) {
		SetFocus(GetDlgItem(hwnd,IDOK));
		return 1; /* Success */
	}
	else if (message == WM_COMMAND) {
		if (HIWORD(lparam) == 0) { /* from a menu */
			switch (LOWORD(wparam)) { /* NTS: For Win16: the whole "WORD", for Win32, the lower 16 bits */
				case IDOK:
					EndDialog(hwnd,0);
					break;
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			};
		}
	}

	return 0;
}

void CALLBACK TimerProc(HWND hwnd,UINT msg,UINT idTimer,DWORD dwTime) {
	ui_anim(0);
}

void UpdatePlayStopItems() {
	EnableMenuItem(FileMenu,IDC_FILE_PLAY,MF_BYCOMMAND |
		((mp3_fd >= 0 && !mp3_playing) ? MF_ENABLED : (MF_DISABLED|MF_GRAYED)));
	EnableMenuItem(FileMenu,IDC_FILE_STOP,MF_BYCOMMAND |
		((mp3_fd >= 0 && mp3_playing) ? MF_ENABLED : (MF_DISABLED|MF_GRAYED)));

	EnableMenuItem(SysMenu,IDC_FILE_PLAY,MF_BYCOMMAND |
		((mp3_fd >= 0 && !mp3_playing) ? MF_ENABLED : (MF_DISABLED|MF_GRAYED)));
	EnableMenuItem(SysMenu,IDC_FILE_STOP,MF_BYCOMMAND |
		((mp3_fd >= 0 && mp3_playing) ? MF_ENABLED : (MF_DISABLED|MF_GRAYED)));
}

WindowProcType WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		SetTimer(NULL,1,1000/10,TimerProc);
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		stop_play();
		KillTimer(NULL,1);
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_CLOSE) {
		stop_play();
		return DefWindowProc(hwnd,message,wparam,lparam);
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_KEYDOWN) {
		if (wparam == VK_SPACE) {
			if (mp3_playing) stop_play();
			else begin_play();
			UpdatePlayStopItems();
			ui_anim(1);
		}
		else if (wparam == VK_LEFT) {
			unsigned char wp = mp3_playing;
			signed long mp3_position;
			if (wp) stop_play();
			mp3_position = lseek(mp3_fd,0,SEEK_CUR) - mp3_smallstep - sizeof(mp3_data);
			if (mp3_position < 0) mp3_position = 0;
			lseek(mp3_fd,mp3_position,SEEK_SET);
			if (wp) begin_play();
			ui_anim(1);
		}
		else if (wparam == VK_RIGHT) {
			unsigned char wp = mp3_playing;
			signed long mp3_position;
			if (wp) stop_play();
			mp3_position = lseek(mp3_fd,0,SEEK_CUR) + mp3_smallstep - sizeof(mp3_data);
			lseek(mp3_fd,mp3_position,SEEK_SET);
			if (wp) begin_play();
			ui_anim(1);
		}
		else if (wparam == VK_PRIOR) {
			unsigned char wp = mp3_playing;
			signed long mp3_position;
			if (wp) stop_play();
			mp3_position = lseek(mp3_fd,0,SEEK_CUR) - (8*mp3_smallstep) - sizeof(mp3_data);
			if (mp3_position < 0) mp3_position = 0;
			lseek(mp3_fd,mp3_position,SEEK_SET);
			if (wp) begin_play();
			ui_anim(1);
		}
		else if (wparam == VK_NEXT) {
			unsigned char wp = mp3_playing;
			signed long mp3_position;
			if (wp) stop_play();
			mp3_position = lseek(mp3_fd,0,SEEK_CUR) + (8*mp3_smallstep) - sizeof(mp3_data);
			lseek(mp3_fd,mp3_position,SEEK_SET);
			if (wp) begin_play();
			ui_anim(1);
		}
	}
	else if (message == WM_ERASEBKGND) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,FALSE)) {
			HBRUSH oldBrush,newBrush;
			HPEN oldPen,newPen;

			newPen = (HPEN)GetStockObject(NULL_PEN);
			newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

			oldPen = SelectObject((HDC)wparam,newPen);
			oldBrush = SelectObject((HDC)wparam,newBrush);

			Rectangle((HDC)wparam,um.left,um.top,um.right+1,um.bottom+1);

			SelectObject((HDC)wparam,oldBrush);
			SelectObject((HDC)wparam,oldPen);
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
	}
	else if (message == WM_SYSCOMMAND) {
		if ((wparam&0xFFF0) == (IDC_FILE_OPEN<<4)) {
			prompt_play_mp3(0);
			SetWindowText(hwndMain,mp3_file);
			UpdatePlayStopItems();
		}
		else if ((wparam&0xFFF0) == (IDC_FILE_PLAY<<4)) {
			begin_play();
			UpdatePlayStopItems();
		}
		else if ((wparam&0xFFF0) == (IDC_FILE_STOP<<4)) {
			stop_play();
			UpdatePlayStopItems();
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_COMMAND) {
		switch (LOWORD(wparam)) {
			case IDC_HELP_ABOUT:
				DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwndMain,HelpAboutProc);
				break;
			case IDC_FILE_OPEN:
				prompt_play_mp3(0);
				SetWindowText(hwndMain,mp3_file);
				UpdatePlayStopItems();
				break;
			case IDC_FILE_PLAY:
				begin_play();
				UpdatePlayStopItems();
				break;
			case IDC_FILE_STOP:
				stop_play();
				UpdatePlayStopItems();
				break;
			case IDC_FILE_QUIT:
				SendMessage(hwnd,WM_CLOSE,0,0);
				break;
		}
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;

			BeginPaint(hwnd,&ps);
			EndPaint(hwnd,&ps);
			ui_anim(1);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	mp3_16bit = 1;
	mp3_stereo = 1;
	mp3_sample_rate = 44100;
	myInstance = hInstance;

	cpu_probe();
	probe_dos();
	detect_windows();

	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) {
		MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);
		return 1;
	}

	AppMenu = CreateMenu();
	if (AppMenu == NULL) return 1;

	FileMenu = CreateMenu();
	if (!FileMenu) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED,IDC_FILE_OPEN,"&Open")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_SEPARATOR,0,"")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_DISABLED|MF_GRAYED,IDC_FILE_PLAY,"&Play")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_DISABLED|MF_GRAYED,IDC_FILE_STOP,"&Stop")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_SEPARATOR,0,"")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED,IDC_FILE_QUIT,"&Quit")) {
		return 1;
	}

	if (!AppendMenu(AppMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)FileMenu,"&File")) {
		return 1;
	}

	HelpMenu = CreateMenu();
	if (!HelpMenu) {
		return 1;
	}

	if (!AppendMenu(HelpMenu,MF_STRING|MF_ENABLED,IDC_HELP_ABOUT,"&About")) {
		return 1;
	}

	if (!AppendMenu(AppMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)HelpMenu,"&Help")) {
		return 1;
	}

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = WndProc;
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = AppIcon;
		wnd.hCursor = NULL;
		wnd.hbrBackground = NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}

	hwndMain = CreateWindow(WndProcClass,"PLAYMP3",
		WS_OVERLAPPED|WS_SYSMENU|WS_MINIMIZEBOX|WS_CAPTION,
		CW_USEDEFAULT,CW_USEDEFAULT,
		420,160,
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	SysMenu = GetSystemMenu(hwndMain,FALSE);

	if (!AppendMenu(SysMenu,MF_SEPARATOR,0,"")) {
		return 1;
	}

	if (!AppendMenu(SysMenu,MF_STRING|MF_ENABLED,IDC_FILE_PLAY<<4,"&Play")) {
		return 1;
	}

	if (!AppendMenu(SysMenu,MF_STRING|MF_ENABLED,IDC_FILE_STOP<<4,"&Stop")) {
		return 1;
	}

	if (!AppendMenu(SysMenu,MF_STRING|MF_ENABLED,IDC_FILE_OPEN<<4,"&Open file...")) {
		return 1;
	}

	SetMenu(hwndMain,AppMenu);
	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain); /* FIXME: For some reason this only causes WM_PAINT to print gibberish and cause a GPF. Why? And apparently, Windows 3.0 repaints our window anyway! */

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		ui_anim(0);
	}

	return msg.wParam;
}

/* ------------------------------------------ Windows version --------------------------------------- */
#else
/* ------------------------------------------- MS-DOS version --------------------------------------- */

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/isapnp/isapnp.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>

/* Sound Blaster */
#include <hw/sndsb/sndsb.h>
#include <hw/sndsb/sndsbpnp.h>

/* Gravis Ultrasound */
#include <hw/ultrasnd/ultrasnd.h>

#include "playcom.h"

enum {
	USE_SB=0,
	USE_GUS,
	USE_PC_SPEAKER,
	USE_LPT_DAC
};

enum {
	GOLDRATE_MATCH=0,
	GOLDRATE_DOUBLE,
	GOLDRATE_MAX
};

enum {
	LPT_DAC_STRAIGHT=0,
	LPT_DAC_DISNEY,
	LPT_DAC_MAX
};

/* Sound Blaster */
static struct dma_8237_allocation*	sb_dma = NULL; /* DMA buffer */
static struct sndsb_ctx*		sb_card = NULL;

/* Gravis Ultrasound */
static struct ultrasnd_ctx*		gus_card = NULL;
static unsigned long			gus_write = 0;

static unsigned char			dont_chain_irq = 0;
static unsigned char			dont_sb_idle = 0;
static unsigned char			dont_use_gus_dma_tc = 0;
static unsigned char			dont_use_gus_dma = 0;

/* LPT DAC */
static unsigned short			lpt_port = 0x378;
static unsigned char			lpt_dac_mode = LPT_DAC_STRAIGHT;

static unsigned long			gus_buffer_start = 4096UL;
static unsigned long			gus_buffer_length = 60UL*1024UL;
static unsigned long			gus_buffer_gap = 512;
static unsigned char			gus_channel = 0;
static unsigned char			gus_active = 14;

static unsigned char			drv_mode = 0;
static int				sc_idx = -1;
static unsigned char			goldplay_mode = 0;
static signed char			reduced_irq_interval = 0;
static unsigned char			sample_rate_timer_clamp = 0;
static unsigned char			goldplay_samplerate_choice = GOLDRATE_MATCH;
static void				(interrupt *old_irq)() = NULL;
static unsigned char			old_irq_masked = 0;

#define OUTPUT_SIZE			(32*1024)
static unsigned char*			output_buffer = NULL;
static unsigned int			output_buffer_i=0,output_buffer_o=0;

static unsigned char			pc_speaker_repeat=0;
static unsigned char			pc_speaker_repetition=0;
static unsigned int			output_max_volume;
static unsigned int			output_amplify = 32;	/* default: 2x volume */
#define output_amplify_shr 4

static const char *lpt_dac_mode_str[LPT_DAC_MAX] = {
	"Straight",
	"Disney"
};

static void interrupt irq_0() { /* timer IRQ */
	if (sb_card != NULL) {
		if (sb_card && sb_card->timer_tick_func != NULL)
			sb_card->timer_tick_func(sb_card);
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		if (mp3_playing) {
			irq_0_watchdog_do();
			if (irq_0_watchdog > 0UL) {
				outp(T8254_CONTROL_PORT,(2 << 6) | (1 << 4) | (T8254_MODE_0_INT_ON_TERMINAL_COUNT << 1)); /* MODE 0, low byte only, counter 2 */
				outp(T8254_TIMER_PORT(2),output_buffer[output_buffer_o]);
				if (pc_speaker_repetition == pc_speaker_repeat) {
					pc_speaker_repetition = 0;
					if (++output_buffer_o >= OUTPUT_SIZE)
						output_buffer_o = 0;
				}
				else {
					pc_speaker_repetition++;
				}
			}
		}
	}
	else if (drv_mode == USE_LPT_DAC) {
		if (mp3_playing) {
			irq_0_watchdog_do();
			if (irq_0_watchdog > 0UL) {
				if (lpt_dac_mode == LPT_DAC_STRAIGHT) {
					outp(lpt_port,output_buffer[output_buffer_o]);
					if (++output_buffer_o >= OUTPUT_SIZE)
						output_buffer_o = 0;
				}
				else if (lpt_dac_mode == LPT_DAC_DISNEY) {
					outp(lpt_port,output_buffer[output_buffer_o]);
					outp(lpt_port+2,0x0C);
					outp(lpt_port+2,0x04);
					if (++output_buffer_o >= OUTPUT_SIZE)
						output_buffer_o = 0;
				}
			}
		}
	}

	/* tick rate conversion. we may run the timer at a faster tick rate but the BIOS may have problems
	 * unless we chain through it's ISR at the 18.2 ticks/sec it expects to be called. If we don't,
	 * most BIOS services will work fine but some parts usually involving the floppy drive will have
	 * problems and premature timeouts, or may turn the motor off too soon.  */
	irq_0_count += irq_0_adv;
	if (irq_0_count >= irq_0_max) {
		/* NOTE TO SELF: Apparently the 32-bit protmode version
		   has to chain back to the BIOS or else keyboard input
		   doesn't work?!? */
		irq_0_count -= irq_0_max;
		old_irq_0(); /* call BIOS underneath at 18.2 ticks/sec */
	}
	else {
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
}

void draw_irq_indicator() {
	VGA_ALPHA_PTR wr = vga_alpha_ram;
	unsigned char i;

	if (sb_card != NULL) {
		*wr++ = 0x1E00 | 'S';
		*wr++ = 0x1E00 | 'B';
		*wr++ = 0x1E00 | '-';
		*wr++ = 0x1E00 | 'I';
		*wr++ = 0x1E00 | 'R';
		*wr++ = 0x1E00 | 'Q';
	}
	else if (gus_card != NULL) {
		*wr++ = 0x1E00 | 'G';
		*wr++ = 0x1E00 | 'U';
		*wr++ = 0x1E00 | 'S';
		*wr++ = 0x1E00 | 'I';
		*wr++ = 0x1E00 | 'R';
		*wr++ = 0x1E00 | 'Q';
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		*wr++ = 0x1E00 | 'P';
		*wr++ = 0x1E00 | 'C';
		*wr++ = 0x1E00 | ' ';
		*wr++ = 0x1E00 | 'S';
		*wr++ = 0x1E00 | 'P';
		*wr++ = 0x1E00 | 'K';
	}
	else if (drv_mode == USE_LPT_DAC) {
		*wr++ = 0x1E00 | 'L';
		*wr++ = 0x1E00 | 'P';
		*wr++ = 0x1E00 | 'T';
		*wr++ = 0x1E00 | 'D';
		*wr++ = 0x1E00 | 'A';
		*wr++ = 0x1E00 | 'C';
	}
	else {
		*wr++ = 0x1E00 | ' ';
		*wr++ = 0x1E00 | ' ';
		*wr++ = 0x1E00 | 'x';
		*wr++ = 0x1E00 | 'x';
		*wr++ = 0x1E00 | ' ';
		*wr++ = 0x1E00 | ' ';
	}
	for (i=0;i < 4;i++) *wr++ = (uint16_t)(i == IRQ_anim ? 'x' : '-') | 0x1E00;
}

static void interrupt sb_irq() {
	unsigned char c;

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sb_card);
	sndsb_interrupt_ack(sb_card,c);

	/* FIXME: The sndsb library should NOT do anything in
	   send_buffer_again() if it knows playback has not started! */
	/* for non-auto-init modes, start another buffer */
	if (mp3_playing) {
		/* only call send_buffer_again if 8-bit DMA completed
		   and bit 0 set, or if 16-bit DMA completed and bit 1 set */
		if ((c & 1) && !sb_card->buffer_16bit)
			sndsb_send_buffer_again(sb_card);
		else if ((c & 2) && sb_card->buffer_16bit)
			sndsb_send_buffer_again(sb_card);
	}

	sb_irq_count++;
	if (++IRQ_anim >= 4) IRQ_anim = 0;

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (old_irq_masked || old_irq == NULL || dont_chain_irq) {
		/* ack the interrupt ourself, do not chain */
		if (sb_card->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static unsigned char gus_dma_tc_ignore = 0;

static unsigned long gus_dma_tc = 0;
static unsigned long gus_irq_voice = 0;

static void interrupt gus_irq() {
	unsigned char irq_stat,c;

	do {
		irq_stat = inp(gus_card->port+6);
		if ((irq_stat & 0x80) && (!gus_dma_tc_ignore)) { // bit 7
			/* DMA TC. read and clear. */
			c = ultrasnd_select_read(gus_card,0x41);
			if (c & 0x40) {
				gus_card->dma_tc_irq_happened = 1;
				gus_dma_tc++;
			}
		}
		else if (irq_stat & 0x60) { // bits 6-5
			c = ultrasnd_select_read(gus_card,0x8F);
			if ((c & 0xC0) != 0xC0) {
				gus_irq_voice++;
			}
		}
		else {
			break;
		}
	} while (1);

	sb_irq_count++;
	if (++IRQ_anim >= 4) IRQ_anim = 0;

	if (old_irq_masked || old_irq == NULL || dont_chain_irq) {
		/* ack the interrupt ourself, do not chain */
		if (gus_card->irq1 >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static void load_audio_sb(struct sndsb_ctx *cx,uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	VGA_ALPHA_PTR wr = vga_alpha_ram + 80 - 6;
	unsigned int patience=2000;
	unsigned char load=0;
	uint16_t prev[6];
	signed long samp;
	int rd,i,bufe=0;
	uint32_t how;

	/* caller should be rounding! */
	assert((up_to & 3UL) == 0UL);
	if (up_to >= cx->buffer_size) return;
	if (cx->buffer_size < 32) return;
	if (cx->buffer_last_io == up_to) return;

	if (max == 0) max = cx->buffer_size/4;
	if (max < 16) return;
	if (mp3_fd < 0) return;

	while (max > 0UL) {
		/* the most common "hang" apparently is when IRQ 0 triggers too much
		   and then somehow execution gets stuck here */
		if (irq_0_watchdog < 16UL)
			break;

		if (up_to < cx->buffer_last_io) {
			how = (cx->buffer_size - cx->buffer_last_io); /* from last IO to end of buffer */
			bufe = 1;
		}
		else {
			if (up_to <= 8UL) break;
			how = ((up_to-8UL) - cx->buffer_last_io); /* from last IO to up_to */
			bufe = 0;
		}

		if (how > max)
			how = max;
		else if (how > 4096UL)
			how = 4096UL;
		else if (!bufe && how < min)
			break;
		else if (how == 0UL)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		rd = 0;
		if ((mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) && mp3_data_read < mp3_data_write) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (wav_stereo) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = (((int)(((int16_t*)mp3_data_read)[0])) + ((int)(((int16_t*)mp3_data_read)[1])) >> 1L);
							mp3_data_read += 4;
						}

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((int16_t*)mp3_data_read);
							mp3_data_read += 2;
						}

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (wav_stereo) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
					else {
						/* stereo -> mono */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = (((int)(((uint8_t*)mp3_data_read)[0])) + ((int)(((uint8_t*)mp3_data_read)[1])) >> 1L);
							mp3_data_read += 2;
						}
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
					else {
						/* mono -> mono */
						while (rd < how && mp3_data_read < mp3_data_write) {
							buffer[rd++] = *((uint8_t*)mp3_data_read);
							mp3_data_read++;
						}
					}
				}
			}

			assert(mp3_data_read >= mp3_data);
			assert(mp3_data_read <= mp3_data_write);
		}
		if (mp3_file_type == TYPE_MP3 && mad_synth_ready && mad_synth_readofs < mad_synth.pcm.length) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (mad_synth.pcm.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							rd++;

							samp = mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							rd++;

							mad_synth_readofs++;
						}

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L)) + 1L) >> 1L;
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							mad_synth_readofs++;
							rd++;
						}

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) {
								buffer[rd] = buffer[rd+1] = -32768L;
							}
							else if (samp > 32767L) {
								buffer[rd] = buffer[rd+1] = 32767L;
							}
							else {
								buffer[rd] = buffer[rd+1] = (int16_t)samp;
							}
							mad_synth_readofs++;
							rd += 2;
						}

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 15L);
							if (samp < -32768L) buffer[rd] = -32768L;
							else if (samp > 32767L) buffer[rd] = 32767L;
							else buffer[rd] = (int16_t)samp;
							mad_synth_readofs++;
							rd++;
						}

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (mad_synth.pcm.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							rd++;

							samp = mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							rd++;

							mad_synth_readofs++;
						}
					}
					else {
						/* stereo -> mono */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + 1L) >> 1L;
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							mad_synth_readofs++;
							rd++;
						}
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) {
								buffer[rd] = buffer[rd+1] = 0;
							}
							else if (samp > 127L) {
								buffer[rd] = buffer[rd+1] = 255;
							}
							else {
								buffer[rd] = buffer[rd+1] = (uint8_t)(samp+0x80);
							}
							mad_synth_readofs++;
							rd += 2;
						}
					}
					else {
						/* mono -> mono */
						while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
							samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
							if (samp < -128L) buffer[rd] = 0;
							else if (samp > 127L) buffer[rd] = 255;
							else buffer[rd] = (uint8_t)(samp + 0x80);
							mad_synth_readofs++;
							rd++;
						}
					}
				}
			}
		}
		if (mp3_file_type == TYPE_AAC && aac_dec_last_audio != NULL && aac_dec_last_audio < aac_dec_last_audio_fence) {
			if (mp3_16bit) {
				int16_t FAR *buffer = (int16_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (aac_dec_last_info.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
					else {
						/* stereo -> mono */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (int16_t)((*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 1L);

						rd *= 2;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
					else {
						/* mono -> mono */
						while (rd < (how>>1) && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = *aac_dec_last_audio++;

						rd *= 2;
					}
				}
			}
			else {
				/* 8-bit out */
				uint8_t FAR *buffer = (uint8_t FAR*)(sb_dma->lin + cx->buffer_last_io);

				/* 16-bit out */
				if (aac_dec_last_info.channels == 2) {
					if (mp3_stereo) {
						/* stereo -> stereo */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
					else {
						/* stereo -> mono */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = ((*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 9L) + 0x80;
					}
				}
				else {
					if (mp3_stereo) {
						/* mono -> stereo */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
					else {
						/* mono -> mono */
						while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence)
							buffer[rd++] = (*aac_dec_last_audio++ >> 8) + 0x80;
					}
				}
			}
		}

		if (!mad_more()) {
			if (--patience == 0) {
				mp3_data_read++;
				break;
			}
		}

		cx->buffer_last_io += rd;
		assert(cx->buffer_last_io <= cx->buffer_size);
		if (cx->buffer_last_io == cx->buffer_size) cx->buffer_last_io = 0;
		max -= (uint32_t)rd;
	}

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static void mad_gus_send_il(unsigned long ofs,int16_t *src,unsigned long len,unsigned int channel,unsigned int st) {
	unsigned char FAR *dst;
	unsigned long i=0;

	/* NTS: Don't worry, the GUS context has ONLY one buffer. Calling it again
	 *      returns the same buffer. This is not a memory leak */
	if ((dst = ultrasnd_dram_buffer_alloc(gus_card,4096*2)) == NULL)
		return;

	if (mp3_16bit) {
		len >>= 1;
		assert(len <= 4096UL);
		for (i=0;i < len;i++) ((int16_t*)dst)[i] = src[(i<<st)+channel];
		ultrasnd_send_dram_buffer(gus_card,ofs,len<<1,ULTRASND_DMA_DATA_SIZE_16BIT | ((!dont_use_gus_dma_tc && gus_card->irq1 >= 0) ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a,b;

			a = ultrasnd_peek(gus_card,ofs);
			b = ultrasnd_peek(gus_card,ofs+1);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length+1,b);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
	else {
		assert(len <= 4096UL);
		for (i=0;i < len;i++) ((uint8_t*)dst)[i] = (src[(i<<st)+channel] >> 8);
		ultrasnd_send_dram_buffer(gus_card,ofs,len,0 | ((!dont_use_gus_dma_tc && gus_card->irq1 >= 0) ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a;

			a = ultrasnd_peek(gus_card,ofs);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
}

static void mad_gus_send_il_dac(unsigned long ofs,void *src,unsigned long len,unsigned int channel,unsigned int st) {
	unsigned char FAR *dst;
	unsigned long i=0;

	/* NTS: Don't worry, the GUS context has ONLY one buffer. Calling it again
	 *      returns the same buffer. This is not a memory leak */
	if ((dst = ultrasnd_dram_buffer_alloc(gus_card,4096*2)) == NULL)
		return;

	if (mp3_16bit) {
		len >>= 1;
		assert(len <= 4096UL);
		for (i=0;i < len;i++) ((int16_t*)dst)[i] = ((int16_t*)src)[(i<<st)+channel];
		ultrasnd_send_dram_buffer(gus_card,ofs,len<<1,ULTRASND_DMA_DATA_SIZE_16BIT | ((!dont_use_gus_dma_tc && gus_card->irq1 >= 0) ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a,b;

			a = ultrasnd_peek(gus_card,ofs);
			b = ultrasnd_peek(gus_card,ofs+1);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length+1,b);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
	else {
		assert(len <= 4096UL);
		for (i=0;i < len;i++) ((uint8_t*)dst)[i] = ((uint8_t*)src)[(i<<st)+channel] ^ 0x80;
		ultrasnd_send_dram_buffer(gus_card,ofs,len,0 | ((!dont_use_gus_dma_tc && gus_card->irq1 >= 0) ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a;

			a = ultrasnd_peek(gus_card,ofs);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
}

static void mad_gus_send(unsigned long ofs,mad_sample_t *src,unsigned long len) {
	unsigned char FAR *dst;
	unsigned long i=0;
	signed long samp;

	/* NTS: Don't worry, the GUS context has ONLY one buffer. Calling it again
	 *      returns the same buffer. This is not a memory leak */
	if ((dst = ultrasnd_dram_buffer_alloc(gus_card,1152*2)) == NULL)
		return;

	if (mp3_16bit) {
		len >>= 1;
		assert(len <= 1152UL);
		for (i=0;i < len;i++) {
			samp = src[i] >> ((signed long)MAD_F_FRACBITS - 15L);
			if (samp < -32768L)
				((int16_t*)dst)[i] = (int16_t)(-32768);
			else if (samp > 32767L)
				((int16_t*)dst)[i] = (int16_t)(32767);
			else
				((int16_t*)dst)[i] = (int16_t)samp;
		}

		ultrasnd_send_dram_buffer(gus_card,ofs,len<<1,ULTRASND_DMA_DATA_SIZE_16BIT | ((!dont_use_gus_dma_tc && gus_card->irq1) >= 0 ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a,b;

			a = ultrasnd_peek(gus_card,ofs);
			b = ultrasnd_peek(gus_card,ofs+1);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length+1,b);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
	else {
		assert(len <= 1152UL);
		for (i=0;i < len;i++) {
			samp = src[i] >> ((signed long)MAD_F_FRACBITS - 7L);
			if (samp < -128L)
				dst[i] = (uint8_t)(-128);
			else if (samp > 127L)
				dst[i] = (uint8_t)(127);
			else
				dst[i] = (uint8_t)samp;
		}

		ultrasnd_send_dram_buffer(gus_card,ofs,len,0 | ((!dont_use_gus_dma_tc && gus_card->irq1 >= 0) ? ULTRASND_DMA_TC_IRQ : 0));

		if (ofs == 0) {
			unsigned char a;

			a = ultrasnd_peek(gus_card,ofs);
			ultrasnd_poke(gus_card,ofs+gus_buffer_length,a);
			/* we need to copy the sample to just past the end to ensure
			 * GUS interpolation does not cause "clicking" */
		}
	}
}

static void load_audio_gus(uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	VGA_ALPHA_PTR wr = vga_alpha_ram + 80 - 6;
	unsigned int patience=2000;
	unsigned char load=0;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	if (max == 0) max = gus_buffer_length/4;
	if (max < 16) return;
	if (mp3_fd < 0) return;
	up_to &= (~0x1FUL);

	while (max > 0UL) {
		if (up_to < gus_write) {
			how = (gus_buffer_length - gus_write); /* from last IO to end of buffer */
			bufe = 1;
		}
		else {
			if (up_to <= 0x20UL) break;
			how = (up_to-0x20UL) - gus_write; /* from last IO to up_to */
			bufe = 0;
		}

		if (how > 4096UL)
			how = 4096UL;
		else if (how > max)
			how = max;
		else if (!bufe && how < min)
			break;

		how &= (~0x1FUL);
		if (how == 0UL)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		rd = 0;
		if ((mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) && mp3_data_read < mp3_data_write) {
			unsigned long much = (unsigned long)(mp3_data_write - mp3_data_read) / (wav_stereo ? 2 : 1);
			if (much > how) much = how;
			mad_gus_send_il_dac(gus_write+gus_buffer_start,mp3_data_read,much,0,wav_stereo?1:0);
			if (mp3_stereo) mad_gus_send_il_dac(gus_write+gus_buffer_start+gus_buffer_length+gus_buffer_gap,mp3_data_read,much,1,wav_stereo?1:0);
			mp3_data_read += much * (wav_stereo ? 2 : 1);
			rd = much;

			assert(mp3_data_read >= mp3_data);
			assert(mp3_data_read <= mp3_data_write);
		}
		if (mp3_file_type == TYPE_MP3 && mad_synth_ready && mad_synth_readofs < mad_synth.pcm.length) {
			unsigned int rem = mad_synth.pcm.length - mad_synth_readofs;
			assert(mad_synth.pcm.length <= 1152);
			if (mp3_16bit && how > (rem*2)) how = rem*2;
			if (!mp3_16bit && how > rem) how = rem;
			if (mp3_16bit) assert((how & 1) == 0);
			mad_gus_send(gus_write+gus_buffer_start,mad_synth.pcm.samples[0]+mad_synth_readofs,how);
			if (mp3_stereo) mad_gus_send(gus_write+gus_buffer_start+gus_buffer_length+gus_buffer_gap,mad_synth.pcm.samples[mad_frame.header.mode != MAD_MODE_SINGLE_CHANNEL?1:0]+mad_synth_readofs,how);
			mad_synth_readofs += (how >> (mp3_16bit?1:0));
			rd = how;
		}
		if (mp3_file_type == TYPE_AAC && aac_dec_last_audio != NULL && aac_dec_last_audio < aac_dec_last_audio_fence) {
			unsigned long much = (unsigned long)(aac_dec_last_audio_fence - aac_dec_last_audio) / aac_dec_last_info.channels;
			if (mp3_16bit) much *= 2;
			if (much > how) much = how;
			mad_gus_send_il(gus_write+gus_buffer_start,aac_dec_last_audio,much,0,aac_dec_last_info.channels==2?1:0);
			if (mp3_stereo) mad_gus_send_il(gus_write+gus_buffer_start+gus_buffer_length+gus_buffer_gap,aac_dec_last_audio,much,1,aac_dec_last_info.channels==2?1:0);
			aac_dec_last_audio += (much >> (mp3_16bit?1:0)) * aac_dec_last_info.channels;
			assert(aac_dec_last_audio <= aac_dec_last_audio_fence);
			rd = much;
		}

		if (!mad_more()) {
			if (--patience == 0) {
				mp3_data_read++;
				break;
			}
		}

		gus_write += rd;
		assert(gus_write <= gus_buffer_length);
		if (gus_write == gus_buffer_length) gus_write = 0;
		max -= (uint32_t)rd;
	}

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static void load_audio_pc_speaker(uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	VGA_ALPHA_PTR wr = vga_alpha_ram + 80 - 6;
	unsigned int patience=2000;
	unsigned char load=0;
	signed long samp;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	if (max == 0) max = OUTPUT_SIZE/4;
	if (max < 16) return;
	if (mp3_fd < 0) return;
	up_to &= (~3UL);

	while (max > 0UL) {
		if (up_to < output_buffer_i) {
			how = (OUTPUT_SIZE - output_buffer_i); /* from last IO to end of buffer */
			bufe = 1;
		}
		else {
			if (up_to <= 8UL) break;
			how = (up_to-8UL) - output_buffer_i; /* from last IO to up_to */
			bufe = 0;
		}

		if (how > max)
			how = max;
		else if (how > 4096UL)
			how = 4096UL;
		else if (!bufe && how < min)
			break;
		else if (how == 0UL)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		rd = 0;
		if ((mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) && mp3_data_read < mp3_data_write) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;

			if (mp3_16bit) {
				assert(0); /* begin_play() should force 16-bit mode off! */
			}
			else if (mp3_stereo) {
				assert(0); /* begin_play() should force stereo off! */
			}
			else {
				/* 8-bit out */
				if (wav_stereo) {
					/* stereo -> mono */
					while (rd < how && mp3_data_read < mp3_data_write) {
						samp = ((int)*mp3_data_read++) - 0x80;
						samp += ((int)*mp3_data_read++) - 0x80;
						samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1 + 1);
						samp += output_max_volume >> 1;
						if (samp < 1L) buffer[rd] = 1;
						else if (samp > output_max_volume) buffer[rd] = output_max_volume;
						else buffer[rd] = (uint8_t)samp;
						rd++;
					}
				}
				else {
					/* mono -> mono */
					while (rd < how && mp3_data_read < mp3_data_write) {
						samp = ((int)*mp3_data_read++) - 0x80;
						samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1);
						samp += output_max_volume >> 1;
						if (samp < 1L) buffer[rd] = 1;
						else if (samp > output_max_volume) buffer[rd] = output_max_volume;
						else buffer[rd] = (uint8_t)samp;
						rd++;
					}
				}
			}

			assert(mp3_data_read >= mp3_data);
			assert(mp3_data_read <= mp3_data_write);
		}
		if (mp3_file_type == TYPE_MP3 && mad_synth_ready && mad_synth_readofs < mad_synth.pcm.length) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;
			unsigned int rem = mad_synth.pcm.length - mad_synth_readofs;
			assert(mad_synth.pcm.length <= 1152);
			if (how > rem) how = rem;

			/* 8-bit out */
			if (mad_synth.pcm.channels == 2) {
				/* stereo -> mono */
				while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
					samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + 1L) >> 1L;
					samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1);
					samp += output_max_volume >> 1;
					if (samp < 1L) buffer[rd] = 1;
					else if (samp > output_max_volume) buffer[rd] = output_max_volume;
					else buffer[rd] = (uint8_t)samp;
					mad_synth_readofs++;
					rd++;
				}
			}
			else {
				/* mono -> mono */
				while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
					samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
					samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1);
					samp += output_max_volume >> 1;
					if (samp < 1L) buffer[rd] = 1;
					else if (samp > output_max_volume) buffer[rd] = output_max_volume;
					else buffer[rd] = (uint8_t)samp;
					mad_synth_readofs++;
					rd++;
				}
			}

			rd = how;
		}
		if (mp3_file_type == TYPE_AAC && aac_dec_last_audio != NULL && aac_dec_last_audio < aac_dec_last_audio_fence) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;

			if (aac_dec_last_info.channels >= 2) {
				/* stereo -> mono */

				while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence) {
					samp = (*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 9;
					samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1);
					samp += output_max_volume >> 1;
					if (samp < 1L) buffer[rd] = 1;
					else if (samp > output_max_volume) buffer[rd] = output_max_volume;
					else buffer[rd] = (uint8_t)samp;
					rd++;
				}
			}
			else {
				/* mono -> mono */
				while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence) {
					samp = *aac_dec_last_audio++ >> 8;
					samp = (samp * (signed long)output_max_volume * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 8 + 1);
					samp += output_max_volume >> 1;
					if (samp < 1L) buffer[rd] = 1;
					else if (samp > output_max_volume) buffer[rd] = output_max_volume;
					else buffer[rd] = (uint8_t)samp;
					rd++;
				}
			}
		}

		if (!mad_more()) {
			if (--patience == 0) {
				mp3_data_read++;
				break;
			}
		}

		output_buffer_i += rd;
		assert(output_buffer_i <= OUTPUT_SIZE);
		if (output_buffer_i == OUTPUT_SIZE) output_buffer_i = 0;
		max -= (uint32_t)rd;
	}

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static void load_audio_dac8(uint32_t up_to,uint32_t min,uint32_t max,uint8_t initial) { /* load audio up to point or max */
	VGA_ALPHA_PTR wr = vga_alpha_ram + 80 - 6;
	unsigned int patience=2000;
	unsigned char load=0;
	signed long samp;
	uint16_t prev[6];
	int rd,i,bufe=0;
	uint32_t how;

	if (max == 0) max = OUTPUT_SIZE/4;
	if (max < 16) return;
	if (mp3_fd < 0) return;
	up_to &= (~3UL);

	while (max > 0UL) {
		if (up_to < output_buffer_i) {
			how = (OUTPUT_SIZE - output_buffer_i); /* from last IO to end of buffer */
			bufe = 1;
		}
		else {
			if (up_to <= 8UL) break;
			how = (up_to-8UL) - output_buffer_i; /* from last IO to up_to */
			bufe = 0;
		}

		if (how > max)
			how = max;
		else if (how > 4096UL)
			how = 4096UL;
		else if (!bufe && how < min)
			break;
		else if (how == 0UL)
			break;

		if (!load) {
			load = 1;
			prev[0] = wr[0];
			wr[0] = '[' | 0x0400;
			prev[1] = wr[1];
			wr[1] = 'L' | 0x0400;
			prev[2] = wr[2];
			wr[2] = 'O' | 0x0400;
			prev[3] = wr[3];
			wr[3] = 'A' | 0x0400;
			prev[4] = wr[4];
			wr[4] = 'D' | 0x0400;
			prev[5] = wr[5];
			wr[5] = ']' | 0x0400;
		}

		rd = 0;
		if ((mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) && mp3_data_read < mp3_data_write) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;

			if (mp3_16bit) {
				assert(0); /* begin_play() should force 16-bit mode off! */
			}
			else if (mp3_stereo) {
				assert(0); /* begin_play() should force stereo off! */
			}
			else {
				/* 8-bit out */
				if (wav_stereo) {
					/* stereo -> mono */
					while (rd < how && mp3_data_read < mp3_data_write) {
						samp = ((int)*mp3_data_read++) - 0x80;
						samp += ((int)*mp3_data_read++) - 0x80;
						samp = (samp * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 1 + 1);
						samp += 0x80;
						if (samp < 0L) buffer[rd] = 0;
						else if (samp > 255) buffer[rd] = 255;
						else buffer[rd] = (uint8_t)samp;
						rd++;
					}
				}
				else {
					/* mono -> mono */
					while (rd < how && mp3_data_read < mp3_data_write) {
						samp = ((int)*mp3_data_read++) - 0x80;
						samp = (samp * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 1);
						samp += 0x80;
						if (samp < 0L) buffer[rd] = 0;
						else if (samp > 255) buffer[rd] = 255;
						else buffer[rd] = (uint8_t)samp;
						rd++;
					}
				}
			}

			assert(mp3_data_read >= mp3_data);
			assert(mp3_data_read <= mp3_data_write);
		}
		if (mp3_file_type == TYPE_MP3 && mad_synth_ready && mad_synth_readofs < mad_synth.pcm.length) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;
			unsigned int rem = mad_synth.pcm.length - mad_synth_readofs;
			assert(mad_synth.pcm.length <= 1152);
			if (how > rem) how = rem;

			/* 8-bit out */
			if (mad_synth.pcm.channels == 2) {
				/* stereo -> mono */
				while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
					samp = ((mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + (mad_synth.pcm.samples[1][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L)) + 1L) >> 1L;
					samp = (samp * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 1);
					samp += 0x80;
					if (samp < 0L) buffer[rd] = 0;
					else if (samp > 255) buffer[rd] = 255;
					else buffer[rd] = (uint8_t)samp;
					mad_synth_readofs++;
					rd++;
				}
			}
			else {
				/* mono -> mono */
				while (rd < how && mad_synth_readofs < mad_synth.pcm.length) {
					samp = mad_synth.pcm.samples[0][mad_synth_readofs] >> ((signed long)MAD_F_FRACBITS - 7L);
					samp = (samp * (signed long)(output_amplify)) >> (signed long)(output_amplify_shr + 1);
					samp += 0x80;
					if (samp < 0L) buffer[rd] = 0;
					else if (samp > 255) buffer[rd] = 255;
					else buffer[rd] = (uint8_t)samp;
					mad_synth_readofs++;
					rd++;
				}
			}

			rd = how;
		}
		if (mp3_file_type == TYPE_AAC && aac_dec_last_audio != NULL && aac_dec_last_audio < aac_dec_last_audio_fence) {
			uint8_t *buffer = (uint8_t*)output_buffer + output_buffer_i;

			if (aac_dec_last_info.channels >= 2) {
				/* stereo -> mono */

				while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence) {
					buffer[rd] = ((*aac_dec_last_audio++ + *aac_dec_last_audio++) >> 9)+0x80;
					rd++;
				}
			}
			else {
				/* mono -> mono */
				while (rd < how && aac_dec_last_audio < aac_dec_last_audio_fence) {
					buffer[rd] = (*aac_dec_last_audio++ >> 8)+0x80;
					rd++;
				}
			}
		}

		if (!mad_more()) {
			if (--patience == 0) {
				mp3_data_read++;
				break;
			}
		}

		output_buffer_i += rd;
		assert(output_buffer_i <= OUTPUT_SIZE);
		if (output_buffer_i == OUTPUT_SIZE) output_buffer_i = 0;
		max -= (uint32_t)rd;
	}

	if (load) {
		for (i=0;i < 6;i++)
			wr[i] = prev[i];
	}
}

static void update_cfg();

static void mp3_idle() {
	if (!mp3_playing || mp3_fd < 0)
		return;

	/* if we're playing without an IRQ handler, then we'll want this function
	 * to poll the sound card's IRQ status and handle it directly so playback
	 * continues to work. if we don't, playback will halt on actual Creative
	 * Sound Blaster 16 hardware until it gets the I/O read to ack the IRQ */
	if (!dont_sb_idle) sndsb_main_idle(sb_card);

	/* load from disk */
	if (sb_card != NULL) {
		const unsigned int leeway = 2048;
		uint32_t pos;

		_cli();
		pos = sndsb_read_dma_buffer_position(sb_card);
		if (pos < leeway) pos = 0UL;
		else pos -= leeway;
		pos &= (~3UL); /* round down */

		_sti();

		load_audio_sb(sb_card,pos,max(mp3_sample_rate/8,1152*mp3_bytes_per_sample)/*min*/,
			sb_card->buffer_size/4/*max*/,0/*first block*/);
	}
	else if (gus_card != NULL) {
		unsigned long gus_pos;

		_cli();
		gus_pos = ultrasnd_read_voice_current(gus_card,gus_channel) - gus_buffer_start;
		gus_pos -= 4096UL;
		if ((signed long)gus_pos < 0) gus_pos = 0;
		gus_pos &= (~3UL);
		_sti();

		load_audio_gus(gus_pos/*up to*/,min(4096UL,gus_buffer_length/8)/*min*/,min(32768UL,gus_buffer_length/2)/*max*/,0/*first block*/);
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		load_audio_pc_speaker(output_buffer_o,min(4096UL,gus_buffer_length/8)/*min*/,min(32768UL,gus_buffer_length/2)/*max*/,0/*first block*/);
	}
	else if (drv_mode == USE_LPT_DAC) {
		load_audio_dac8(output_buffer_o,min(4096UL,gus_buffer_length/8)/*min*/,min(32768UL,gus_buffer_length/2)/*max*/,0/*first block*/);
	}
}

void ui_anim(int force) {
	unsigned long mp3_position = lseek(mp3_fd,0,SEEK_CUR);
	VGA_ALPHA_PTR wr = vga_alpha_ram + 10;
	const unsigned int width = 70 - 4;
	const char *card_name = "?";
	unsigned int i,rem,rem2,cc;
	const char *msg = "DMA:";
	unsigned int percent;

	if (sb_card != NULL)
		card_name = "SB";
	else if (gus_card != NULL)
		card_name = "GUS";
	else if (drv_mode == USE_PC_SPEAKER)
		card_name = "PCSPK";
	else if (drv_mode == USE_LPT_DAC)
		card_name = "LPTDAC";

	mp3_idle();

	rem = 0;
	if (sb_card != NULL) {
		rem = sndsb_read_dma_buffer_position(sb_card);
	}
	else if (gus_card != NULL) {
		rem = ultrasnd_read_voice_current(gus_card,gus_channel) - gus_buffer_start;
		if ((signed long)rem < 0) rem = 0;
	}
	else if (drv_mode == USE_PC_SPEAKER || drv_mode == USE_LPT_DAC) {
		rem = output_buffer_o;
	}

	if (force || last_dma_position != rem) {
		if (mp3_playing)
			mp3_position += (unsigned long)(mp3_data_read - mp3_data);

		last_dma_position = rem;
		if (rem != 0) rem--;
		if (sb_card != NULL)
			rem = (unsigned int)(((unsigned long)rem * (unsigned long)width) / (unsigned long)sb_card->buffer_size);
		else if (gus_card != NULL)
			rem = (unsigned int)(((unsigned long)rem * (unsigned long)width) / (unsigned long)gus_buffer_length);
		else if (drv_mode == USE_PC_SPEAKER || drv_mode == USE_LPT_DAC)
			rem = (unsigned int)(((unsigned long)rem * (unsigned long)width) / (unsigned long)OUTPUT_SIZE);

		rem2 = 0;
		if (sb_card != NULL) {
			rem2 = sb_card->buffer_last_io;
			if (rem2 != 0) rem2--;
			rem2 = (unsigned int)(((unsigned long)rem2 * (unsigned long)width) / (unsigned long)sb_card->buffer_size);
		}
		else if (gus_card != NULL) {
			rem2 = gus_write;
			if (rem2 != 0) rem2--;
			rem2 = (unsigned int)(((unsigned long)rem2 * (unsigned long)width) / (unsigned long)gus_buffer_length);
		}
		else if (drv_mode == USE_PC_SPEAKER || drv_mode == USE_LPT_DAC) {
			rem2 = output_buffer_i;
			if (rem2 != 0) rem2--;
			rem2 = (unsigned int)(((unsigned long)rem2 * (unsigned long)width) / (unsigned long)OUTPUT_SIZE);
		}

		while (*msg) *wr++ = (uint16_t)(*msg++) | 0x1E00;
		for (i=0;i < width;i++) {
			if (i == rem2)
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' ')) | 0x7000;
			else
				wr[i] = (uint16_t)(i == rem ? 'x' : (i < rem ? '-' : ' ')) | 0x1E00;
		}

		if (mp3_data_length != 0) {
			percent =
				(unsigned int)(((mp3_position >> 12UL) * 100UL) / ((mp3_data_length+0xFFFUL)>>12UL));
		}
		else {
			percent = 0;
		}

		msg = temp_str;
		sprintf(temp_str,"%s %ub %s %5luHz %c @%lu %%%u",card_name,mp3_16bit ? 16 : 8,mp3_stereo ? "ST" : "MO",
			mp3_sample_rate,mp3_playing ? 'p' : 's',mp3_position,percent);
		for (wr=vga_alpha_ram+(80*1),cc=0;cc < 36 && *msg != 0;cc++) *wr++ = 0x1F00 | ((unsigned char)(*msg++));
		if (drv_mode == USE_PC_SPEAKER || drv_mode == USE_LPT_DAC) {
			sprintf(temp_str," x%.2f",(double)output_amplify / (1 << output_amplify_shr));
			msg = temp_str;
			while (*msg != 0) *wr++ = 0x1F00 | ((unsigned char)(*msg++));
		}
		for (;cc < 42;cc++) *wr++ = 0x1F20;
		if (sb_card != NULL) {
			msg = sndsb_dspoutmethod_str[sb_card->dsp_play_method];
			rem = sndsb_dsp_out_method_supported(sb_card,mp3_sample_rate,mp3_stereo,mp3_16bit) ? 0x1A : 0x1C;
		}
		for (;cc < 56 && *msg != 0;cc++) *wr++ = (rem << 8) | ((unsigned char)(*msg++));
		for (;cc < 56;cc++) *wr++ = 0x1F20;
		for (;cc < 62;cc++) *wr++ = 0x1F20;

		/* finish the row */
		for (;cc < 80;cc++) *wr++ = 0x1F20;
	}

	irq_0_watchdog_ack();
	draw_irq_indicator();

	{
		static const unsigned char anims[] = {'-','/','|','\\'};
		if (++animator >= 4) animator = 0;
		wr = vga_alpha_ram + 80 + 79;
		*wr = anims[animator] | 0x1E00;
	}
}

void begin_play() {
	if (mp3_playing || mp3_fd < 0) return;

	if (sb_card != NULL) { /* Sound Blaster */
		unsigned long choice_rate = sample_rate_timer_clamp ? mp3_sample_rate_by_timer : mp3_sample_rate;
		if (goldplay_mode) {
			if (goldplay_samplerate_choice == GOLDRATE_DOUBLE)
				choice_rate *= 2;
			else if (goldplay_samplerate_choice == GOLDRATE_MAX) {
				/* basically the maximum the DSP will run at */
				if (sb_card->dsp_play_method <= SNDSB_DSPOUTMETHOD_200)
					choice_rate = 22050;
				else if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_201)
					choice_rate = 44100;
				else if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_3xx)
					choice_rate = mp3_stereo ? 22050 : 44100;
				else if (sb_card->pnp_name == NULL)
					choice_rate = 44100; /* Most clones and non-PnP SB16 cards max out at 44.1KHz */
				else
					choice_rate = 48000; /* SB16 ViBRA (PnP-era) cards max out at 48Khz */
			}
		}

		update_cfg();
		irq_0_watchdog_reset();
		if (!sndsb_prepare_dsp_playback(sb_card,choice_rate,mp3_stereo,mp3_16bit))
			return;

		mp3_data_clear();
		mad_reset_decoder();
		sndsb_setup_dma(sb_card);
		memset(sb_dma->lin,mp3_16bit?0:0x80,sb_card->buffer_size);
		load_audio_sb(sb_card,sb_card->buffer_size/2,0/*min*/,0/*max*/,1/*first block*/);
		if (!sndsb_begin_dsp_playback(sb_card))
			return;

		_cli();
		if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
			unsigned long nr = (unsigned long)sb_card->buffer_rate * 2UL;
			write_8254_system_timer(t8254_us2ticks(1000000UL / nr));
			irq_0_count = 0;
			irq_0_adv = 182UL;		/* 18.2Hz */
			irq_0_max = nr * 10UL;		/* sample rate */
		}
		else if (sb_card->goldplay_mode) {
			write_8254_system_timer(mp3_sample_rate_by_timer_ticks);
			irq_0_count = 0;
			irq_0_adv = 182UL;		/* 18.2Hz */
			irq_0_max = (T8254_REF_CLOCK_HZ / mp3_sample_rate_by_timer_ticks) * 10UL;
		}

		draw_irq_indicator();
		mp3_playing = 1;
		_sti();
	}
	else if (gus_card != NULL) {
		_cli();
		update_cfg();
		irq_0_watchdog_reset();
		mp3_data_clear();
		mad_reset_decoder();

		ultrasnd_abort_dma_transfer(gus_card);
		ultrasnd_stop_all_voices(gus_card);
		ultrasnd_stop_timers(gus_card);

		gus_write = 0;
		load_audio_gus((gus_buffer_length/2UL)/*up to*/,0/*min*/,0/*max*/,1/*first block*/);

		ultrasnd_stop_voice(gus_card,gus_channel);
		ultrasnd_set_active_voices(gus_card,gus_active);
		ultrasnd_set_voice_mode(gus_card,gus_channel,ULTRASND_VOICE_MODE_LOOP | (mp3_16bit?ULTRASND_VOICE_MODE_16BIT:0) | ULTRASND_VOICE_MODE_STOP);
		ultrasnd_set_voice_fc(gus_card,gus_channel,ultrasnd_sample_rate_to_fc(gus_card,mp3_sample_rate));
		ultrasnd_set_voice_ramp_rate(gus_card,gus_channel,0,0);
		ultrasnd_set_voice_ramp_start(gus_card,gus_channel,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
		ultrasnd_set_voice_ramp_end(gus_card,gus_channel,0xF0);
		ultrasnd_set_voice_volume(gus_card,gus_channel,0xFFF0); /* full vol */
		ultrasnd_set_voice_pan(gus_card,gus_channel,mp3_stereo?0:7);
		ultrasnd_set_voice_ramp_control(gus_card,gus_channel,0);
		ultrasnd_set_voice_start(gus_card,gus_channel,gus_buffer_start);
		ultrasnd_set_voice_end(gus_card,gus_channel,gus_buffer_start+gus_buffer_length-(mp3_16bit?2:1));
		ultrasnd_set_voice_current(gus_card,gus_channel,gus_buffer_start);
		ultrasnd_set_voice_mode(gus_card,gus_channel,ULTRASND_VOICE_MODE_LOOP | (mp3_16bit?ULTRASND_VOICE_MODE_16BIT:0) |
			(gus_card->irq1 >= 0 ? ULTRASND_VOICE_MODE_IRQ : 0));

		if (mp3_stereo) {
			ultrasnd_stop_voice(gus_card,gus_channel+1);
			ultrasnd_set_voice_mode(gus_card,gus_channel+1,ULTRASND_VOICE_MODE_LOOP | (mp3_16bit?ULTRASND_VOICE_MODE_16BIT:0) | ULTRASND_VOICE_MODE_STOP);
			ultrasnd_set_voice_fc(gus_card,gus_channel+1,ultrasnd_sample_rate_to_fc(gus_card,mp3_sample_rate));
			ultrasnd_set_voice_ramp_rate(gus_card,gus_channel+1,0,0);
			ultrasnd_set_voice_ramp_start(gus_card,gus_channel+1,0xF0); /* NTS: You have to set the ramp start/end because it will override your current volume */
			ultrasnd_set_voice_ramp_end(gus_card,gus_channel+1,0xF0);
			ultrasnd_set_voice_volume(gus_card,gus_channel+1,0xFFF0); /* full vol */
			ultrasnd_set_voice_pan(gus_card,gus_channel+1,15);
			ultrasnd_set_voice_ramp_control(gus_card,gus_channel+1,0);
			ultrasnd_set_voice_start(gus_card,gus_channel+1,gus_buffer_start+gus_buffer_length+gus_buffer_gap);
			ultrasnd_set_voice_end(gus_card,gus_channel+1,gus_buffer_start+gus_buffer_length+gus_buffer_gap+gus_buffer_length-(mp3_16bit?2:1));
			ultrasnd_set_voice_current(gus_card,gus_channel+1,gus_buffer_start+gus_buffer_length+gus_buffer_gap);
			ultrasnd_set_voice_mode(gus_card,gus_channel+1,ULTRASND_VOICE_MODE_LOOP | (mp3_16bit?ULTRASND_VOICE_MODE_16BIT:0) |
				(gus_card->irq1 >= 0 ? ULTRASND_VOICE_MODE_IRQ : 0));
		}

		ultrasnd_start_voice_imm(gus_card,gus_channel);
		if (mp3_stereo) ultrasnd_start_voice_imm(gus_card,gus_channel+1);
		mp3_playing = 1;
		_sti();
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		_cli();
		output_buffer_i = 0;
		output_buffer_o = 0;
		pc_speaker_repeat=0;
		pc_speaker_repetition=0;
		mp3_16bit = 0;
		mp3_stereo = 0;
		mp3_data_clear();
		mad_reset_decoder();

		if (output_buffer == NULL) {
			if ((output_buffer = malloc(OUTPUT_SIZE)) == NULL) {
				fprintf(stderr,"Cannot alloc output buffer\n");
				_sti();
				return;
			}
		}

		irq_0_watchdog_reset();
		load_audio_pc_speaker((OUTPUT_SIZE/2UL)/*up to*/,0/*min*/,0/*max*/,1/*first block*/);
		{
			unsigned long final,timer_ticks;
			while ((mp3_sample_rate*(pc_speaker_repeat+1)) < 19000UL) pc_speaker_repeat++;
			final = mp3_sample_rate*(pc_speaker_repeat+1);
			timer_ticks = T8254_REF_CLOCK_HZ / final;
			if (timer_ticks == 0) timer_ticks = 1;
			output_max_volume = (unsigned int)timer_ticks;
			write_8254_system_timer(timer_ticks);
			irq_0_count = 0;
			irq_0_adv = 182UL;		/* 18.2Hz */
			irq_0_max = (T8254_REF_CLOCK_HZ / timer_ticks) * 10UL;
		}
		t8254_pc_speaker_set_gate(3);
		write_8254_pc_speaker(1);
		mp3_playing = 1;
		_sti();
	}
	else if (drv_mode == USE_LPT_DAC) {
		_cli();
		output_buffer_i = 0;
		output_buffer_o = 0;
		mp3_16bit = 0;
		mp3_stereo = 0;
		mp3_data_clear();
		mad_reset_decoder();

		if (output_buffer == NULL) {
			if ((output_buffer = malloc(OUTPUT_SIZE)) == NULL) {
				fprintf(stderr,"Cannot alloc output buffer\n");
				_sti();
				return;
			}
		}

		irq_0_watchdog_reset();
		load_audio_dac8((OUTPUT_SIZE/2UL)/*up to*/,0/*min*/,0/*max*/,1/*first block*/);
		{
			unsigned long timer_ticks = t8254_us2ticks(1000000 / mp3_sample_rate);
			if (timer_ticks == 0) timer_ticks = 1;
			output_max_volume = (unsigned int)timer_ticks;
			write_8254_system_timer(timer_ticks);
			irq_0_count = 0;
			irq_0_adv = 182UL;		/* 18.2Hz */
			irq_0_max = (T8254_REF_CLOCK_HZ / timer_ticks) * 10UL;
		}

		if (lpt_dac_mode == LPT_DAC_DISNEY)
			outp(lpt_port+2,0x04);

		mp3_playing = 1;
		_sti();
	}
}

void stop_play() {
	if (!mp3_playing) return;

	if (sb_card != NULL) { /* Sound Blaster */
		_cli();
		if (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT || sb_card->goldplay_mode) {
			irq_0_count = 0;
			irq_0_adv = 1;
			irq_0_max = 1;
			write_8254_system_timer(0); /* restore 18.2 tick/sec */
		}
		sndsb_stop_dsp_playback(sb_card);
		mp3_playing = 0;
		ui_anim(1);
		_sti();
	}
	else if (gus_card != NULL) {
		_cli();
		if (mp3_stereo) ultrasnd_stop_voice(gus_card,gus_channel+1);
		ultrasnd_stop_voice(gus_card,gus_channel);
		mp3_playing = 0;
		ui_anim(1);
		_sti();
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		_cli();
		{
			irq_0_count = 0;
			irq_0_adv = 1;
			irq_0_max = 1;
			write_8254_system_timer(0); /* restore 18.2 tick/sec */
		}
		t8254_pc_speaker_set_gate(0);
		write_8254_pc_speaker(0);
		mp3_playing = 0;
		ui_anim(1);
		_sti();
	}
	else if (drv_mode == USE_LPT_DAC) {
		_cli();
		{
			irq_0_count = 0;
			irq_0_adv = 1;
			irq_0_max = 1;
			write_8254_system_timer(0); /* restore 18.2 tick/sec */
		}

		if (lpt_dac_mode == LPT_DAC_DISNEY)
			outp(lpt_port+2,0x0C);

		mp3_playing = 0;
		ui_anim(1);
		_sti();
	}
}

static void change_param_menu() {
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	unsigned char selector=change_param_idx;
	unsigned char oldmethod=sb_card != NULL ? sb_card->dsp_play_method : 0;
	unsigned int cc,ra;
	VGA_ALPHA_PTR vga;
	char tmp[128];

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
				for (vga=vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
				ui_anim(1);
			}
			vga_moveto(0,4);

			vga_write_color(selector == 0 ? 0x70 : 0x1F);
			sprintf(tmp,"Sample rate:   %uHz",mp3_sample_rate);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 1 ? 0x70 : 0x1F);
			sprintf(tmp,"Channels:      %s",mp3_stereo ? "stereo" : "mono");
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			vga_write_color(selector == 2 ? 0x70 : 0x1F);
			sprintf(tmp,"Bits:          %u-bit",mp3_16bit ? 16 : 8);
			vga_write(tmp);
			vga_write_until(30);
			vga_write("\n");

			if (sb_card != NULL) {
				vga_write_color(selector == 3 ? 0x70 : 0x1F);
				vga_write(  "Translation:   ");
				vga_write("None");
				vga_write_until(30);
				vga_write("\n");

				vga_write_color(selector == 4 ? 0x70 : 0x1F);
				vga_write(  "DSP mode:      ");
				if (sndsb_dsp_out_method_supported(sb_card,mp3_sample_rate,mp3_stereo,mp3_16bit))
					vga_write_color(selector == 4 ? 0x70 : 0x1F);
				else
					vga_write_color(selector == 4 ? 0x74 : 0x1C);
				vga_write(sndsb_dspoutmethod_str[sb_card->dsp_play_method]);
				vga_write_until(30);
				vga_write("\n");
			}
			else if (gus_card != NULL) {
				vga_write_color(selector == 3 ? 0x70 : 0x1F);
				vga_write(  "GUS channel:   ");
				sprintf(tmp,"%u",gus_channel);
				vga_write(tmp);
				vga_write_until(30);
				vga_write("\n");

				vga_write_color(selector == 4 ? 0x70 : 0x1F);
				vga_write(  "Active chans:  ");
				sprintf(tmp,"%u",gus_active);
				vga_write(tmp);
				vga_write_until(30);
				vga_write("\n");
			}
			else if (drv_mode == USE_LPT_DAC) {
				vga_write_color(selector == 3 ? 0x70 : 0x1F);
				vga_write(  "DAC mode:      ");
				sprintf(tmp,"%s",lpt_dac_mode_str[lpt_dac_mode]);
				vga_write(tmp);
				vga_write_until(30);
				vga_write("\n");

				vga_write_color(selector == 4 ? 0x70 : 0x1F);
				vga_write(  "               ");
				vga_write("");
				vga_write_until(30);
				vga_write("\n");
			}

			vga_write("\n");
			vga_write_sync();
			_sti();
			redraw = 0;
			uiredraw = 0;
		}

		if (kbhit()) {
			int c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27 || c == 13)
				loop = 0;
			else if (isdigit(c)) {
				if (selector == 0) { /* sample rate, allow typing in sample rate */
					int i=0;
					VGA_ALPHA_PTR sco;
					struct vga_msg_box box;
					vga_msg_box_create(&box,"Custom sample rate",2,0);
					sco = vga_alpha_ram + ((box.y+2) * vga_width) + box.x + 2;
					sco[i] = c | 0x1E00;
					temp_str[i++] = c;
					while (1) {
						c = getch();
						if (c == 0) c = getch() << 8;

						if (c == 27)
							break;
						else if (c == 13) {
							if (i == 0) break;
							temp_str[i] = 0;
							mp3_sample_rate = strtol(temp_str,NULL,0);
							if (mp3_sample_rate < 2000) mp3_sample_rate = 2000;
							else if (mp3_sample_rate > 64000) mp3_sample_rate = 64000;
							uiredraw=1;
							break;
						}
						else if (isdigit(c)) {
							if (i < 5) {
								sco[i] = c | 0x1E00;
								temp_str[i++] = c;
							}
						}
						else if (c == 8) {
							if (i > 0) i--;
							sco[i] = ' ' | 0x1E00;
						}
					}
					vga_msg_box_destroy(&box);
				}
			}
			else if (c == 0x4800) { /* up arrow */
				if (selector > 0) selector--;
				else selector=4;
				uiredraw=1;
			}
			else if (c == 0x4B00) { /* left arrow */
				switch (selector) {
					case 0:	/* sample rate */
						ra = param_preset_rates[0];
						for (cc=0;cc < (sizeof(param_preset_rates)/sizeof(param_preset_rates[0]));cc++) {
							if (param_preset_rates[cc] < mp3_sample_rate)
								ra = param_preset_rates[cc];
						}
						mp3_sample_rate = ra;
						break;
					case 1:	/* stereo/mono */
						mp3_stereo = !mp3_stereo;
						break;
					case 2: /* 8/16-bit */
						mp3_16bit = !mp3_16bit;
						break;
					case 3: /* translatin */
						if (sb_card != NULL) {
						}
						else if (gus_card != NULL) {
							if (gus_channel == 0) gus_channel = gus_active - 1;
							else gus_channel--;
						}
						else if (drv_mode == USE_LPT_DAC) {
							if (lpt_dac_mode == 0) lpt_dac_mode = LPT_DAC_MAX - 1;
							else lpt_dac_mode--;
						}
						break;
					case 4: /* DSP mode */
						if (sb_card != NULL) {
							if (sb_card->dsp_play_method == 0)
								sb_card->dsp_play_method = SNDSB_DSPOUTMETHOD_MAX - 1;
							else
								sb_card->dsp_play_method--;
						}
						else if (gus_card != NULL) {
							if (gus_active > 1) gus_active--;
							if (gus_channel >= gus_active) gus_channel = gus_active - 1;
						}
						break;
				};
				update_cfg();
				uiredraw=1;
			}
			else if (c == 0x4D00) { /* right arrow */
				switch (selector) {
					case 0:	/* sample rate */
						for (cc=0;cc < ((sizeof(param_preset_rates)/sizeof(param_preset_rates[0]))-1);) {
							if (param_preset_rates[cc] > mp3_sample_rate) break;
							cc++;
						}
						mp3_sample_rate = param_preset_rates[cc];
						break;
					case 1:	/* stereo/mono */
						mp3_stereo = !mp3_stereo;
						break;
					case 2: /* 8/16-bit */
						mp3_16bit = !mp3_16bit;
						break;
					case 3: /* translatin */
						if (sb_card != NULL) {
						}
						else if (gus_card != NULL) {
							if (++gus_channel == gus_active) gus_channel = 0;
						}
						else if (drv_mode == USE_LPT_DAC) {
							if (++lpt_dac_mode == LPT_DAC_MAX) lpt_dac_mode = 0;
						}
						break;
					case 4: /* DSP mode */
						if (sb_card != NULL) {
							if (++sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_MAX)
								sb_card->dsp_play_method = 0;
						}
						else if (gus_card != NULL) {
							if (gus_active < 32) gus_active++;
						}
						break;
				};
				update_cfg();
				uiredraw=1;
			}
			else if (c == 0x5000) { /* down arrow */
				if (selector < 4) selector++;
				else selector=0;
				uiredraw=1;
			}
		}

		ui_anim(0);
	}

	if (sb_card != NULL) {
		if (!irq_0_had_warned && sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) {
			/* NOTE TO SELF: It can overwhelm the UI in DOSBox too, but DOSBox seems able to
			   recover if you manage to hit CTRL+F12 to speed up the CPU cycles in the virtual machine.
			   On real hardware, even with the recovery method the machine remains hung :( */
			if (confirm_yes_no_dialog(dos32_irq_0_warning))
				irq_0_had_warned = 1;
			else
				sb_card->dsp_play_method = oldmethod;
		}
	}
	else if (gus_card != NULL) {
		/* NTS: The GF1 chip, on changing the active voice count, seems to treat any new
		 *      channels added on as if they have corrupt state (Ultrasound MAX behavior).
		 *      So we have to clear their state before allowing them to run if we want to
		 *      avoid hangs, random channels buzzing, and IRQ storms. */
		ultrasnd_select_write(gus_card,0x4C,0x03);
		ultrasnd_set_active_voices(gus_card,32);
		ultrasnd_stop_all_voices(gus_card);
		if (gus_card->irq1 != -1) ultrasnd_select_write(gus_card,0x4C,0x07);
	}

	change_param_idx = selector;
	mp3_bytes_per_sample = (mp3_stereo ? 2 : 1) * (mp3_16bit ? 2 : 1);
}

static void play_with_mixer_sb() {
	signed short visrows=25-(4+1);
	signed short visy=4;
	signed char mixer=-1;
	unsigned char bb;
	unsigned char loop=1;
	unsigned char redraw=1;
	unsigned char uiredraw=1;
	signed short offset=0;
	signed short selector=0;
	struct sndsb_mixer_control* ent;
	unsigned char rawmode=0;
	signed short cc,x,y;
	VGA_ALPHA_PTR vga;

	while (loop) {
		if (redraw || uiredraw) {
			_cli();
			if (redraw) {
				for (vga=vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
				ui_anim(1);
			}
			vga_moveto(0,2);
			vga_write_color(0x1F);
			if (rawmode) {
				sprintf(temp_str,"Raw mixer: R=leave raw  x=enter byte value\n");
				vga_write(temp_str);
				vga_write("\n");

				if (selector > 0xFF) selector = 0xFF;
				else if (selector < 0) selector = 0;
				offset = 0;
				for (cc=0;cc < 256;cc++) {
					x = ((cc & 15)*3)+4;
					y = (cc >> 4)+4;
					bb = sndsb_read_mixer(sb_card,(unsigned char)cc);
					vga_moveto(x,y);
					vga_write_color(cc == selector ? 0x70 : 0x1E);
					sprintf(temp_str,"%02X ",bb);
					vga_write(temp_str);

					if ((cc&15) == 0) {
						sprintf(temp_str,"%02x  ",cc&0xF0);
						vga_write_color(0x1F);
						vga_moveto(0,y);
						vga_write(temp_str);
					}
					if (cc <= 15) {
						sprintf(temp_str,"%02x ",cc);
						vga_write_color(0x1F);
						vga_moveto(x,y-1);
						vga_write(temp_str);
					}
				}
			}
			else {
				sprintf(temp_str,"Mixer: %s as %s  M=toggle mixer R=raw\n",sndsb_mixer_chip_str(sb_card->mixer_chip),
					mixer >= 0 ? sndsb_mixer_chip_str(mixer) : "(same)");
				vga_write(temp_str);
				vga_write("\n");

				if (selector >= sb_card->sb_mixer_items)
					selector = sb_card->sb_mixer_items - 1;
				if (offset >= sb_card->sb_mixer_items)
					offset = sb_card->sb_mixer_items - 1;
				if (offset < 0)
					offset = 0;

				for (y=0;y < visrows;y++) {
					if ((y+offset) >= sb_card->sb_mixer_items)
						break;
					if (!sb_card->sb_mixer)
						break;

					ent = sb_card->sb_mixer + offset + y;
					vga_moveto(0,y+visy);
					vga_write_color((y+offset) == selector ? 0x70 : 0x1E);
					if (ent->length == 1)
						x=sprintf(temp_str,"%s     %s",
							sndsb_read_mixer_entry(sb_card,ent) ? "On " : "Off",ent->name);
					else
						x=sprintf(temp_str,"%-3u/%-3u %s",sndsb_read_mixer_entry(sb_card,ent),
							(1 << ent->length) - 1,ent->name);

					while (x < 80) temp_str[x++] = ' '; temp_str[x] = 0;
					vga_write(temp_str);
				}
			}

			vga_write_sync();
			_sti();
			redraw = 0;
			uiredraw = 0;
		}

		if (kbhit()) {
			int c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 'M' || c == 'm') {
				selector = 0;
				offset = 0;
				mixer++;
				if (mixer == 0) mixer++;
				if (mixer == sb_card->mixer_chip) mixer++;
				if (mixer >= SNDSB_MIXER_MAX) mixer = -1;
				sndsb_choose_mixer(sb_card,mixer);
				redraw=1;
			}
			else if (isdigit(c)) {
				int i=0;
				char temp_str[7];
				unsigned int val;
				VGA_ALPHA_PTR sco;
				struct vga_msg_box box;
				vga_msg_box_create(&box,"Custom value",2,0);
				sco = vga_alpha_ram + ((box.y+2) * vga_width) + box.x + 2;
				sco[i] = c | 0x1E00;
				temp_str[i++] = c;
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						c = getch();
						if (c == 0) c = getch() << 8;

						if (c == 27)
							break;
						else if (c == 13) {
							if (i == 0) break;
							temp_str[i] = 0;
							val = (unsigned int)strtol(temp_str,NULL,0);
							val &= (1 << sb_card->sb_mixer[selector].length) - 1;
							sndsb_write_mixer_entry(sb_card,sb_card->sb_mixer+selector,val);
							break;
						}
						else if (isdigit(c)) {
							if (i < 5) {
								sco[i] = c | 0x1E00;
								temp_str[i++] = c;
							}
						}
						else if (c == 8) {
							if (i > 0) i--;
							sco[i] = ' ' | 0x1E00;
						}
					}
				}
				vga_msg_box_destroy(&box);
				uiredraw=1;
			}
			else if (c == 'x') {
				int a,b;

				vga_moveto(0,2);
				vga_write_color(0x1F);
				vga_write("Type hex value:                             \n");
				vga_write_sync();

				a = getch();
				vga_moveto(20,2);
				vga_write_color(0x1E);
				vga_writec((char)a);
				vga_write_sync();

				b = getch();
				vga_moveto(21,2);
				vga_write_color(0x1E);
				vga_writec((char)b);
				vga_write_sync();

				if (isxdigit(a) && isxdigit(b)) {
					unsigned char nb;
					nb = (unsigned char)xdigit2int(a) << 4;
					nb |= (unsigned char)xdigit2int(b);
					sndsb_write_mixer(sb_card,(unsigned char)selector,nb);
				}

				redraw = 1;
			}
			else if (c == 'r' || c == 'R') {
				rawmode = !rawmode;
				selector = 0;
				offset = 0;
				redraw = 1;
			}
			else if (c == 27)
				loop = 0;
			else if (c == 0x4800) { /* up arrow */
				if (rawmode) {
					selector -= 0x10;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector > 0) {
						uiredraw=1;
						selector--;
						if (offset > selector)
							offset = selector;
					}
				}
			}
			else if (c == 0x4B00) { /* left arrow */
				if (rawmode) {
					selector--;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector >= 0 && selector < sb_card->sb_mixer_items &&
						sb_card->sb_mixer != NULL) {
						unsigned char v = sndsb_read_mixer_entry(sb_card,sb_card->sb_mixer+selector);
						if (v > 0) v--;
						sndsb_write_mixer_entry(sb_card,sb_card->sb_mixer+selector,v);
						uiredraw=1;
					}
				}
			}
			else if (c == 0x4D00) { /* right arrow */
				if (rawmode) {
					selector++;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if (selector >= 0 && selector < sb_card->sb_mixer_items &&
						sb_card->sb_mixer != NULL) {
						unsigned char v = sndsb_read_mixer_entry(sb_card,sb_card->sb_mixer+selector);
						if (v < ((1 << sb_card->sb_mixer[selector].length)-1)) v++;
						sndsb_write_mixer_entry(sb_card,sb_card->sb_mixer+selector,v);
						uiredraw=1;
					}
				}
			}
			else if (c == 0x4900) { /* page up */
				if (rawmode) {
				}
				else {
					if (selector > 0) {
						selector -= visrows-1;
						if (selector < 0) selector = 0;
						if (selector < offset) offset = selector;
						uiredraw=1;
					}
				}
			}
			else if (c == 0x5000) { /* down arrow */
				if (rawmode) {
					selector += 0x10;
					selector &= 0xFF;
					uiredraw=1;
				}
				else {
					if ((selector+1) < sb_card->sb_mixer_items) {
						uiredraw=1;
						selector++;
						if (selector >= (offset+visrows)) {
							offset = selector-(visrows-1);
						}
					}
				}
			}
			else if (c == 0x5100) { /* page down */
				if (rawmode) {
				}
				else {
					if ((selector+1) < sb_card->sb_mixer_items) {
						selector += visrows-1;
						if (selector >= sb_card->sb_mixer_items)
							selector = sb_card->sb_mixer_items-1;
						if (selector >= (offset+visrows))
							offset = selector-(visrows-1);
						uiredraw=1;
					}
				}
			}
			else if (c == '<') {
				if (rawmode) {
				}
				else {
					if (selector >= 0 && selector < sb_card->sb_mixer_items &&
						sb_card->sb_mixer != NULL) {
						sndsb_write_mixer_entry(sb_card,sb_card->sb_mixer+selector,0);
						uiredraw=1;
					}
				}
			}
			else if (c == '>') {
				if (rawmode) {
				}
				else {
					if (selector >= 0 && selector < sb_card->sb_mixer_items &&
						sb_card->sb_mixer != NULL) {
						sndsb_write_mixer_entry(sb_card,sb_card->sb_mixer+selector,
							(1 << sb_card->sb_mixer[selector].length) - 1);
						uiredraw=1;
					}
				}
			}
		}

		ui_anim(0);
	}
}

static struct vga_menu_item main_menu_playback_reduced_irq =
	{"xxx",			'i',	0,	0};
static struct vga_menu_item main_menu_playback_goldplay =
	{"xxx",			'g',	0,	0};
static struct vga_menu_item main_menu_playback_goldplay_mode =
	{"xxx",			'm',	0,	0};
static struct vga_menu_item main_menu_playback_timer_clamp =
	{"xxx",			0,	0,	0};
static struct vga_menu_item main_menu_playback_dsp_autoinit_dma =
	{"xxx",			't',	0,	0};
static struct vga_menu_item main_menu_playback_dsp_autoinit_command =
	{"xxx",			'c',	0,	0};

static const struct vga_menu_item* main_menu_playback_sb[] = {
	&main_menu_playback_play,
	&main_menu_playback_stop,
	&menu_separator,
	&main_menu_playback_params,
	&main_menu_playback_reduced_irq,
	&main_menu_playback_goldplay,
	&main_menu_playback_goldplay_mode,
	&main_menu_playback_timer_clamp,
	&main_menu_playback_dsp_autoinit_dma,
	&main_menu_playback_dsp_autoinit_command,
	NULL
};

static const struct vga_menu_item* main_menu_playback_gus[] = {
	&main_menu_playback_play,
	&main_menu_playback_stop,
	&menu_separator,
	&main_menu_playback_params,
	NULL
};

static const struct vga_menu_item* main_menu_playback_other[] = {
	&main_menu_playback_play,
	&main_menu_playback_stop,
	&menu_separator,
	&main_menu_playback_params,
	NULL
};

static const struct vga_menu_item main_menu_device_dsp_reset =
	{"DSP reset",		'r',	0,	0};
static const struct vga_menu_item main_menu_device_mixer_reset =
	{"Mixer reset",		'r',	0,	0};
static const struct vga_menu_item main_menu_device_mixer_controls =
	{"Mixer controls",	'm',	0,	0};
static const struct vga_menu_item main_menu_device_info =
	{"Information",		'i',	0,	0};
static const struct vga_menu_item main_menu_device_choose_sound_card =
	{"Choose sound card",	'c',	0,	0};

static const struct vga_menu_item main_menu_device_gus_reset_tinker =
	{"GUS reset tinker",	'r',	0,	0};

static const struct vga_menu_item* main_menu_device_sb[] = {
	&main_menu_device_dsp_reset,
	&main_menu_device_mixer_reset,
	&main_menu_device_mixer_controls,
	&main_menu_device_info,
	&main_menu_device_choose_sound_card,
	NULL
};

static const struct vga_menu_item* main_menu_device_gus[] = {
	&main_menu_device_info,
	&main_menu_device_choose_sound_card,
	&main_menu_device_gus_reset_tinker,
	NULL
};

const struct vga_menu_item* main_menu_file[] = {
	&main_menu_file_set,
	&main_menu_file_quit,
	NULL
};

const struct vga_menu_item* main_menu_help[] = {
	&main_menu_help_about,
	NULL
};

static const struct vga_menu_bar_item main_menu_bar_sb[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Playback ",		'P',	0x19,	6,	10,	&main_menu_playback_sb}, /* ALT-P */
	{" Device ",		'D',	0x20,	16,	8,	&main_menu_device_sb}, /* ALT-D */
	{" Help ",		'H',	0x23,	24,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static const struct vga_menu_bar_item main_menu_bar_gus[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Playback ",		'P',	0x19,	6,	10,	&main_menu_playback_gus}, /* ALT-P */
	{" Device ",		'D',	0x20,	16,	8,	&main_menu_device_gus}, /* ALT-D */
	{" Help ",		'H',	0x23,	24,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static const struct vga_menu_bar_item main_menu_bar_other[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Playback ",		'P',	0x19,	6,	10,	&main_menu_playback_other}, /* ALT-P */
	{" Help ",		'H',	0x23,	16,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static const struct vga_menu_bar_item main_menu_bar_none[] = {
	/* name                 key     scan    x       w       id */
	{" File ",		'F',	0x21,	0,	6,	&main_menu_file}, /* ALT-F */
	{" Help ",		'H',	0x23,	6,	6,	&main_menu_help}, /* ALT-H */
	{NULL,			0,	0x00,	0,	0,	0}
};

static void my_vga_menu_idle() {
	ui_anim(0);
}

static void update_cfg() {
	unsigned int r;

	mp3_sample_rate_by_timer_ticks = t8254_us2ticks(1000000 / mp3_sample_rate);
	if (mp3_sample_rate_by_timer_ticks == 0) mp3_sample_rate_by_timer_ticks = 1;
	mp3_sample_rate_by_timer = T8254_REF_CLOCK_HZ / mp3_sample_rate_by_timer_ticks;

	if (sb_card != NULL) {
		sb_card->goldplay_mode = goldplay_mode;
		sb_card->dsp_adpcm = 0;
		sb_card->dsp_record = 0;
		r = mp3_sample_rate;
		sb_card->buffer_irq_interval = r;
		if (reduced_irq_interval == 2)
			sb_card->buffer_irq_interval =
				sb_card->buffer_size / mp3_bytes_per_sample;
		else if (reduced_irq_interval == 0)
			sb_card->buffer_irq_interval /= 15;
		else if (reduced_irq_interval == -1)
			sb_card->buffer_irq_interval /= 100;
	}

	if (reduced_irq_interval == 2)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: full length";
	else if (reduced_irq_interval == 1)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: large";
	else if (reduced_irq_interval == 0)
		main_menu_playback_reduced_irq.text =
			"IRQ interval: small";
	else /* -1 */
		main_menu_playback_reduced_irq.text =
			"IRQ interval: tiny";

	if (goldplay_samplerate_choice == GOLDRATE_MATCH)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Match";
	else if (goldplay_samplerate_choice == GOLDRATE_DOUBLE)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Double";
	else if (goldplay_samplerate_choice == GOLDRATE_MAX)
		main_menu_playback_goldplay_mode.text =
			"Goldplay sample rate: Max";
	else
		main_menu_playback_goldplay_mode.text =
			"?";

	if (sb_card != NULL) {
		main_menu_playback_goldplay.text =
			goldplay_mode ? "Goldplay mode: On" : "Goldplay mode: Off";
	}
	else {
		main_menu_playback_goldplay.text =
			"--";
	}

	main_menu_playback_timer_clamp.text =
		sample_rate_timer_clamp ? "Clamp samplerate to timer: On" : "Clamp samplerate to timer: Off";
	main_menu_playback_dsp_autoinit_dma.text =
		sb_card->dsp_autoinit_dma ? "DMA autoinit: On" : "DMA autoinit: Off";
	main_menu_playback_dsp_autoinit_command.text =
		sb_card->dsp_autoinit_command ? "DSP playback: auto-init" : "DSP playback: single-cycle";
}

static void help() {
	printf("test [options]\n");
	printf(" /h /help             This help\n");
	printf(" /nopnp               Don't scan for ISA Plug & Play devices\n");
	printf(" /noprobe             Don't probe ISA I/O ports for non PnP devices\n");
	printf(" /noblaster           Don't use BLASTER environment variable\n");
	printf(" /noultrasnd          Don't use ULTRASND environment variable\n");
	printf(" /16k /8k /4k         Limit DMA buffer to 16k, 8k, or 4k\n");
	printf(" /nosb                Don't use Sound Blaster\n");
	printf(" /nogus               Don't use Gravis Ultrasound\n");
	printf(" /debug               Show debugging information\n");
	printf(" /mp3=<file>          Open with specified MP3 file\n");
	printf(" /play                Automatically start playing WAV file\n");
	printf(" /drv=<N>             Automatically pick output mode (sb,gus,pcspeaker,lptdac)\n");
	printf(" /sc=<N>              Automatically pick Nth sound card (first card=1)\n");
	printf(" /ddac                Force DSP Direct DAC output mode\n");
	printf(" /nochain             Don't chain to previous IRQ (sound blaster IRQ)\n");
	printf(" /noidle              Don't use sndsb library idle function\n");
	printf(" /nodmatc             Don't use Ultrasound DMA TC IRQ\n");
	printf(" /nogusdma            Don't use Ultrasound DMA\n");
	printf(" /nocountdmatc        Don't count Ultrasound DMA TC IRQ\n");
}

static void draw_device_info_gus(struct ultrasnd_ctx *cx,int x,int y,int w,int h) {
	/* clear prior contents */
	{
		VGA_ALPHA_PTR p = vga_alpha_ram + (y * vga_width) + x;
		unsigned int a,b;

		for (b=0;b < h;b++) {
			for (a=0;a < w;a++) {
				*p++ = 0x1E20;
			}
			p += vga_width - w;
		}
	}

	vga_write_color(0x1E);

	vga_moveto(x,y + 0);
	sprintf(temp_str,"BASE:%03Xh  DMA:%d,%d  IRQ:%d,%d  RAM:%dKB  256KB Boundary:%d",
		cx->port,	cx->dma1,	cx->dma2,	cx->irq1,	cx->irq2,
		cx->total_ram / 1024UL,	cx->boundary256k);
	vga_write(temp_str);

	vga_moveto(x,y + 1);
	sprintf(temp_str,"Voices: %u Output: %luHz DMAIRQ:%lu VOICEIRQ:%lu TC:%u",
		cx->active_voices,	cx->output_rate,
		gus_dma_tc,		gus_irq_voice,
		gus_card->dma_tc_irq_happened);
	vga_write(temp_str);
}

static void draw_device_info_sb(struct sndsb_ctx *cx,int x,int y,int w,int h) {
	int row = 2;

	/* clear prior contents */
	{
		VGA_ALPHA_PTR p = vga_alpha_ram + (y * vga_width) + x;
		unsigned int a,b;

		for (b=0;b < h;b++) {
			for (a=0;a < w;a++) {
				*p++ = 0x1E20;
			}
			p += vga_width - w;
		}
	}

	vga_write_color(0x1E);

	vga_moveto(x,y + 0);
	sprintf(temp_str,"BASE:%03Xh  MPU:%03Xh  DMA:%-2d DMA16:%-2d IRQ:%-2d ",
		cx->baseio,	cx->mpuio,	cx->dma8,	cx->dma16,	cx->irq);
	vga_write(temp_str);
	if (cx->dsp_ok) {
		sprintf(temp_str,"DSP: v%u.%u  ",
			cx->dsp_vmaj,	cx->dsp_vmin);
		vga_write(temp_str);
	}
	else {
		vga_write("DSP: No  ");
	}

	if (cx->mixer_ok) {
		sprintf(temp_str,"MIXER: %s",sndsb_mixer_chip_str(cx->mixer_chip));
		vga_write(temp_str);
	}
	else {
		vga_write("MIXER: No");
	}

	vga_moveto(x,y + 1);
	if (cx->dsp_ok) {
		sprintf(temp_str,"DSP String: %s",cx->dsp_copyright);
		vga_write(temp_str);
	}

	if (row < h && (cx->is_gallant_sc6600 || cx->mega_em || cx->sbos)) {
		vga_moveto(x,y + (row++));
		if (cx->is_gallant_sc6600) vga_write("SC-6600 ");
		if (cx->mega_em) vga_write("MEGA-EM ");
		if (cx->sbos) vga_write("SBOS ");
	}
	if (row < h) {
		vga_moveto(x,y + (row++));
		if (cx->pnp_name != NULL) {
			isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
			vga_write("ISA PnP: ");
			vga_write(temp_str);
			vga_write(" ");
			vga_write(cx->pnp_name);
		}
	}
}

static void show_device_info_sb() {
	int c,rows=2,cols=70;
	struct vga_msg_box box;

	if (sb_card->is_gallant_sc6600 || sb_card->mega_em || sb_card->sbos)
		rows++;
	if (sb_card->pnp_id != 0 || sb_card->pnp_name != NULL)
		rows++;

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */
	draw_device_info_sb(sb_card,box.x+2,box.y+1,cols-4,rows);

	while (1) {
		ui_anim(0);
		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27 || c == 13)
				break;
		}
	}

	vga_msg_box_destroy(&box);
}

static void show_device_info_gus() {
	unsigned long p_gus_irq_voice = 0;
	unsigned long p_gus_dma_tc = 0;
	int c,rows=2,cols=70,redraw=1;
	struct vga_msg_box box;

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */

	while (1) {
		ui_anim(0);

		if (redraw) {
			p_gus_dma_tc = gus_dma_tc;
			p_gus_irq_voice = gus_irq_voice;
			draw_device_info_gus(gus_card,box.x+2,box.y+1,cols-4,rows);
			redraw = 0;
		}
		else {
			if (p_gus_dma_tc != gus_dma_tc || p_gus_irq_voice != gus_irq_voice) {
				redraw = 1;
				p_gus_dma_tc = gus_dma_tc;
				p_gus_irq_voice = gus_irq_voice;
			}
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27 || c == 13)
				break;
		}
	}

	vga_msg_box_destroy(&box);
}

static void draw_sound_card_choice_sb(unsigned int x,unsigned int y,unsigned int w,struct sndsb_ctx *cx,int sel) {
	const char *msg = cx->dsp_copyright;

	vga_moveto(x,y);
	if (cx->baseio != 0) {
		vga_write_color(sel ? 0x70 : 0x1F);
		sprintf(temp_str,"%03Xh IRQ%-2d DMA%d DMA%d MPU:%03Xh ",
			cx->baseio,	cx->irq,	cx->dma8,	cx->dma16,	cx->mpuio);
		vga_write(temp_str);
		while (vga_pos_x < (x+w) && *msg != 0) vga_writec(*msg++);
	}
	else {
		vga_write_color(sel ? 0x70 : 0x18);
		vga_write("(none)");
	}
	while (vga_pos_x < (x+w)) vga_writec(' ');
}

static void draw_sound_card_choice_gus(unsigned int x,unsigned int y,unsigned int w,struct ultrasnd_ctx *cx,int sel) {
	vga_moveto(x,y);
	if (cx->port > 0) {
		vga_write_color(sel ? 0x70 : 0x1F);
		sprintf(temp_str,"%03Xh IRQ:%d,%d DMA:%d,%d RAM: %uKB ",
			cx->port,	cx->irq1,	cx->irq2,	cx->dma1,	cx->dma2,	cx->total_ram / 1024UL);
		vga_write(temp_str);
	}
	else {
		vga_write_color(sel ? 0x70 : 0x18);
		vga_write("(none)");
	}
	while (vga_pos_x < (x+w)) vga_writec(' ');
}

static void choose_sound_card_sb() {
	int c,rows=3+1+SNDSB_MAX_CARDS,cols=70,sel=0,i;
	unsigned char wp = mp3_playing;
	struct sndsb_ctx *card;
	struct vga_msg_box box;

	if (sb_card->is_gallant_sc6600 || sb_card->mega_em || sb_card->sbos)
		rows++;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		card = &sndsb_card[i];
		if (card == sb_card) sel = i;
	}

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */
	draw_device_info_sb(sb_card,box.x+2,box.y+1+rows-3,cols,3);
	for (i=0;i < SNDSB_MAX_CARDS;i++)
		draw_sound_card_choice_sb(box.x+2,box.y+1+i,cols,&sndsb_card[i],i == sel);

	card = NULL;
	while (1) {
		ui_anim(0);
		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				card = NULL;
				break;
			}
			else if (c == 13) {
				card = &sndsb_card[sel];
				if (card->baseio != 0) break;
				card = NULL;
			}
			else if (c == 0x4800) {
				draw_sound_card_choice_sb(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],0);
				if (sel == 0) sel = SNDSB_MAX_CARDS - 1;
				else sel--;
				draw_sound_card_choice_sb(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],1);
				draw_device_info_sb(&sndsb_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
			else if (c == 0x5000) {
				draw_sound_card_choice_sb(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],0);
				if (++sel == SNDSB_MAX_CARDS) sel = 0;
				draw_sound_card_choice_sb(box.x+2,box.y+1+sel,cols,&sndsb_card[sel],1);
				draw_device_info_sb(&sndsb_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
		}
	}

	if (card != NULL) {
		stop_play();
		if (sb_card->irq != -1) {
			_dos_setvect(irq2int(sb_card->irq),old_irq);
			if (old_irq_masked) p8259_mask(sb_card->irq);
		}

		sb_card = card;
		sndsb_assign_dma_buffer(sb_card,sb_dma);
		if (sb_card->irq != -1) {
			old_irq_masked = p8259_is_masked(sb_card->irq);
			old_irq = _dos_getvect(irq2int(sb_card->irq));
			_dos_setvect(irq2int(sb_card->irq),sb_irq);
			p8259_unmask(sb_card->irq);
		}

		if (wp) begin_play();
	}

	vga_msg_box_destroy(&box);
}

static void do_gus_reset_tinker() {
	int c,rows=3+2,cols=72;
	unsigned char active_voices = 0;
	unsigned char reset_reg = 0;
	unsigned char fredraw = 1;
	unsigned char redraw = 1;
	struct vga_msg_box box;

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */

	while (1) {
		ui_anim(0);

		if (redraw || fredraw) {
			_cli();

			active_voices = ultrasnd_select_read(gus_card,0x8E);
			/* BUGFIX: DOSBox/DOSBox-X does not emulate reading back the active voices register */
			if (active_voices == 0) active_voices = 0xE0 | (gus_card->active_voices - 1); /* fake it */

			reset_reg = ultrasnd_select_read(gus_card,0x4C);

			_sti();

			if (fredraw) {
				vga_moveto(box.x+2,box.y+1);
				vga_write_color(0x1E);
				vga_write("GUS reset register:");

				// NOTE: If you run an active voice, and then drop the active voice count down below the
				//       active voice (cutting it off), strange and erratic things can happen, including
				//       stuck voices and IRQ storms. On the Ultrasound MAX, the GF1 acts as if voices
				//       beyond the active count have corrupted state. Warn the user by turning the text red.
				vga_moveto(box.x+34,box.y+1);
				vga_write_color(0x1E);
				vga_write("Active voices:");

				vga_moveto(box.x+2,box.y+3);
				vga_write_color(0x1E);
				vga_write("Play with the register by pressing 0-7 on the keyboard to toggle bits.");

				vga_moveto(box.x+2,box.y+4);
				vga_write_color(0x1E);
				vga_write("Up/Down arrow keys to play with Active Voices. May cause audio to stop.");

				vga_moveto(box.x+2,box.y+5);
				vga_write_color(0x1E);
				vga_write("For safety reasons, new active voices are initialized when incremented.");
			}

			sprintf(temp_str,"0x%02x",reset_reg);
			vga_moveto(box.x+2+21,box.y+1);
			vga_write_color(0x1F);
			vga_write(temp_str);

			sprintf(temp_str,"0x%02x",active_voices);
			vga_moveto(box.x+2+48,box.y+1);
			if (((active_voices & 0x1F) + 1) <= gus_channel)
				vga_write_color(0x1C);	// bright red
			else
				vga_write_color(0x1F);
			vga_write(temp_str);

			fredraw = 0;
			redraw = 0;
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				break;
			}
			else if (c == 13) {
			}
			else if (c == 0x4800) { //up
				_cli();
				active_voices--;
				ultrasnd_select_write(gus_card,0x0E,active_voices);
				gus_card->active_voices = (active_voices & 0x1F) + 1;
				redraw = 1;
				_sti();
			}
			else if (c == 0x5000) { //down
				// WARNING: If you increment the active voices register while the GF1 is running (and especially
				//          while voices are active) what can happen is that new voices added by the change
				//          may have corrupted state and can do anything from adding buzzing noises to the output
				//          to causing random IRQ storms. In some cases, it can cause our IRQ handler to get
				//          stuck handling an endless stream of queued IRQ events.
				_cli();
				active_voices++;
				ultrasnd_select_write(gus_card,0x0E,active_voices);
				if ((active_voices&0x1F) != 0) ultrasnd_stop_and_reset_voice(gus_card,active_voices & 0x1F); // to avoid random behavior, lockups, and random noises, initialize the new voice
				gus_card->active_voices = (active_voices & 0x1F) + 1;
				redraw = 1;
				_sti();
			}
			else if (c >= '0' && c <= '7') {
				_cli();
				reset_reg ^= (1 << (c - '0'));
				ultrasnd_select_write(gus_card,0x4C,reset_reg);
				redraw = 1;
				_sti();
			}
		}
	}

	vga_msg_box_destroy(&box);

}

static void choose_sound_card_gus() {
	int c,rows=3+1+MAX_ULTRASND,cols=70,sel=0,i;
	unsigned char wp = mp3_playing;
	struct ultrasnd_ctx *card;
	struct vga_msg_box box;

	for (i=0;i < MAX_ULTRASND;i++) {
		card = &ultrasnd_card[i];
		if (card == gus_card) sel = i;
	}

	vga_msg_box_create(&box,"",rows,cols); /* 3 rows 70 cols */
	draw_device_info_gus(gus_card,box.x+2,box.y+1+rows-3,cols,3);
	for (i=0;i < MAX_ULTRASND;i++)
		draw_sound_card_choice_gus(box.x+2,box.y+1+i,cols,&ultrasnd_card[i],i == sel);

	card = NULL;
	while (1) {
		ui_anim(0);
		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				card = NULL;
				break;
			}
			else if (c == 13) {
				card = &ultrasnd_card[sel];
				if (card->port > 0) break;
				card = NULL;
			}
			else if (c == 0x4800) {
				draw_sound_card_choice_gus(box.x+2,box.y+1+sel,cols,&ultrasnd_card[sel],0);
				if (sel == 0) sel = MAX_ULTRASND - 1;
				else sel--;
				draw_sound_card_choice_gus(box.x+2,box.y+1+sel,cols,&ultrasnd_card[sel],1);
				draw_device_info_gus(&ultrasnd_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
			else if (c == 0x5000) {
				draw_sound_card_choice_gus(box.x+2,box.y+1+sel,cols,&ultrasnd_card[sel],0);
				if (++sel == MAX_ULTRASND) sel = 0;
				draw_sound_card_choice_gus(box.x+2,box.y+1+sel,cols,&ultrasnd_card[sel],1);
				draw_device_info_gus(&ultrasnd_card[sel],box.x+2,box.y+1+rows-3,cols,3);
			}
		}
	}

	if (card != NULL) {
		stop_play();

		gus_card = card;

		if (wp) begin_play();
	}

	vga_msg_box_destroy(&box);
}

static void sndsb_atexit() {
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
}

int main(int argc,char **argv) {
	int no_sb = 0;
	int no_gus = 0;
	int debug = 0;
	int autopick = -1;
	unsigned char sb_irq_pcount = 0;
	int i,loop,redraw,bkgndredraw,cc;
	const struct vga_menu_item *mitem = NULL;
	int disable_ultrasnd_env = 0;
	int disable_blaster_env = 0;
	uint32_t buffer_limit = 0;
	int disable_probe = 0;
	int disable_pnp = 0;
	int force_ddac = 0;
	VGA_ALPHA_PTR vga;
	int autoplay = 0;

	for (i=1;i < argc;) {
		char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"noidle")) {
				dont_sb_idle = 1;
			}
			else if (!strcmp(a,"nogusdma")) {
				dont_use_gus_dma = 1;
			}
			else if (!strcmp(a,"nocountdmatc")) {
				gus_dma_tc_ignore = 1;
			}
			else if (!strcmp(a,"nodmatc")) {
				dont_use_gus_dma_tc = 1;
			}
			else if (!strcmp(a,"nochain")) {
				dont_chain_irq = 1;
			}
			else if (!strcmp(a,"16k")) {
				buffer_limit = 16UL * 1024UL;
			}
			else if (!strcmp(a,"8k")) {
				buffer_limit = 8UL * 1024UL;
			}
			else if (!strcmp(a,"4k")) {
				buffer_limit = 4UL * 1024UL;
			}
			else if (!strcmp(a,"debug")) {
				debug = 1;
			}
			else if (!strcmp(a,"nosb")) {
				no_sb = 1;
			}
			else if (!strcmp(a,"nogus")) {
				no_gus = 1;
			}
			else if (!strcmp(a,"nopnp")) {
				disable_pnp = 1;
			}
			else if (!strncmp(a,"mp3=",4)) {
				a += 4;
				strcpy(mp3_file,a);
			}
			else if (!strcmp(a,"play")) {
				autoplay = 1;
			}
			else if (!strncmp(a,"sc=",3)) {
				a += 3;
				sc_idx = strtol(a,NULL,0);
			}
			else if (!strncmp(a,"drv=",4)) {
				a += 4;
				if (!strcmp(a,"gus"))
					autopick = USE_GUS;
				else if (!strcmp(a,"sb"))
					autopick = USE_SB;
				else if (!strcmp(a,"pcspeaker"))
					autopick = USE_PC_SPEAKER;
				else if (!strcmp(a,"lptdac"))
					autopick = USE_LPT_DAC;
			}
			else if (!strcmp(a,"noprobe")) {
				disable_probe = 1;
			}
			else if (!strcmp(a,"noblaster")) {
				disable_blaster_env = 1;
			}
			else if (!strcmp(a,"noultrasnd")) {
				disable_ultrasnd_env = 1;
			}
			else if (!strcmp(a,"ddac")) {
				force_ddac = 1;
			}
			else if (!strcmp(a,"-nmi")) {
#if TARGET_MSDOS == 32
				sndsb_nmi_32_hook = 0;
#endif
			}
			else if (!strcmp(a,"+nmi")) {
#if TARGET_MSDOS == 32
				sndsb_nmi_32_hook = 1;
#endif
			}
			else if (!strcmp(a,"nomirqp")) {
				sndsb_probe_options.disable_manual_irq_probing = 1;
			}
			else if (!strcmp(a,"noairqp")) {
				sndsb_probe_options.disable_alt_irq_probing = 1;
			}
			else if (!strcmp(a,"nosb16cfg")) {
				sndsb_probe_options.disable_sb16_read_config_byte = 1;
			}
			else if (!strcmp(a,"nodmap")) {
				sndsb_probe_options.disable_manual_dma_probing = 1;
			}
			else if (!strcmp(a,"nohdmap")) {
				sndsb_probe_options.disable_manual_high_dma_probing = 1;
			}
			else if (!strcmp(a,"nowinvxd")) {
				sndsb_probe_options.disable_windows_vxd_checks = 1;
			}
			else {
				help();
				return 1;
			}
		}
	}

	if (autopick >= 0 && autopick != USE_SB)
		no_sb = 1;
	if (autopick >= 0 && autopick != USE_GUS)
		no_gus = 1;

	probe_dos();
	detect_windows();
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	if (!probe_8237()) {
		printf("Cannot init 8237 DMA\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}
	atexit(sndsb_atexit);
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (!init_ultrasnd()) {
		printf("Cannot init library\n");
		return 1;
	}

	if (debug)
		ultrasnd_debug(1);

	if (!disable_pnp) {
		if (find_isa_pnp_bios()) {
			int ret;
			char tmp[192];
			unsigned int j;
			const char *whatis = NULL;
			unsigned char csn,data[192];

			memset(data,0,sizeof(data));
			if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
				struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
				isapnp_probe_next_csn = nfo->total_csn;
				isapnp_read_data = nfo->isa_pnp_port;
			}
			else {
				printf("  ISA PnP BIOS failed to return configuration info\n");
			}

			if (isapnp_read_data != 0) {
				printf("Scanning ISA PnP devices...\n");
				for (csn=1;csn < 255;csn++) {
					isa_pnp_init_key();
					isa_pnp_wake_csn(csn);

					isa_pnp_write_address(0x06); /* CSN */
					if (isa_pnp_read_data() == csn) {
						/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
						/* if we don't, then we only read back the resource data */
						isa_pnp_init_key();
						isa_pnp_wake_csn(csn);

						for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

						if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis) && !no_sb) {
							isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
							if ((ret = sndsb_try_isa_pnp(*((uint32_t*)data),csn)) <= 0)
								printf("ISA PnP: error %d for %s '%s'\n",ret,tmp,whatis);
						}
					}

					/* return back to "wait for key" state */
					isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
				}
			}
		}
	}
	/* Non-plug & play scan */
	if (!no_sb && !disable_blaster_env && sndsb_try_blaster_var() != NULL) {
		printf("Created card ent. for BLASTER variable. IO=%X MPU=%X DMA=%d DMA16=%d IRQ=%d\n",
			sndsb_card_blaster->baseio,
			sndsb_card_blaster->mpuio,
			sndsb_card_blaster->dma8,
			sndsb_card_blaster->dma16,
			sndsb_card_blaster->irq);
		if (!sndsb_init_card(sndsb_card_blaster)) {
			printf("Nope, didn't work\n");
			sndsb_free_card(sndsb_card_blaster);
		}
	}
	if (!no_gus && !disable_ultrasnd_env && ultrasnd_try_ultrasnd_env() != NULL) {
		unsigned char doit = 1;

		printf("ULTRASND environment variable: PORT=0x%03x IRQ=%d,%d DMA=%d,%d\n",
			ultrasnd_env->port,
			ultrasnd_env->irq1,
			ultrasnd_env->irq2,
			ultrasnd_env->dma1,
			ultrasnd_env->dma2);

		/* BUGFIX: Under Windows 3.1, with Creative SB16 drivers resident, and a GUS
		 *         (no drivers installed) that occupies the same IRQ as the Sound Blaster,
		 *         an IRQ conflict arises during testing that prevents this program
		 *         from receiving SB IRQs. So if we're running under Windows, don't
		 *         initialize the Ultrasound if it occupies the same IRQ as any Sound Blaster. */
		if (windows_mode == WINDOWS_ENHANCED) { /* check: does this apply to Windows 95/98/ME too? */
			/* TODO: What happens if you DO have the GUS drivers installed for Windows?
			 *       Do you suppose the GUS is locked out from us, or did Gravis have their
			 *       drivers trap the I/O and virtualize them too? */
			for (i=0;i < SNDSB_MAX_CARDS;i++) {
				if (sndsb_card[i].baseio == 0) continue;
				if (sndsb_card[i].windows_emulation) {
					if ((ultrasnd_env->irq1 >= 0 && sndsb_card[i].irq == ultrasnd_env->irq1) ||
						(ultrasnd_env->irq2 >= 0 && sndsb_card[i].irq == ultrasnd_env->irq2)) {
						fprintf(stderr,"I am running under Windows, the GUS IRQ conflicts with the Sound Blaster IRQ,\n");
						fprintf(stderr,"and a device driver is (likely) virtualizing the Sound Blaster. Not using GUS.\n");
						doit = 0;
					}
				}
			}
		}

		if (!doit || ultrasnd_probe(ultrasnd_env,1)) {
			printf("Never mind, doesn't work\n");
			ultrasnd_free_card(ultrasnd_env);
		}
	}
	if (!disable_probe) {
		if (!no_sb) {
			if (sndsb_try_base(0x220))
				printf("Also found one at 0x220\n");
			if (sndsb_try_base(0x240))
				printf("Also found one at 0x240\n");
		}

		/* NTS: We used to probe for the GUS, but that seems to cause too many
		 *      problems. So instead, we demand the user set the ULTRASND variable. */
	}

	if (autopick < 0) {
		int def_choice = -1;

		printf("Which audio hardware would you like to use?\n");

		if (!no_sb) {
			for (i=0;i < SNDSB_MAX_CARDS;) {
				struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
				if (cx == NULL || cx->baseio == 0) i++;
				break;
			}

			if (i < SNDSB_MAX_CARDS) {
				printf("A. Sound Blaster\n");
				if (def_choice < 0) def_choice = 'A';
			}
		}

		if (!no_gus) {
			for (i=0;i < MAX_ULTRASND;) {
				struct ultrasnd_ctx *u = &ultrasnd_card[i];
				if (ultrasnd_card_taken(u)) break;
				i++;
			}

			if (i < MAX_ULTRASND) {
				printf("B. Gravis Ultrasound\n");
				if (def_choice < 0) def_choice = 'B';
			}
		}

		if (windows_mode == WINDOWS_NONE) {
			printf("C. PC speaker\n");
			printf("D. DAC on LPT1\n");
		}

		i = getch();
		printf("\n");
		if (i == 27) return 0;
		if (i == 13) i = def_choice;
		if (i == 'a' || i == 'A')
			autopick = USE_SB;
		if (i == 'b' || i == 'B')
			autopick = USE_GUS;
		if (i == 'c' || i == 'C')
			autopick = USE_PC_SPEAKER;
		if (i == 'd' || i == 'D')
			autopick = USE_LPT_DAC;
	}

	drv_mode = autopick;
	if (autopick == USE_SB) {
		if (sc_idx < 0) {
			int count=0;
			for (i=0;i < SNDSB_MAX_CARDS;i++) {
				struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
				if (sndsb_card[i].baseio == 0) continue;
				printf("  [%u] base=%X mpu=%X dma=%d dma16=%d irq=%d DSP=%u 1.XXAI=%u\n",
						i+1,cx->baseio,cx->mpuio,cx->dma8,cx->dma16,cx->irq,cx->dsp_ok,cx->dsp_autoinit_dma);
				printf("      MIXER=%u[%s] DSPv=%u.%u SC6600=%u OPL=%X GAME=%X AWE=%X\n",
						cx->mixer_ok,sndsb_mixer_chip_str(cx->mixer_chip),
						(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin,
						cx->is_gallant_sc6600,cx->oplio,cx->gameio,cx->aweio);
				if (cx->pnp_name != NULL) {
					isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
					printf("      ISA PnP[%u]: %s %s\n",cx->pnp_csn,temp_str,cx->pnp_name);
				}
				printf("      '%s'\n",cx->dsp_copyright);
				count++;
			}
			if (count == 0) {
				printf("No cards found.\n");
				return 1;
			}
			printf("-----------\n");
			printf("Select the card you wish to test: "); fflush(stdout);
			i = getch();
			printf("\n");
			if (i == 27) return 0;
			if (i == 13 || i == 10) i = '1';
			sc_idx = i - '0';
		}

		if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
			printf("Sound card index out of range\n");
			return 1;
		}

		sb_card = &sndsb_card[sc_idx-1];
		if (sb_card->baseio == 0) {
			printf("No such card\n");
			return 1;
		}
	}
	else if (autopick == USE_GUS) {
		if (sc_idx < 0) {
			for (i=0;i < MAX_ULTRASND;i++) {
				struct ultrasnd_ctx *u = &ultrasnd_card[i];
				if (ultrasnd_card_taken(u)) {
					printf("[%u] RAM=%dKB PORT=0x%03x IRQ=%d,%d DMA=%d,%d 256KB-boundary=%u voices=%u/%luHz\n",
						i+1,
						(int)(u->total_ram >> 10UL),
						u->port,
						u->irq1,
						u->irq2,
						u->dma1,
						u->dma2,
						u->boundary256k,
						u->active_voices,
						u->output_rate);
				}
			}
			printf("Which card to use? "); fflush(stdout);
			i = -1; scanf("%d",&i); i--;
		}
		else {
			i = sc_idx - 1;
		}
		if (i < 0 || i >= MAX_ULTRASND || !ultrasnd_card_taken(&ultrasnd_card[i])) return 1;
		gus_card = &ultrasnd_card[i];
	}
	else if (drv_mode == USE_PC_SPEAKER) {
		if (windows_mode != WINDOWS_NONE) {
			printf("PC Speaker output is NOT an option under Windows\n");
			return 1;
		}

		mp3_16bit = 0;
		mp3_stereo = 0;
	}
	else if (drv_mode == USE_LPT_DAC) {
		if (windows_mode != WINDOWS_NONE) {
			printf("LPT DAC output is NOT an option under Windows\n");
			return 1;
		}

		mp3_16bit = 0;
		mp3_stereo = 0;
	}
	else {
		printf("Unknown choice\n");
		return 1;
	}

	if (sb_card != NULL) {
		printf("Allocating sound buffer..."); fflush(stdout);
		{
			uint32_t choice = sndsb_recommended_dma_buffer_size(sb_card,buffer_limit);

			do {
				sb_dma = dma_8237_alloc_buffer(choice);
				if (sb_dma == NULL) choice -= 4096UL;
			} while (sb_dma == NULL && choice > 4096UL);

			if (sb_dma == NULL) {
				printf(" failed\n");
				return 0;
			}
		}

		if (sb_card->irq != -1) {
			old_irq_masked = p8259_is_masked(sb_card->irq);
			old_irq = _dos_getvect(irq2int(sb_card->irq));
			_dos_setvect(irq2int(sb_card->irq),sb_irq);
			p8259_unmask(sb_card->irq);
		}

		if (force_ddac) sb_card->dsp_play_method = SNDSB_DSPOUTMETHOD_DIRECT;
		reduced_irq_interval = (sb_card->dsp_play_method == SNDSB_DSPOUTMETHOD_1xx);

		if (!sndsb_assign_dma_buffer(sb_card,sb_dma)) {
			printf("Cannot assign DMA buffer\n");
			return 1;
		}

		/* decide now: 8-bit or 16-bit? */
		mp3_16bit = (sb_card->dsp_play_method >= SNDSB_DSPOUTMETHOD_4xx) &&
			sndsb_dsp_out_method_supported(sb_card,22050,1/*stereo*/,1/*16-bit*/);
		mp3_stereo = (sb_card->dsp_play_method >= SNDSB_DSPOUTMETHOD_200) &&
			sndsb_dsp_out_method_supported(sb_card,11025,1/*stereo*/,0/*16-bit*/);
	}
	else if (gus_card != NULL) {
		mp3_16bit = 1;
		mp3_stereo = 1;

		ultrasnd_select_write(gus_card,0x4C,0x03);
		if (gus_card->irq1 != -1) {
			old_irq_masked = p8259_is_masked(gus_card->irq1);
			old_irq = _dos_getvect(irq2int(gus_card->irq1));
			_dos_setvect(irq2int(gus_card->irq1),gus_irq);
			p8259_unmask(gus_card->irq1);
			ultrasnd_select_write(gus_card,0x4C,0x07);
		}

		if (dont_use_gus_dma)
			gus_card->use_dma = 0;
	}

	i = int10_getmode();
	if (i != 3) int10_setmode(3);

	/* hook IRQ 0 */
	irq_0_adv = 1;
	irq_0_max = 1;
	irq_0_count = 0;
	old_irq_0 = _dos_getvect(irq2int(0));
	_dos_setvect(irq2int(0),irq_0);
	p8259_unmask(0);
	
	vga_write_color(0x07);
	vga_clear();

	loop=1;
	redraw=1;
	bkgndredraw=1;
	if (sb_card != NULL)
		vga_menu_bar.bar = main_menu_bar_sb;
	else if (gus_card != NULL)
		vga_menu_bar.bar = main_menu_bar_gus;
	else if (drv_mode == USE_PC_SPEAKER || drv_mode == USE_LPT_DAC)
		vga_menu_bar.bar = main_menu_bar_other;
	else
		vga_menu_bar.bar = main_menu_bar_none;

	vga_menu_bar.sel = -1;
	vga_menu_bar.row = 3;
	vga_menu_idle = my_vga_menu_idle;
	update_cfg();

	if (mp3_file[0] != 0) open_mp3();
	if (autoplay) begin_play();
	while (loop) {
		if ((mitem = vga_menu_bar_keymon()) != NULL) {
			/* act on it */
			if (mitem == &main_menu_file_quit) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (mitem == &main_menu_file_set) {
				prompt_play_mp3(0);
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (mitem == &main_menu_playback_play) {
				if (!mp3_playing) {
					begin_play();
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_playback_stop) {
				if (mp3_playing) {
					stop_play();
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_device_dsp_reset) {
				if (sb_card != NULL) {
					struct vga_msg_box box;
					vga_msg_box_create(&box,"Resetting DSP...",0,0);
					stop_play();
					sndsb_reset_dsp(sb_card);
					t8254_wait(t8254_us2ticks(1000000));
					vga_msg_box_destroy(&box);
				}
			}
			else if (mitem == &main_menu_device_mixer_reset) {
				if (sb_card != NULL) {
					struct vga_msg_box box;
					vga_msg_box_create(&box,"Resetting mixer...",0,0);
					sndsb_reset_mixer(sb_card);
					t8254_wait(t8254_us2ticks(1000000));
					vga_msg_box_destroy(&box);
				}
			}
			else if (mitem == &main_menu_help_about) {
				struct vga_msg_box box;
				vga_msg_box_create(&box,"SB/GUS/PC Speaker/LPT DAC "
					COMMON_ABOUT_HELP_STR
					,0,0);
				while (1) {
					ui_anim(0);
					if (kbhit()) {
						i = getch();
						if (i == 0) i = getch() << 8;
						if (i == 13 || i == 27) break;
					}
				}
				vga_msg_box_destroy(&box);
			}
			else if (mitem == &main_menu_playback_goldplay) {
				if (sb_card != NULL) {
					unsigned char wp = mp3_playing;
					if (wp) stop_play();
					goldplay_mode = !goldplay_mode;
					if (!irq_0_had_warned && goldplay_mode) {
						/* NOTE TO SELF: It can overwhelm the UI in DOSBox too, but DOSBox seems able to
						   recover if you manage to hit CTRL+F12 to speed up the CPU cycles in the virtual machine.
						   On real hardware, even with the recovery method the machine remains hung :( */
						if (confirm_yes_no_dialog(dos32_irq_0_warning))
							irq_0_had_warned = 1;
						else
							goldplay_mode = 0;
					}
					update_cfg();
					if (wp) begin_play();
				}
			}
			else if (mitem == &main_menu_playback_goldplay_mode) {
				if (sb_card != NULL) {
					unsigned char wp = mp3_playing;
					if (wp) stop_play();
					if (++goldplay_samplerate_choice > GOLDRATE_MAX)
						goldplay_samplerate_choice = GOLDRATE_MATCH;
					update_cfg();
					if (wp) begin_play();
				}
			}
			else if (mitem == &main_menu_playback_dsp_autoinit_dma) {
				unsigned char wp = mp3_playing;
				if (wp) stop_play();
				sb_card->dsp_autoinit_dma = !sb_card->dsp_autoinit_dma;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_dsp_autoinit_command) {
				unsigned char wp = mp3_playing;
				if (wp) stop_play();
				sb_card->dsp_autoinit_command = !sb_card->dsp_autoinit_command;
				update_cfg();
				if (wp) begin_play();
			}
			else if (mitem == &main_menu_playback_timer_clamp) {
				if (sb_card != NULL) {
					unsigned char wp = mp3_playing;
					if (wp) stop_play();
					sample_rate_timer_clamp = !sample_rate_timer_clamp;
					update_cfg();
					if (wp) begin_play();
				}
			}
			else if (mitem == &main_menu_playback_reduced_irq) {
				if (sb_card != NULL) {
					unsigned char wp = mp3_playing;
					if (wp) stop_play();
					if (++reduced_irq_interval >= 3) reduced_irq_interval = -1;
					update_cfg();
					if (wp) begin_play();
				}
			}
			else if (mitem == &main_menu_playback_params) {
				unsigned char wp = mp3_playing;
				if (wp) stop_play();
				change_param_menu();
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (mitem == &main_menu_device_mixer_controls) {
				if (sb_card != NULL) {
					play_with_mixer_sb();
					bkgndredraw = 1;
					redraw = 1;
				}
			}
			else if (mitem == &main_menu_device_info) {
				if (sb_card != NULL) show_device_info_sb();
				else if (gus_card != NULL) show_device_info_gus();
			}
			else if (mitem == &main_menu_device_choose_sound_card) {
				if (sb_card != NULL) choose_sound_card_sb();
				else if (gus_card != NULL) choose_sound_card_gus();
			}
			else if (mitem == &main_menu_device_gus_reset_tinker) {
				if (gus_card != NULL) do_gus_reset_tinker();
			}
		}

		if (sb_irq_count != sb_irq_pcount) {
			sb_irq_pcount = sb_irq_count;
			redraw = 1;
		}

		if (redraw || bkgndredraw) {
			_cli();
			if (!mp3_playing) update_cfg();
			if (bkgndredraw) {
				for (vga=vga_alpha_ram+(80*2),cc=0;cc < (80*23);cc++) *vga++ = 0x1E00 | 177;
				draw_irq_indicator();
				vga_menu_bar_draw();
			}
			ui_anim(bkgndredraw);
			vga_moveto(0,2);
			vga_write_color(0x1F);
			for (vga=vga_alpha_ram+(80*2),cc=0;cc < 80;cc++) *vga++ = 0x1F20;
			vga_write("File: ");
			vga_write(mp3_file);
			vga_write_sync();
			bkgndredraw = 0;
			redraw = 0;
			_sti();
		}

		if (kbhit()) {
			i = getch();
			if (i == 0) i = getch() << 8;

			if (i == 27) {
				if (confirm_quit()) {
					loop = 0;
					break;
				}
			}
			else if (i == ' ') {
				if (mp3_playing) stop_play();
				else begin_play();
			}
			else if (i == 0x4B00) { /* left arrow */
				unsigned char wp = mp3_playing;
				signed long mp3_position;
				if (wp) stop_play();
				mp3_position = lseek(mp3_fd,0,SEEK_CUR) - mp3_smallstep - sizeof(mp3_data);
				if (mp3_position < 0) mp3_position = 0;
				lseek(mp3_fd,mp3_position,SEEK_SET);
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == 0x4D00) { /* right arrow */
				unsigned char wp = mp3_playing;
				signed long mp3_position;
				if (wp) stop_play();
				mp3_position = lseek(mp3_fd,0,SEEK_CUR) + mp3_smallstep - sizeof(mp3_data);
				lseek(mp3_fd,mp3_position,SEEK_SET);
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == 0x4900) { /* page up */
				unsigned char wp = mp3_playing;
				signed long mp3_position;
				if (wp) stop_play();
				mp3_position = lseek(mp3_fd,0,SEEK_CUR) - (8*mp3_smallstep) - sizeof(mp3_data);
				if (mp3_position < 0) mp3_position = 0;
				lseek(mp3_fd,mp3_position,SEEK_SET);
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == 0x5100) { /* page down */
				unsigned char wp = mp3_playing;
				signed long mp3_position;
				if (wp) stop_play();
				mp3_position = lseek(mp3_fd,0,SEEK_CUR) + (8*mp3_smallstep) - sizeof(mp3_data);
				lseek(mp3_fd,mp3_position,SEEK_SET);
				if (wp) begin_play();
				bkgndredraw = 1;
				redraw = 1;
			}
			else if (i == '-' || i == 0x8E00) {
				redraw = 1;
				if (output_amplify > (1 << output_amplify_shr))
					output_amplify--;
			}
			else if (i == '=' || i == 0x9000) {
				redraw = 1;
				if (output_amplify < (128 << output_amplify_shr))
					output_amplify++;
			}
		}

		ui_anim(0);
	}

	_sti();
	vga_write_sync();
	printf("Stopping playback...\n");
	stop_play();
	printf("Closing WAV...\n");
	close_mp3();
	printf("Freeing buffer...\n");

	if (gus_card != NULL)
		gus_card = NULL;

	if (sb_dma != NULL) {
		dma_8237_free_buffer(sb_dma);
		sb_dma = NULL;
	}

	if (sb_card != NULL) {
		printf("Releasing IRQ %d...\n",sb_card->irq);
		if (sb_card->irq != -1)
			_dos_setvect(irq2int(sb_card->irq),old_irq);
	}
	else if (gus_card != NULL) {
		ultrasnd_select_write(gus_card,0x4C,0x03);

		ultrasnd_abort_dma_transfer(gus_card);
		ultrasnd_stop_all_voices(gus_card);
		ultrasnd_stop_timers(gus_card);
		ultrasnd_flush_irq_events(gus_card);

		printf("Releasing IRQ %d...\n",gus_card->irq1);
		if (gus_card->irq1 != -1)
			_dos_setvect(irq2int(gus_card->irq1),old_irq);
	}

	if (output_buffer != NULL) {
		free(output_buffer);
		output_buffer = NULL;
	}

	_dos_setvect(irq2int(0),old_irq_0);
	free_libmad();
	free_faad2();
	return 0;
}

/* ------------------------------------------- MS-DOS version --------------------------------------- */
#endif


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static void stop_child(PROCESS_INFORMATION *p){if(p->hProcess){(void)TerminateProcess(p->hProcess,99U);(void)WaitForSingleObject(p->hProcess,2000U);}}
static int fail(const char *m,PROCESS_INFORMATION *p,HANDLE in,HANDLE out){
 fprintf(stderr,"Windows TUI integration failed: %s (error=%lu)\n",m,(unsigned long)GetLastError());
 stop_child(p);if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);(void)FreeConsole();
 if(p->hThread)CloseHandle(p->hThread);if(p->hProcess)CloseHandle(p->hProcess);return 1;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;char cmd[32768];int n=snprintf(cmd,sizeof(cmd),"\"%s\"",argv[1]);if(n<0||(size_t)n>=sizeof(cmd))return 2;
 STARTUPINFOA si;PROCESS_INFORMATION p;ZeroMemory(&si,sizeof(si));ZeroMemory(&p,sizeof(p));si.cb=sizeof(si);
 if(!CreateProcessA(NULL,cmd,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,NULL,&si,&p)){fprintf(stderr,"CreateProcess failed: %lu\n",(unsigned long)GetLastError());return 1;}
 if(!FreeConsole())return fail("could not detach from runner console",&p,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE);
 bool attached=false;for(unsigned i=0;i<200U;i++){if(AttachConsole(p.dwProcessId)){attached=true;break;}Sleep(25U);}
 if(!attached)return fail("could not attach to child console",&p,INVALID_HANDLE_VALUE,INVALID_HANDLE_VALUE);
 HANDLE in=CreateFileA("CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0U,NULL);
 HANDLE out=CreateFileA("CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0U,NULL);
 if(in==INVALID_HANDLE_VALUE||out==INVALID_HANDLE_VALUE)return fail("could not open console handles",&p,in,out);
 DWORD saved_in=0U,saved_out=0U;if(!GetConsoleMode(in,&saved_in)||!GetConsoleMode(out,&saved_out))return fail("could not read original console modes",&p,in,out);
 bool ready=false;for(unsigned i=0;i<300U;i++){
  if(WaitForSingleObject(p.hProcess,10U)==WAIT_OBJECT_0)break;
  DWORD im=0U,om=0U;CONSOLE_CURSOR_INFO cursor;CONSOLE_SCREEN_BUFFER_INFO info;
  if(!GetConsoleMode(in,&im)||!GetConsoleMode(out,&om)||!GetConsoleCursorInfo(out,&cursor)||!GetConsoleScreenBufferInfo(out,&info))continue;
  DWORD cells=(DWORD)info.dwSize.X*(DWORD)info.dwSize.Y;if(!cells||cells>1000000U)continue;
  wchar_t *buf=(wchar_t*)calloc((size_t)cells+1U,sizeof(wchar_t));if(!buf)return fail("out of memory",&p,in,out);
  COORD origin={0,0};DWORD got=0U;bool read_ok=ReadConsoleOutputCharacterW(out,buf,cells,origin,&got)!=0;bool marker=read_ok&&wcsstr(buf,L"READY")!=NULL;free(buf);
  if((im&(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT))==0U&&(om&ENABLE_VIRTUAL_TERMINAL_PROCESSING)!=0U&&!cursor.bVisible&&marker){ready=true;break;}
 }
 if(!ready)return fail("raw input, VT output, or cursor hiding was not observed",&p,in,out);
 INPUT_RECORD ev;ZeroMemory(&ev,sizeof(ev));ev.EventType=KEY_EVENT;ev.Event.KeyEvent.bKeyDown=TRUE;ev.Event.KeyEvent.wRepeatCount=1U;ev.Event.KeyEvent.wVirtualKeyCode=0x58U;ev.Event.KeyEvent.wVirtualScanCode=0x2dU;ev.Event.KeyEvent.uChar.UnicodeChar=L'x';
 DWORD written=0U;if(!WriteConsoleInputW(in,&ev,1U,&written)||written!=1U)return fail("could not send key event",&p,in,out);
 if(WaitForSingleObject(p.hProcess,5000U)!=WAIT_OBJECT_0)return fail("TUI did not return after key input",&p,in,out);
 DWORD exit_code=1U;if(!GetExitCodeProcess(p.hProcess,&exit_code)||exit_code!=0U)return fail("TUI did not receive the expected key",&p,in,out);
 DWORD restored_in=0U,restored_out=0U;if(!GetConsoleMode(in,&restored_in)||!GetConsoleMode(out,&restored_out))return fail("could not read restored console modes",&p,in,out);
 if(restored_in!=saved_in||restored_out!=saved_out)return fail("console modes were not restored exactly",&p,in,out);
 CONSOLE_CURSOR_INFO cursor;if(!GetConsoleCursorInfo(out,&cursor)||!cursor.bVisible)return fail("cursor was not shown after TUI",&p,in,out);
 CloseHandle(in);CloseHandle(out);(void)FreeConsole();CloseHandle(p.hThread);CloseHandle(p.hProcess);
 puts("PASS: Windows TUI raw input, ANSI output, cursor control, and console restoration.");return 0;
}

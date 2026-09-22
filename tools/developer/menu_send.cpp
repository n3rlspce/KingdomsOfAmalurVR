#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include "../../src/bridge_tracking/developer_commands.hpp"
#include "../../src/bridge_tracking/rig_status.hpp"

static std::string quote(const std::wstring& path){
    std::string out="'";
    for(wchar_t ch:path){
        if(ch<32||ch>126)return {}; // Do not silently mangle a script pathname.
        if(ch=='\\'||ch=='\'')out+='\\';
        out+=static_cast<char>(ch);
    }
    return out+"'";
}
static std::wstring output(HANDLE handle){
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if(!GetConsoleScreenBufferInfo(handle,&info))return {};
    const DWORD count=std::min<DWORD>(DWORD(info.dwSize.X)*DWORD(info.dwSize.Y),262144);
    std::wstring s(count,L' ');DWORD read{};
    // Read most recent rows if the console has a very large history buffer.
    COORD origin{0,static_cast<SHORT>(std::max<int>(0,info.dwCursorPosition.Y-int(count/info.dwSize.X)+1))};
    if(!ReadConsoleOutputCharacterW(handle,s.data(),count,origin,&read))return {};
    s.resize(read);return s;
}

int wmain(int argc,wchar_t** argv){
 if(argc!=3)return 2;
 std::wofstream receipt(argv[2],std::ios::trunc);
 auto finish=[&](const wchar_t* message,int code){receipt<<message<<L'\n';receipt.flush();return code;};
 wchar_t* end{};DWORD pid=wcstoul(argv[1],&end,10);if(!pid||*end)return finish(L"Invalid game process.",2);
 HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
 if(!process)return finish(L"Game process unavailable.",2);
 wchar_t path[32768]{};DWORD length=32768;bool found=QueryFullProcessImageNameW(process,0,path,&length)!=0;
 const wchar_t* slash=wcsrchr(path,L'\\');
 if(!found||!slash||_wcsicmp(slash+1,L"koa.exe")){CloseHandle(process);return finish(L"Target is not Amalur.",2);}
 std::wstring request=std::wstring(path,slash-path+1)+L"mods\\amalur_menu_request.lua";
 FreeConsole();if(!AttachConsole(pid)){CloseHandle(process);return finish(L"Framework console unavailable.",2);}
 SetConsoleCtrlHandler(nullptr,TRUE);
 HANDLE screen=CreateFileW(L"CONOUT$",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
 bool published=false;
 auto cleanup=[&](){if(published)DeleteFileW(request.c_str());if(screen!=INVALID_HANDLE_VALUE)CloseHandle(screen);FreeConsole();CloseHandle(process);};
 if(screen==INVALID_HANDLE_VALUE){cleanup();return finish(L"Framework output unavailable.",2);}
 auto captured=output(screen);std::wstring session;long long stamp{};
 auto at=captured.rfind(L"AMALUR_MENU_TICK|");
 if(at!=std::wstring::npos){at+=17;auto stop=captured.find(L'|',at);if(stop!=std::wstring::npos){session=captured.substr(at,stop-at);stamp=_wtoi64(captured.c_str()+stop+1);}}
 const auto now=static_cast<long long>(std::time(nullptr));
 if(session.empty()||session.size()>100||session.find_first_not_of(L"0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::wstring::npos||stamp>now||now-stamp>3){cleanup();return finish(L"Pause unavailable: the game's UI update callback is not running.",2);}
 auto nonce=std::to_string(GetCurrentProcessId())+"_"+std::to_string(GetTickCount64());
 std::string script="return {session="+quote(session)+",nonce='"+nonce+"',expires="+std::to_string(now+4)+"}";
 auto temporary=request+L".tmp";std::ofstream file(temporary,std::ios::trunc);file<<script;file.close();
 if(!file||!MoveFileExW(temporary.c_str(),request.c_str(),MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temporary.c_str());cleanup();return finish(L"Could not send pause request.",2);}
 published=true;auto marker=L"AMALUR_MENU|"+std::wstring(nonce.begin(),nonce.end())+L"|";auto started=GetTickCount64();
 while(GetTickCount64()-started<5000&&WaitForSingleObject(process,0)==WAIT_TIMEOUT){
  auto text=output(screen);auto pos=text.rfind(marker);
  if(pos!=std::wstring::npos){auto value=text.substr(pos+marker.size());auto stop=value.find(L"|END");if(stop!=std::wstring::npos){value.resize(stop);bool ok=value.rfind(L"OK|",0)==0;auto split=value.find(L'|');auto message=split==std::wstring::npos?value:value.substr(split+1);cleanup();return finish(message.c_str(),ok?0:1);}}
  Sleep(80);
 }
 cleanup();return finish(L"The game's UI did not respond. Pause was not confirmed.",2);
}

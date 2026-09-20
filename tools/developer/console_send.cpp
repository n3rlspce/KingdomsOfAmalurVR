#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include "../../src/tracking/developer_commands.hpp"

static std::string quote(const std::wstring& path){
    std::string out="'";
    for(wchar_t ch:path){
        if(ch<32||ch>126)return {}; // Do not silently mangle a script pathname.
        if(ch=='\\'||ch=='\'')out+='\\';
        out+=static_cast<char>(ch);
    }
    return out+"'";
}
static std::string script(const std::wstring& path,int row,const std::string& nonce){
    auto q=quote(path),command=amalur::developer::command(row);
    if(q.empty()||command.empty())return {};
    const auto source="local ok,r=pcall(function() local f,e=loadfile("+q+"); assert(f,e); f(); return "+command+
        " end); print('AMALUR'..'_DEV_"+nonce+"|'..(ok and 'OK|' or 'ERROR|')..tostring(r))";
    // Framework 1.4 reads stdin with operator>>: whitespace splits commands.
    // Encode the complete Lua chunk as one token, including spaces in paths.
    std::string encoded="assert(loadstring('";
    for(unsigned char ch:source){char escaped[5];sprintf_s(escaped,"\\%03u",unsigned(ch));encoded+=escaped;}
    return encoded+"'))()";
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
    if(argc==2&&std::wstring(argv[1])==L"--emit-test-command"){
        puts(script(L"C:\\test's folder\\amalur_dev.lua",1,"offline").c_str());return 0;
    }
    if(argc==4&&std::wstring(argv[1])==L"--inspect"){
        wchar_t* end{};DWORD pid=wcstoul(argv[2],&end,10);
        if(!pid||*end)return 2;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!process)return 2;
        wchar_t name[32768]{};DWORD length=32768;
        bool named=QueryFullProcessImageNameW(process,0,name,&length)!=0;
        CloseHandle(process);
        const wchar_t* slash=wcsrchr(name,L'\\');
        if(!named||_wcsicmp(slash?slash+1:name,L"koa.exe")!=0)return 2;
        FreeConsole();
        if(!AttachConsole(pid))return 3;
        SetConsoleCtrlHandler(nullptr,TRUE);
        HANDLE screen=CreateFileW(L"CONOUT$",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        auto captured=screen!=INVALID_HANDLE_VALUE?output(screen):std::wstring{};
        if(screen!=INVALID_HANDLE_VALUE)CloseHandle(screen);
        FreeConsole();
        std::wofstream file(argv[3],std::ios::trunc);file<<captured;
        return !captured.empty()&&file.good()?0:4;
    }
    if(argc==2&&std::wstring(argv[1])==L"--selftest"){
        if(!amalur::developer::command(-1).empty()||!amalur::developer::command(amalur::developer::actions).empty())return 1;
        auto s=script(L"C:\\test's folder\\amalur_dev.lua",1,"42");
        if(s.find("AMALUR_DEV_42|")!=std::string::npos)return 2; // echoed source cannot be an ACK
        if(s.find_first_of(" \t\r\n")!=std::string::npos||s.rfind("assert(loadstring('",0)!=0)return 3;
        if(!quote(L"C:\\bad\nname").empty())return 4;
        for(int i=2;i<11;++i)if(amalur::developer::command(i).find("',1)")==std::string::npos)return 5;
        return 0;
    }
    if(argc!=4)return 2;
    std::wofstream receipt(argv[3],std::ios::trunc);
    auto finish=[&](const std::wstring& msg,int code){receipt<<msg<<L'\n';receipt.flush();return code;};
    wchar_t* end{};unsigned long pid=wcstoul(argv[1],&end,10);
    if(!pid||*end)return finish(L"Invalid game process.",2);
    long row=wcstol(argv[2],&end,10);
    if(*end||amalur::developer::command(row).empty())return finish(L"Invalid command.",2);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
    if(!process)return finish(L"Game process unavailable.",2);
    wchar_t name[32768]{};DWORD len=32768;
    bool named=QueryFullProcessImageNameW(process,0,name,&len)!=0;
    const wchar_t* slash=wcsrchr(name,L'\\');
    if(!named||_wcsicmp(slash?slash+1:name,L"koa.exe")!=0){CloseHandle(process);return finish(L"Target is not Amalur.",2);}
    FreeConsole();
    if(!AttachConsole(pid)){CloseHandle(process);return finish(L"Lua framework console unavailable; nothing sent.",2);}
    SetConsoleCtrlHandler(nullptr,TRUE);
    HANDLE input=CreateFileW(L"CONIN$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    HANDLE screen=CreateFileW(L"CONOUT$",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    auto close=[&](){if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);if(screen!=INVALID_HANDLE_VALUE)CloseHandle(screen);FreeConsole();CloseHandle(process);};
    DWORD mode{};
    if(input==INVALID_HANDLE_VALUE||screen==INVALID_HANDLE_VALUE||!GetConsoleMode(input,&mode)||output(screen).empty()){
        close();return finish(L"Console transport unavailable; nothing sent.",2);
    }
    INPUT_RECORD queued[256];DWORD queuedCount{};
    if(!PeekConsoleInputW(input,queued,256,&queuedCount)){
        close();return finish(L"Cannot check console input; nothing sent.",2);
    }
    for(DWORD i=0;i<queuedCount;++i)if(queued[i].EventType==KEY_EVENT&&queued[i].Event.KeyEvent.bKeyDown){
        close();return finish(L"Console has pending keyboard input; nothing sent.",2);
    }
    // Do not append to partially typed text in a line editor.
    CONSOLE_SCREEN_BUFFER_INFO info{};GetConsoleScreenBufferInfo(screen,&info);
    if(info.dwCursorPosition.X!=0){close();return finish(L"Console line is not empty; nothing sent.",2);}
    wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);
    std::wstring path=module;path=path.substr(0,path.find_last_of(L"\\/")+1)+L"amalur_dev.lua";
    const auto nonce=std::to_string(GetCurrentProcessId())+"_"+std::to_string(GetTickCount64());
    auto line=script(path,static_cast<int>(row),nonce);
    if(line.empty()){close();return finish(L"Unsupported script path; nothing sent.",2);}
    line+='\r';std::vector<INPUT_RECORD> events;
    for(char ch:line){INPUT_RECORD e{};e.EventType=KEY_EVENT;e.Event.KeyEvent.bKeyDown=TRUE;e.Event.KeyEvent.wRepeatCount=1;
        e.Event.KeyEvent.wVirtualKeyCode=ch=='\r'?VK_RETURN:0;e.Event.KeyEvent.uChar.UnicodeChar=ch;events.push_back(e);
        e.Event.KeyEvent.bKeyDown=FALSE;events.push_back(e);}
    DWORD written{};
    if(!WriteConsoleInputW(input,events.data(),static_cast<DWORD>(events.size()),&written)||written!=events.size()){
        close();return finish(L"Console write incomplete. Outcome unknown; do not retry.",3);
    }
    std::wstring marker=L"AMALUR_DEV_"+std::wstring(nonce.begin(),nonce.end())+L"|";
    const ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<6000&&WaitForSingleObject(process,0)==WAIT_TIMEOUT){
        auto text=output(screen);auto at=text.find(marker);
        if(at!=std::wstring::npos){
            auto result=text.substr(at+marker.size());
            if(result.rfind(L"OK|",0)==0){close();return finish(row==0?L"Connected. Ready for explicit commands.":L"Command submitted to Lua; verify the result in game.",0);}
            if(result.rfind(L"ERROR|",0)==0){close();return finish(L"Lua rejected command: "+result.substr(6,75),4);}
        }
        Sleep(50);
    }
    close();return finish(L"No acknowledgement. Outcome unknown; do not repeat.",3);
}

#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include "../../src/tracking/developer_commands.hpp"
#include "../../src/tracking/rig_status.hpp"

static std::string quote(const std::wstring& path){
    std::string out="'";
    for(wchar_t ch:path){
        if(ch<32||ch>126)return {}; // Do not silently mangle a script pathname.
        if(ch=='\\'||ch=='\'')out+='\\';
        out+=static_cast<char>(ch);
    }
    return out+"'";
}
static std::string script(int row,const std::string& nonce,const std::wstring& session){
    auto command=amalur::developer::command(row),q=quote(session);
    if(command.empty()||q.empty())return {};
    return "return {nonce='"+nonce+"',session="+q+",connect="+(row==0?"true":"false")+
        ",run=function() return "+command+" end}";
}
static bool gameplay(DWORD pid){
    amalur::RigStatusChannel channel;amalur::RigStatus status;
    return channel.transfer(status,false)&&status.version==1&&status.pid==pid&&
        DWORD(GetTickCount()-status.tick)<500&&status.paused==0&&status.tracked!=0;
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
        puts(script(1,"offline",L"test_session").c_str());return 0;
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
        auto s=script(1,"42",L"test_session");
        if(s.find("run=function() return amalur_dev.wolf() end")==std::string::npos)return 2;
        if(s.find("connect=false")==std::string::npos||s.rfind("return {",0)!=0)return 3;
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
    if(!gameplay(pid)){CloseHandle(process);return finish(L"Close inventory/menus and load gameplay first; nothing sent.",2);}
    const std::wstring request=std::wstring(name,slash-name+1)+L"mods\\amalur_request.lua";
    FreeConsole();
    if(!AttachConsole(pid)){CloseHandle(process);return finish(L"Lua framework console unavailable; nothing sent.",2);}
    SetConsoleCtrlHandler(nullptr,TRUE);
    HANDLE screen=CreateFileW(L"CONOUT$",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    bool published=false;
    auto close=[&](){if(published)DeleteFileW(request.c_str());if(screen!=INVALID_HANDLE_VALUE)CloseHandle(screen);FreeConsole();CloseHandle(process);};
    const auto captured=screen==INVALID_HANDLE_VALUE?std::wstring{}:output(screen);
    if(captured.empty()){
        close();return finish(L"Framework output unavailable; nothing sent.",2);
    }
    std::wstring session;
    if(row!=0){
        const std::wstring tag=L"AMALUR_DEV_SESSION|";
        auto at=captured.rfind(tag);
        if(at!=std::wstring::npos){at+=tag.size();auto endAt=captured.find(L'|',at);if(endAt!=std::wstring::npos)session=captured.substr(at,endAt-at);}
        if(session.empty()||session.size()>64||session.find_first_not_of(L"0123456789_")!=std::wstring::npos){
            close();return finish(L"Connect to the game update dispatcher first; nothing sent.",2);
        }
    }
    const auto nonce=std::to_string(GetCurrentProcessId())+"_"+std::to_string(GetTickCount64());
    auto line=script(static_cast<int>(row),nonce,session);
    if(line.empty()){close();return finish(L"Unsupported script path; nothing sent.",2);}
    const auto temporary=request+L"."+std::wstring(nonce.begin(),nonce.end())+L".tmp";
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){close();return finish(L"Cannot stage request; nothing sent.",2);}
    DWORD written{};
    bool complete=WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
    CloseHandle(file);
    if(!complete||!gameplay(pid)||!MoveFileExW(temporary.c_str(),request.c_str(),MOVEFILE_WRITE_THROUGH)){
        DeleteFileW(temporary.c_str());close();return finish(L"Request busy or gameplay unavailable; nothing sent.",2);
    }
    published=true;
    std::wstring marker=L"AMALUR_DEV_"+std::wstring(nonce.begin(),nonce.end())+L"|";
    const ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<6000&&WaitForSingleObject(process,0)==WAIT_TIMEOUT){
        auto text=output(screen);auto at=text.find(marker);
        if(at!=std::wstring::npos){
            auto result=text.substr(at+marker.size());
            if(result.rfind(L"OK|",0)==0){close();return finish(row==0?L"Connected. Ready for explicit commands.":L"Command submitted to Lua; verify the result in game.",0);}
            if(result.rfind(L"ERROR|",0)==0){auto endAt=result.find(L"|END");close();return finish(L"Lua rejected command: "+result.substr(6,endAt==std::wstring::npos?512:endAt-6),4);}
        }
        Sleep(50);
    }
    close();return finish(L"No acknowledgement. Outcome unknown; do not repeat.",3);
}

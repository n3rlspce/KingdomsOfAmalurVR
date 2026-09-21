#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>

// Separate UI channel: opening pause must not require a living player, an
// unpaused game, or the cheat connection. Never inject Lua through the console.
class MenuRecovery {
    std::wstring base;
    std::string nonce;
    DWORD pid{};
    ULONGLONG deadline{}, nextPoll{};
    void cleanup(){
        std::ifstream request(base+L"request.txt");std::string session,owner;
        std::getline(request,session);std::getline(request,owner);request.close();
        if(owner==nonce)DeleteFileW((base+L"request.txt").c_str());
        nonce.clear();deadline=0;
    }
public:
    std::wstring status;
    bool failed{};
    ~MenuRecovery(){if(deadline)cleanup();}
    void submit(DWORD gamePid){
        if(deadline)return;
        failed=true;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,gamePid);
        if(!process){status=L"No game process available.";return;}
        wchar_t path[32768]{};DWORD length=32768;
        bool found=QueryFullProcessImageNameW(process,0,path,&length)!=0;CloseHandle(process);
        const wchar_t* slash=wcsrchr(path,L'\\');
        if(!found||!slash||_wcsicmp(slash+1,L"koa.exe")){status=L"No Amalur game detected.";return;}
        base=std::wstring(path,slash-path+1)+L"mods\\amalur_menu_";
        std::ifstream ready(base+L"ready.txt");std::string session;long long stamp{};
        std::getline(ready,session);ready>>stamp;
        auto now=static_cast<long long>(std::time(nullptr));
        if(session.empty()||session.size()>100||!ready||stamp>now||now-stamp>3){
            status=L"Menu recovery is not active. Restart the game to load the new UI hook.";return;
        }
        nonce=std::to_string(GetCurrentProcessId())+"_"+std::to_string(GetTickCount64());
        std::wstring temporary=base+L"request.tmp";
        std::ofstream request(temporary,std::ios::trunc);
        request<<session<<'\n'<<nonce<<'\n'<<now+4<<'\n';request.close();
        if(!request||!MoveFileExW(temporary.c_str(),(base+L"request.txt").c_str(),MOVEFILE_REPLACE_EXISTING)){
            DeleteFileW(temporary.c_str());nonce.clear();status=L"Could not send pause request.";return;
        }
        pid=gamePid;deadline=GetTickCount64()+5000;nextPoll=0;failed=false;
        status=L"Opening the game's pause menu...";
    }
    // Returns once per completed request so failure can reopen Settings.
    bool poll(DWORD gamePid){
        if(!deadline)return false;
        auto now=GetTickCount64();if(now<nextPoll)return false;nextPoll=now+100;
        std::ifstream result(base+L"result.txt");std::string id,code,message;
        std::getline(result,id);std::getline(result,code);std::getline(result,message);
        if(gamePid==pid&&id==nonce&&(code=="OK"||code=="ERROR")){
            failed=code!="OK";status=std::wstring(message.begin(),message.end()).substr(0,220);
            cleanup();return true;
        }
        if(gamePid!=pid||now>=deadline){
            failed=true;status=L"The game's UI did not respond. Pause was not confirmed.";
            cleanup();return true;
        }
        return false;
    }
};

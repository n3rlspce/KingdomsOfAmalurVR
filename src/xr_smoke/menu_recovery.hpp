#pragma once
#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>

// Separate UI channel: opening pause must not require a living player, an
// unpaused game, or the cheat connection. Never inject Lua through the console.
class MenuRecovery {
    DWORD configuredPid{};
    std::wstring startupOptions, receipt;
    PROCESS_INFORMATION child{};
public:
    std::wstring status;
    bool automaticContinue=true;
    void configure(DWORD gamePid){
        if(!gamePid||gamePid==configuredPid)return;
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,gamePid);
        if(!process)return;
        wchar_t path[32768]{};DWORD length=32768;
        bool found=QueryFullProcessImageNameW(process,0,path,&length)!=0;CloseHandle(process);
        const wchar_t* slash=wcsrchr(path,L'\\');
        if(!found||!slash||_wcsicmp(slash+1,L"koa.exe"))return;
        startupOptions=std::wstring(path,slash-path+1)+L"mods\\amalur_startup_options.txt";
        std::ifstream file(startupOptions);std::string value;std::getline(file,value);
        automaticContinue=value!="return false";configuredPid=gamePid;
    }
    void toggleAutomaticContinue(){
        if(startupOptions.empty()){status=L"No game detected; startup setting was not changed.";return;}
        auto temporary=startupOptions+L".tmp";std::ofstream file(temporary,std::ios::trunc);
        file<<(automaticContinue?"return false\n":"return true\n");file.close();
        if(!file||!MoveFileExW(temporary.c_str(),startupOptions.c_str(),MOVEFILE_REPLACE_EXISTING)){
            DeleteFileW(temporary.c_str());status=L"Could not save the startup setting.";return;
        }
        automaticContinue=!automaticContinue;
        status=automaticContinue?L"Next launch: continue latest save automatically. Autosave stays OFF.":L"Next launch: stop at the main menu. Autosave stays OFF.";
    }
    bool failed{};
    ~MenuRecovery(){if(child.hThread)CloseHandle(child.hThread);if(child.hProcess)CloseHandle(child.hProcess);}
    void submit(DWORD gamePid){
        if(child.hProcess)return;
        failed=true;
        if(!gamePid){status=L"No game process available.";return;}
        wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
        std::wstring folder=exe;folder=folder.substr(0,folder.find_last_of(L"\\/")+1);
        receipt=folder+L"menu-result-"+std::to_wstring(GetCurrentProcessId())+L".txt";
        DeleteFileW(receipt.c_str());
        auto helper=folder+L"amalur-menu-send.exe";
        auto cmd=L"\""+helper+L"\" "+std::to_wstring(gamePid)+L" \""+receipt+L"\"";
        STARTUPINFOW si{sizeof(si)};
        if(!CreateProcessW(helper.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&si,&child)){
            child={};status=L"Menu helper missing.";return;
        }
        failed=false;status=L"Opening the game's pause menu...";
    }
    bool poll(DWORD gamePid){
        configure(gamePid);
        if(!child.hProcess||WaitForSingleObject(child.hProcess,0)!=WAIT_OBJECT_0)return false;
        DWORD code=1;GetExitCodeProcess(child.hProcess,&code);
        std::wifstream file(receipt);std::wstring result;std::getline(file,result);file.close();
        CloseHandle(child.hThread);CloseHandle(child.hProcess);child={};DeleteFileW(receipt.c_str());
        failed=code!=0||result.empty();status=result.empty()?L"No pause result was received.":result.substr(0,220);
        return true;
    }
};

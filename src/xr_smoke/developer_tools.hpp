#pragma once
#include "../tracking/developer_commands.hpp"
#include <fstream>

// Console transport is a separate process: attaching must not steal the XR
// bridge's console or block its frame loop. No automatic retry of mutations.
class DeveloperTools {
    PROCESS_INFORMATION child{};
    std::wstring receipt;
public:
    std::wstring status=L"Connect to the Lua framework before spawning or granting.";
    bool connected=false;
    DWORD connectedPid{};
    ~DeveloperTools(){if(child.hThread)CloseHandle(child.hThread);if(child.hProcess)CloseHandle(child.hProcess);}
    bool busy()const{return child.hProcess!=nullptr;}
    void submit(int row,DWORD gamePid){
        if(busy())return;
        if(!gamePid){status=L"No loaded game detected.";connected=false;return;}
        if(row!=0&&(!connected||connectedPid!=gamePid)){status=L"Select Connect first.";connected=false;return;}
        if(amalur::developer::command(row).empty())return;
        wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
        std::wstring folder=exe;folder=folder.substr(0,folder.find_last_of(L"\\/")+1);
        receipt=folder+L"developer-result-"+std::to_wstring(GetCurrentProcessId())+L".txt";
        DeleteFileW(receipt.c_str());
        std::wstring helper=folder+L"amalur-dev-send.exe";
        std::wstring cmd=L"\""+helper+L"\" "+std::to_wstring(gamePid)+L" "+std::to_wstring(row)+L" \""+receipt+L"\"";
        STARTUPINFOW si{sizeof(si)};
        if(!CreateProcessW(helper.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&si,&child)){
            child={};status=L"Developer helper missing. Build the developer tools first.";connected=false;return;
        }
        connectedPid=gamePid;
        status=row==0?L"Connecting...":L"Request pending; do not repeat.";
    }
    void poll(DWORD gamePid){
        if(connectedPid&&gamePid!=connectedPid)connected=false;
        if(!busy())return;
        if(WaitForSingleObject(child.hProcess,0)!=WAIT_OBJECT_0)return;
        DWORD code=1;GetExitCodeProcess(child.hProcess,&code);
        std::wifstream file(receipt);std::wstring result;std::getline(file,result);
        CloseHandle(child.hThread);CloseHandle(child.hProcess);child={};
        if(result.empty())status=L"No response. Outcome unknown; do not retry blindly.";
        else status=result.substr(0,100);
        connected=code==0&&gamePid==connectedPid;
        DeleteFileW(receipt.c_str());
    }
};

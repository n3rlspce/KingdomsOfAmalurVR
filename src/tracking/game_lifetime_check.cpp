#include "../xr_smoke/game_lifetime.hpp"
#include <cstdio>
#include <string>
int wmain(int argc,wchar_t**){
    if(argc>1){Sleep(300);return 0;}
    GameLifetime lifetime;
    if(lifetime.exited()||lifetime.attach(0))return 1;
    wchar_t path[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return 2;
    std::wstring command=L"\""+std::wstring(path)+L"\" --child";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
    if(!CreateProcessW(path,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child))return 3;
    const bool attached=lifetime.attach(child.dwProcessId);
    const bool running=!lifetime.exited();
    const bool noReplacement=!lifetime.attach(GetCurrentProcessId());
    const auto finished=WaitForSingleObject(child.hProcess,5000);
    const bool exited=lifetime.exited()&&lifetime.gameExited();
    CloseHandle(child.hThread);CloseHandle(child.hProcess);
    if(!attached||!running||!noReplacement||finished!=WAIT_OBJECT_0||!exited)return 4;
    std::puts("PASS: startup wait, live process, exit detection and retained process identity");
    return 0;
}

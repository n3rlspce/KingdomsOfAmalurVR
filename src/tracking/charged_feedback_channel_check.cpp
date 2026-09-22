#define AMALUR_CHARGED_FEEDBACK_MAPPING L"Local\\AmalurChargedFeedbackOfflineCheckV1"
#define AMALUR_CHARGED_FEEDBACK_MUTEX L"Local\\AmalurChargedFeedbackOfflineCheckMutexV1"
#include "charged_feedback_channel.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* label){if(!ok){printf("FAIL: %s\n",label);std::exit(1);}}
int main(){
    using namespace amalur;
    ChargedFeedbackChannel writer,reader;
    ChargedFeedbackWriter policy;const auto now=GetTickCount64();
    policy.sample(GetCurrentProcessId(),22,3,44,now,true,false);
    auto sent=policy.packet();check(writer.transfer(sent,true),"writer opens isolated mapping");
    ChargedFeedbackPacket received;check(reader.transfer(received,false),"reader opens isolated mapping");
    check(!memcmp(&sent,&received,sizeof(sent))&&validChargedFeedback(received,now),"packet round trip preserves fields and ABI");
    policy.sample(GetCurrentProcessId(),22,3,44,now+1,true,true);sent=policy.packet();
    check(writer.transfer(sent,true)&&reader.transfer(received,false)&&received.readySequence==1,"ready event reaches consumer");
    check(!writer.transfer(received,false)&&!reader.transfer(sent,true),"mapping direction cannot be switched");
    puts("PASS: isolated charged feedback IPC packet, ready event, fixed ABI and direction guards");
}

#include "WandInteraction.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace WandInteraction;State s;Point socket{.2f,1,0},hands[2]={{-.4f,1,0},socket};bool valid[2]={true,true},grip[2]{},trigger[2]{};
 auto tick=[&](float dt=.01f,bool available=true){s.update(dt,socket,hands,valid,grip,trigger,available);};
 tick();assert(s.owner==-1 && distance(s.position,socket)==0);
 socket.x+=.08f;s.retarget(socket);assert(distance(s.position,socket)<.0001f);
 grip[0]=true;tick();assert(s.owner==-1);
 grip[1]=true;tick();assert(s.owner==1);
 trigger[1]=true;tick();assert(s.charging);
 trigger[1]=false;tick();assert(!s.charging&&!s.cancel&&s.owner==1);
 trigger[1]=true;tick();hands[1].y+=.4f;tick();grip[1]=false;tick();assert(s.cancel&&!s.charging&&s.returning&&s.owner==-1);
 socket.x+=.1f;tick(.3f);assert(!s.returning&&distance(s.position,socket)<.0001f);
 hands[0]=socket;grip[0]=false;tick();grip[0]=true;trigger[0]=true;tick();assert(s.owner==0&&!s.charging);
 trigger[0]=false;tick();trigger[0]=true;tick();assert(s.charging);
 tick(.01f,false);assert(s.cancel&&s.owner==-1&&!s.charging);
 tick();assert(s.owner==-1);
 grip[0]=false;tick(.3f);grip[0]=true;trigger[0]=false;tick();assert(s.owner==0);
 trigger[0]=true;tick();valid[0]=false;tick();assert(s.cancel&&s.owner==-1);
 valid[0]=true;grip[0]=true;trigger[0]=false;hands[0]={socket.x-.4f,socket.y,socket.z};tick();hands[0]={socket.x-.19f,socket.y,socket.z};tick();assert(s.owner==0);
 std::puts("PASS: proximity, held-grip entry, both hands, live holster retarget, release-to-cast, drop cancellation, preheld trigger, focus and tracking loss");
}

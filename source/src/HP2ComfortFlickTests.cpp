#include "HP2Comfort.h"
#include "HP2Flick.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
    assert(HP2Comfort::crouchDrop(1.2f,1.2f,40,50)==0); // arbitrary starting height
    assert(HP2Comfort::crouchDrop(1.8f,1.79f,40,50)==0); // standing tracking noise
    assert(HP2Comfort::crouchDrop(1.2f,.9f,40,50)>.19f);
    assert(HP2Comfort::crouchDrop(1.7f,1.66f,40,50)==0); // relaxed standing stays straight
    assert(HP2Comfort::crouchDrop(1.7f,1.60f,40,50)>0); // deliberate crouch still bends
    assert(HP2Comfort::crouchDrop(1.2f,1.4f,40,50)==0);
    HP2Comfort::Height h;assert(h.update(0,.016f,true)==0);
    float step=h.update(20,.016f,true);assert(step>0&&step<20);
    assert(h.update(40,.016f,false)==40); // jumping is never softened
    assert(h.update(400,.016f,true)==400); // travel resets
    h.reset();h.update(100,.016f,true);
    float down=h.update(90,.016f,false,false,true);assert(down>90&&down<100);
    assert(h.update(110,.016f,false,false,false)==110); // upward jump remains immediate
    h.reset();h.update(0,.016f,true);
    for(int i=1;i<=12;++i){float z=h.update(float(i*8),.016f,true);assert(z<i*8&&z>=0);}
    assert(h.update(1000,.016f,true)==1000);
    HP2Flick::Recognizer f;assert(f.update(0,0,.02f,true)==-1);
    assert(f.update(.06f,0,.02f,true)==3);
    assert(f.update(.12f,0,.02f,true)==-1); // one event per flick
    for(int i=0;i<8;++i)assert(f.update(.12f,0,.02f,true)==-1);
    assert(f.update(.06f,0,.02f,true)==-1); // reverse recovery, not left flick
    assert(f.update(0,0,.02f,true)==-1);
    for(int i=0;i<3;++i)assert(f.update(0,0,.02f,true)==-1);
    assert(f.update(0,.06f,.02f,true)==0);
    f.reset();f.update(0,0,.02f,true);assert(f.update(-.06f,0,.02f,true)==2);
    f.reset();f.update(0,0,.02f,true);assert(f.update(0,-.06f,.02f,true)==1);
    f.reset();f.update(0,0,.02f,true);assert(f.update(2,0,.02f,true)==-1);
    f.reset();f.update(0,0,.02f,true);assert(f.update(.06f,.06f,.02f,true)==-1);
    for(float dt:{.01f,.016f,.033f}) {
        HP2Flick::Recognizer rhythm;rhythm.update(0,0,dt,true);
        assert(rhythm.update(.03f,0,dt,true)==-1);
        assert(rhythm.update(.06f,0,dt,true)==3);
        for(int i=0;i<100;++i)assert(rhythm.update(.06f,0,dt,true)==-1); // held endpoint never rearms
        assert(rhythm.update(.03f,0,dt,true)==-1);
        assert(rhythm.update(0,0,dt,true)==-1); // return suppressed
        for(int i=0;i<6;++i)assert(rhythm.update(0,0,dt,true)==-1);
        assert(rhythm.update(-.03f,0,dt,true)==-1);
        assert(rhythm.update(-.06f,0,dt,true)==2); // deliberate opposite beat works
    }
    puts("HP2 comfort/flick tests passed: steps, jumps, travel, cardinal flicks, one event, rearm, discontinuity and diagonal rejection");
}

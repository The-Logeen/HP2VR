#include "HP2GnomeThrow.h"
#include <cassert>
#include <cstdio>
static void settle(HP2GnomeThrow::Gesture& g){for(int i=0;i<12;++i)assert(!g.update(0,0,0,.02f,true));}
int main(){
    HP2GnomeThrow::Gesture g;settle(g);assert(!g.update(.025f,0,0,.02f,true));assert(!g.update(.05f,0,0,.02f,true));assert(g.update(.075f,0,0,.02f,true));assert(!g.update(.1f,0,0,.02f,true));
    g.reset();settle(g);assert(!g.update(0,.08f,0,.02f,true));
    g.reset();settle(g);assert(!g.update(-.08f,0,0,.02f,true));
    g.reset();settle(g);assert(!g.update(0,0,.08f,.02f,true));
    g.reset();settle(g);assert(!g.update(1,0,0,.02f,true));
    g.reset();settle(g);assert(!g.update(.08f,0,0,.02f,false));
    g.reset();assert(!g.update(0,0,0,.02f,true));assert(!g.update(.08f,0,0,.02f,true));
    g.reset();settle(g);for(int i=1;i<=30;++i)assert(!g.update(i*.004f,0,0,.02f,true));
    puts("HP2 gnome gesture passed: deliberate forward throw, one-shot, settle, slow/side/back/up/tracking-loss/jump rejection");
}

#include "HP2ClimbPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
    using namespace HP2ClimbPolicy;
    assert(!recover(0,.3f,.3f));
    assert(!recover(0,1.f,.1f));
    assert(recover(0,1.f,.2f));
    assert(recover(0,3.f,0));
    assert(!recover(.02f,.5f,0));
    assert(recover(.08f,.5f,0));
    assert(top(40,0,20,96,1));
    assert(!top(0,0,0,96,1));
    assert(!top(40,0,20,96,.5f));
    assert(!top(100,0,20,96,1));
    assert(!top(40,0,60,96,1));
    assert(nearAttempt(100,90,20,15));
    assert(!nearAttempt(100,20,20,15));
    assert(!nearAttempt(100,100,100,15));
    std::puts("HP2 climb policy passed: native-first, failed exit, bounded supported top, no downward rescue");
}

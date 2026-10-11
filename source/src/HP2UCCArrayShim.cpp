#include "Engine.h"

// This HP2 SDK snapshot declares these two UE1 container helpers but its
// import library does not export them. The original UCC linked them from the
// contemporary Core static library, which is not present in the SDK archive.
// Keep the implementation private to the research commandlet host.
void FArray::Realloc(INT elementSize) {
    Data = GMalloc->Realloc(Data, ArrayMax * elementSize, TEXT("FArray"));
}

void FArray::Remove(INT index, INT count, INT elementSize) {
    check(index >= 0);
    check(count >= 0);
    check(index + count <= ArrayNum);
    if (count == 0) return;

    appMemmove(
        static_cast<BYTE*>(Data) + index * elementSize,
        static_cast<BYTE*>(Data) + (index + count) * elementSize,
        (ArrayNum - index - count) * elementSize);
    ArrayNum -= count;

    if (3 * ArrayNum < 2 * ArrayMax) {
        ArrayMax = ArrayNum;
        Realloc(elementSize);
    }
}

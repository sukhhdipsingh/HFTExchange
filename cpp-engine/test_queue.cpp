#include "include/LockFreeQueue.h"

int main() {
    hft::LockFreeQueue<int, 4> q;
    q.push(1);
    int x;
    q.pop(x);
    return 0;
}

#include "scaler/io/ymq/timed_queue.h"

using namespace scaler::ymq;

int main()
{
#ifdef __linux__
    TimedQueue tq;

    Timestamp ts;

    tq.push(ts, [] { printf("in timer\n"); });
#endif  // __linux__
}

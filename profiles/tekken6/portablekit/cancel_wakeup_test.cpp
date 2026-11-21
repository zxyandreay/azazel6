#include "cancel_wakeup.hpp"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (false)

int main() {
    using portablekit::Thread;
    using portablekit::ThreadStatus;
    CHECK(azazel6::cancel_wakeup(nullptr) == portablekit::error::kUnknownThid);
    Thread dormant{};
    dormant.status = ThreadStatus::Dormant;
    dormant.wakeup_count = 2;
    CHECK(azazel6::cancel_wakeup(&dormant) == portablekit::error::kDormant);
    CHECK(dormant.wakeup_count == 2);
    Thread dead{};
    dead.status = ThreadStatus::Dead;
    dead.wakeup_count = 1;
    CHECK(azazel6::cancel_wakeup(&dead) == portablekit::error::kUnknownThid);
    CHECK(dead.wakeup_count == 1);
    Thread ready{};
    ready.status = ThreadStatus::Ready;
    ready.wakeup_count = 3;
    CHECK(azazel6::cancel_wakeup(&ready) == 3);
    CHECK(ready.wakeup_count == 0);
    CHECK(azazel6::cancel_wakeup(&ready) == 0);
    Thread waiting{};
    waiting.status = ThreadStatus::Waiting;
    waiting.wakeup_count = 1;
    CHECK(azazel6::cancel_wakeup(&waiting) == 1);
    CHECK(waiting.wakeup_count == 0);
}

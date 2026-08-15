// tests/test_store.cpp — the SessionStore is bounded under adversarial churn.
// TTL alone bounds nothing if a client cycles unique session ids faster than
// they expire; a hard size cap + oldest-eviction keeps memory bounded. Set the
// cap LOW via env before the singleton is first touched.
#include <cstdlib>
#if defined(_WIN32)
#  define SETENV(k,v) _putenv_s(k,v)
#else
#  define SETENV(k,v) setenv(k,v,1)
#endif
#include <waya/surface/session.hpp>
#include <iostream>
#include <string>

using namespace waya::surface::detail;

static int pass = 0, fail = 0;
static void check(bool c, const char* m){ if(c) ++pass; else { ++fail; std::cerr << "FAIL: " << m << "\n"; } }

int main() {
    SETENV("WAYA_SESSION_CAP", "100");
    auto& S = SessionStore::instance();   // reads the cap on first construction

    // Flood 2000 unique ids (a reconnect-id memory-DoS). The store must stay
    // bounded near the cap, not hold all 2000.
    for (int i = 0; i < 2000; ++i) S.save<int>("s" + std::to_string(i), i);
    int alive = 0;
    for (int i = 0; i < 2000; ++i) if (S.take<int>("s" + std::to_string(i)).has_value()) ++alive;
    check(alive <= 120, "store is hard-bounded under a unique-id flood (<= cap + one drop batch)");
    check(alive >= 80,  "store keeps ~cap entries (doesn't over-evict)");

    // A normal save/take round-trips exactly (the cap doesn't corrupt fresh entries).
    S.save<std::string>("live", std::string("hello"));
    auto got = S.take<std::string>("live");
    check(got.has_value() && *got == "hello", "save/take round-trips a retained model");
    check(!S.take<std::string>("live").has_value(), "take removes the entry (one owner)");

    std::cout << "test_store: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}

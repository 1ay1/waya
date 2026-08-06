// tests/fail/typed.cpp — the type-state gate suite: each case MUST NOT compile.
// ctest builds one target per case with -DWAYA_CASE=N and expects failure
// (WILL_FAIL). This is the executable proof that "impossible layout doesn't
// compile" — the signature maya-for-the-web guarantee.
#include <waya/surface/live.hpp>
#include <waya/surface/typed.hpp>

using namespace waya::tui;

waya::surface::NodeRef bad() {
#if   WAYA_CASE == 1
    // gap requires a flex/grid container; Text is inline.
    return Text("hi") | gap(12);
#elif WAYA_CASE == 2
    // justify requires a container; Box is a plain block.
    return Box(Text("hi")) | justify_center;
#elif WAYA_CASE == 3
    // align requires a container; Text is inline.
    return Text("x") | align_center;
#elif WAYA_CASE == 4
    // wrap requires a container; Box is a block.
    return Box() | wrap_on;
#elif WAYA_CASE == 5
    // gap on a Box (block) — not a container.
    return Box(Text("a"), Text("b")) | gap(8);
#else
    return Box();  // case 0 / unknown: MUST compile (sanity — not in the fail range)
#endif
}

int main() { auto n = bad(); return n == nullptr; }

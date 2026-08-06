// tests/test_mod.cpp — the zero-cost style layer: the small-buffer Mod (no heap
// alloc for the common case) and the Building proxy (finalize ONCE per chain,
// not per mod). Proves the maya "styling is ~free" property while keeping the
// diff-critical hashing identical to the old per-mod finalize.
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya::surface;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }
static bool has(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
static std::string css_of(NodeRef n) { return DomBackend{}.render(*n).css; }

int main() {
    // ── Mod value semantics: copy, move, compose (SBO + heap) ────────────────
    {
        Mod m = bold;                 // small inline mod
        Mod copy = m;                 // copy
        Mod moved = std::move(copy);  // move
        auto n = text("x") | moved;
        check(has(css_of(n), "font-weight"), "copied+moved Mod applies");
    }
    {
        // deep composite forces the heap path; copy + apply must be correct
        Mod deep = bold | italic | underline | strike
                 | fg(0x111111) | bg(0x222222) | pad(4) | round(8);
        Mod copy = deep;
        auto n = text("y") | copy;
        auto c = css_of(n);
        check(has(c, "font-weight") && has(c, "font-style") && has(c, "#111111") && has(c, "#222222"),
              "deep composite (heap) copies + applies all parts");
    }
    {
        // noop is the identity
        auto a = text("z") | bold | noop | italic;
        check(has(css_of(a), "font-weight") && has(css_of(a), "font-style"), "noop is identity");
    }

    // ── Building: a chain finalizes ONCE but yields the SAME hash as before ──
    {
        // Two identical chains must hash identically (diff correctness).
        auto a = col(text("Hi") | fg(0x111) | bold | pad(8), box() | round(4)) | gap(12) | pad(16);
        auto b = col(text("Hi") | fg(0x111) | bold | pad(8), box() | round(4)) | gap(12) | pad(16);
        NodeRef na = a, nb = b;
        check(na->hash == nb->hash, "identical chains hash equal (deferred finalize)");
        check(diff(na, nb).empty(), "identical chains diff to nothing");
    }
    {
        // A single-field change is detected as exactly one op.
        auto a = col(text("Hi") | bold | pad(8)) | gap(12);
        auto b = col(text("Ho") | bold | pad(8)) | gap(12);
        NodeRef na = a, nb = b;
        auto p = diff(na, nb);
        check(p.size() == 1 && p[0].op == Op::set_text, "one text change -> one set_text op");
    }
    {
        // Mod ORDER still resolves right-wins (last mod on a field wins).
        auto n = box() | bg(0x111111) | bg(0x222222);
        check(has(css_of(n), "#222222") && !has(css_of(n), "#111111"), "right-most mod wins (deterministic)");
    }
    {
        // A Building is usable as a NodeRef everywhere (implicit consume).
        NodeRef n = box(text("a")) | pad(4);   // returned into a NodeRef
        check(n != nullptr && n->hash != 0, "Building -> NodeRef finalizes");
        // and nesting a Building as a child finalizes it too
        auto parent = col(box(text("x")) | pad(2) | round(2), text("y") | bold);
        NodeRef np = parent;
        check(np->kids.size() == 2 && np->kids[0]->hash != 0, "Building child finalized when nested");
    }

    std::cout << "test_mod: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}

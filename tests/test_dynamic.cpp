/// tests/test_dynamic.cpp — runtime-data combinators (each/when/dyn/raw).

#include <waya/waya.hpp>

#include <iostream>
#include <string>
#include <vector>

using namespace waya::dsl;
using namespace waya::style;
using namespace waya::style::literals;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)
#define CHECK_EQ(a,b) do { auto _a=(a); auto _b=(b); if (_a==_b) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "\n  exp: " << _b \
              << "\n  got: " << _a << "\n"; } } while (0)

static bool has(const std::string& h, std::string_view n) { return h.find(n) != std::string::npos; }
static int count(const std::string& h, std::string_view n) {
    int c = 0; for (std::size_t p = 0; (p = h.find(n, p)) != std::string::npos; ++p) ++c; return c;
}

struct Row { std::string name; int ms; bool down; };

int main() {
    std::vector<Row> rows = {{"api", 12, false}, {"db", 40, true}, {"cache", 3, false}};

    // ── each: a list renders every row, in order ────────────────────────────
    {
        auto ui = tbody_(each(rows, [](const Row& r) {
            return tr_(td_(text(r.name)), td_(text(r.ms, "ms")));
        }));
        auto r = waya::render::render(ui);
        CHECK(has(r.html, "<td>api</td><td>12ms</td>"));
        CHECK(has(r.html, "<td>db</td><td>40ms</td>"));
        CHECK_EQ(count(r.html, "<tr>"), 3);
    }

    // ── each with per-row conditional styling ───────────────────────────────
    {
        auto ui = tbody_(each(rows, [](const Row& r) {
            return tr_(
                td_(text(r.name)),
                when(r.down, td_(text("DOWN")) | fg(0xef4444),
                             td_(text("ok"))   | fg(0x22c55e))
            );
        }));
        auto r = waya::render::render(ui);
        CHECK(has(r.html, "DOWN"));
        CHECK(has(r.html, "ok"));
        CHECK(has(r.css, "color:#ef4444"));
        CHECK(has(r.css, "color:#22c55e"));
    }

    // ── interning across rows: identical row styles share ONE rule ──────────
    {
        std::vector<int> xs = {1,2,3,4,5,6,7,8,9,10};
        auto ui = ul_(each(xs, [](int) {
            return li_(text("item")) | pad(4_px) | bg(0x111111);   // same style each row
        }));
        auto r = waya::render::render(ui);
        CHECK_EQ(count(r.html, "<li"), 10);      // ten items
        CHECK_EQ(count(r.css, ".wa-"), 1);        // ONE interned rule
    }

    // ── when single-branch: false renders nothing ───────────────────────────
    {
        auto a = waya::render::render(div_(when(false, p_(text("x")))));
        auto b = waya::render::render(div_(when(true,  p_(text("x")))));
        CHECK_EQ(a.html, std::string("<div></div>"));
        CHECK_EQ(b.html, std::string("<div><p>x</p></div>"));
    }

    // ── dyn: arbitrary runtime construction, category stated ────────────────
    {
        int n = 3;
        auto ui = div_(dyn<waya::html::Cat::Flow>([n] {
            return n > 0 ? p_(text("positive")) : p_(text("non-positive"));
        }));
        auto r = waya::render::render(ui);
        CHECK(has(r.html, "positive"));
    }

    // ── each_indexed ────────────────────────────────────────────────────────
    {
        std::vector<std::string> names = {"a", "b"};
        auto ui = ol_(each_indexed(names, [](const std::string& s, std::size_t i) {
            return li_(text(std::to_string(i) + ":" + s));
        }));
        auto r = waya::render::render(ui);
        CHECK(has(r.html, "<li>0:a</li>"));
        CHECK(has(r.html, "<li>1:b</li>"));
    }

    // ── raw: trusted HTML passes through unescaped ──────────────────────────
    {
        auto r = waya::render::render(div_(raw("<b>bold</b>")));
        CHECK_EQ(r.html, std::string("<div><b>bold</b></div>"));
    }

    // ── nested each (list of lists) ─────────────────────────────────────────
    {
        std::vector<std::vector<int>> grid = {{1,2},{3}};
        auto ui = div_(each(grid, [](const std::vector<int>& rowv) {
            return ul_(each(rowv, [](int v) { return li_(text(v)); }));
        }));
        auto r = waya::render::render(ui);
        CHECK(has(r.html, "<ul><li>1</li><li>2</li></ul>"));
        CHECK(has(r.html, "<ul><li>3</li></ul>"));
    }

    std::cout << "test_dynamic: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}

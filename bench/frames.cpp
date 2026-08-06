// bench/frames.cpp — measure what waya actually sends on the wire.
//
// The design's central claim: after the first paint, an interaction streams only
// the changed nodes — tens of bytes, not a re-render. This measures that on a
// realistic dashboard: full first paint vs. the delta frame for a single-field
// change, and the per-op diff cost.
//
//   c++ -std=c++2c -O2 -Iinclude bench/frames.cpp -o frames && ./frames

#include <waya/surface/live.hpp>
#include <waya/ui.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace waya::surface;
using namespace waya::ui;

// A realistic dashboard: a header, a stat row, a data table, a chart. `sel` is
// the one bit of state a click changes (a highlighted row) — so a "click" is a
// minimal, representative interaction.
struct Row { int id; std::string name; std::string role; float trend; };

static std::vector<Row> make_rows(int n) {
    std::vector<Row> rs;
    for (int i = 0; i < n; ++i)
        rs.push_back({i, "User " + std::to_string(i), (i % 3 == 0 ? "Admin" : "Member"),
                      (float)((i * 37) % 100)});
    return rs;
}

static NodeRef dashboard(const std::vector<Row>& rows, int sel, int counter) {
    std::vector<NodeRef> cells;
    const char* heads[] = {"ID", "Name", "Role", "Trend"};
    for (auto h : heads) cells.push_back(text(h) | semibold | fg_muted | pad_y(8));
    for (auto& r : rows) {
        bool on = r.id == sel;
        auto tint = on ? bg_primary : noop;
        cells.push_back(text(r.id) | pad_y(8) | tint);
        cells.push_back(text(r.name) | pad_y(8) | tint | tap(r.id));
        cells.push_back(badge(r.role, r.role == "Admin" ? Tone::primary : Tone::neutral));
        cells.push_back(sparkline({r.trend, r.trend * 0.6f, r.trend, r.trend * 1.2f}) | w(80) | h(20));
    }
    auto table = box(); table->kids = std::move(cells); table->style.flow = Flow::grid;
    table->style.extra.emplace_back("grid-template-columns", "repeat(4,minmax(0,auto))");
    finalize(*table);   // recompute the subtree hash after populating kids

    return col(
        row(text("Dashboard") | heading, push(), badge("live", Tone::success),
            text("count: " + std::to_string(counter)) | fg_muted),
        divider(),
        row(card(text("Users") | fg_muted, text((long long)rows.size()) | font(32) | bold),
            card(text("Active") | fg_muted, text(counter) | font(32) | bold)) | gap(16),
        table
    ) | gap(20) | pad(24) | theme(midnight());
}

static void report(const char* label, std::size_t bytes) {
    std::printf("  %-34s %6zu bytes\n", label, bytes);
}

int main() {
    for (int n : {10, 100, 1000}) {
        auto rows = make_rows(n);
        // The live runtime clears the per-render message table before each view()
        // so handler tokens are assigned by registration ORDER and stay stable
        // frame-to-frame. Mirror that here, or every interactive node would look
        // "changed" and inflate the delta.
        detail::begin_msg_capture(); auto a = dashboard(rows, -1, 0);
        detail::begin_msg_capture(); auto b = dashboard(rows, 3, 0);   // one row highlighted (a click)
        detail::begin_msg_capture(); auto c = dashboard(rows, 3, 1);   // a counter tick (one text node)

        std::string full  = encode_full(*a);
        std::string click = encode_delta(diff(a, b));
        std::string tick  = encode_delta(diff(b, c));    // b→c: ONLY the counter changed

        std::printf("\n=== %d-row dashboard ===\n", n);
        report("full first paint", full.size());
        report("delta: highlight a row (click)", click.size());
        report("delta: counter tick (1 text)", tick.size());
        std::printf("  reduction (click vs full):     %.1fx smaller\n",
                    click.empty() ? 0.0 : (double)full.size() / (double)click.size());

        // timing: how long to compute + encode a delta
        using clock = std::chrono::steady_clock;
        const int iters = 2000;
        auto t0 = clock::now();
        std::size_t sink = 0;
        for (int i = 0; i < iters; ++i) sink += encode_delta(diff(b, c)).size();
        auto t1 = clock::now();
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count() / iters;
        std::printf("  diff+encode time:              %.1f us/frame  (%.0f fps headroom)\n",
                    us, 1e6 / us);
        (void)sink;
    }
    std::printf("\n");
    return 0;
}

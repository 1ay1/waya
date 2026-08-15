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
#include <random>
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

    // ── Core allocation model ──────────────────────────────────────
    // The diff is O(1)-skip cheap; the real per-frame cost is BUILDING the tree.
    // A Node is ~720 bytes and view() rebuilds the whole tree each frame. The
    // node pool recycles those blocks (frame N's nodes are freed as frame N+1's
    // are built, so the free-list feeds the next frame). Measured below.
    {
        using clock = std::chrono::steady_clock;
        const int n = 2000;
        auto tree = [&](int sel){
            std::vector<NodeRef> rs;
            for (int i = 0; i < n; ++i)
                rs.push_back(row(text("User " + std::to_string(i)), text(i%3?"Member":"Admin"))
                    | pad(8) | (i==sel? bg(0x1e293b) : Mod{}) | round(6));
            return col_(std::move(rs)) | gap(4);
        };
        NodeRef prev = tree(0);                       // warm the pool
        auto t0 = clock::now();
        for (int k = 0; k < 200; ++k){ NodeRef nx = tree(k%n); volatile auto s = diff(prev,nx).size(); (void)s; prev = std::move(nx); }
        auto t1 = clock::now();
        double us = std::chrono::duration<double,std::micro>(t1-t0).count()/200;
        std::printf("\n=== core: %d-row frame cycle (build+diff+recycle) ===\n", n);
        std::printf("  build+diff+drop:               %.0f us/frame  (%.0f fps)\n", us, 1e6/us);
        std::printf("  node pool: %zu blocks recycled, %zu peak (0 malloc for node storage steady-state)\n",
                    detail::node_pool().free_list.size(), detail::node_pool().high_water);
    }

    // ── Stylesheet interning is O(1) ─────────────────────────────────
    // Identical styles collapse to one CSS class. The dedup is a hash lookup, so
    // rendering a page with N DISTINCT styles is O(N), not O(N²). Scaling check:
    {
        using clock = std::chrono::steady_clock;
        auto distinct = [](int n){
            std::vector<NodeRef> rs;
            for (int i = 0; i < n; ++i) rs.push_back(text("r"+std::to_string(i)) | pad((float)(i%400+1)) | fg((std::uint32_t)(i*7)));
            return col_(std::move(rs));
        };
        std::printf("\n=== core: stylesheet interning (distinct styles) ===\n");
        double prev_us = 0; int prev_n = 0;
        for (int n : {500, 5000}){
            auto t = distinct(n);
            auto t0 = clock::now();
            for (int k=0;k<20;k++){ DomBackend b; volatile auto sz=b.render(*t).css.size(); (void)sz; }
            auto t1 = clock::now();
            double us = std::chrono::duration<double,std::micro>(t1-t0).count()/20;
            std::printf("  %d distinct styles: %.0f us\n", n, us);
            if (prev_us > 0)
                std::printf("  scaling %dx rows -> %.1fx time (O(n): linear, was O(n\xc2\xb2): ~%dx)\n",
                            n/prev_n, us/prev_us, (n/prev_n)*(n/prev_n));
            prev_us = us; prev_n = n;
        }
    }

    // ── Keyed reorder is O(n log n) + minimal ops ───────────────────
    // A keyed list that reorders (drag-sort, live rank shuffle) is reconciled
    // via a longest-increasing-subsequence: the maximal already-ordered set
    // never moves, so moving K rows emits ~K move ops — not the O(n) cascade a
    // naive left-to-right reconcile produces. Verify BOTH the op count and the
    // time for a realistic few-row reorder of a big list.
    {
        using clock = std::chrono::steady_clock;
        auto keyed = [](const std::vector<int>& order){
            std::vector<NodeRef> rs; rs.reserve(order.size());
            for (int id : order) rs.push_back(text("Row "+std::to_string(id)) | key("k"+std::to_string(id)));
            return col_(std::move(rs));
        };
        std::printf("\n=== core: keyed reorder (move K rows of a 2000-row list) ===\n");
        const int n = 2000;
        std::vector<int> base(n); for (int i=0;i<n;i++) base[i]=i;
        auto na = keyed(base);
        std::mt19937 g(7);
        for (int kmoves : {1, 5, 20}){
            std::vector<int> v = base;
            for (int j=0;j<kmoves;j++){
                int from=g()%v.size(); int val=v[from]; v.erase(v.begin()+from);
                int to=g()%(v.size()+1); v.insert(v.begin()+to,val);
            }
            auto nb = keyed(v);
            std::size_t ops=0; auto t0=clock::now();
            for (int r=0;r<100;r++) ops = diff(na, nb).size();
            auto t1=clock::now();
            double us = std::chrono::duration<double,std::micro>(t1-t0).count()/100;
            std::printf("  move %2d rows: %6.0f us, %zu ops  (LIS: ~K ops, not the O(n) cascade)\n",
                        kmoves, us, ops);
        }
    }
    std::printf("\n");
    return 0;
}

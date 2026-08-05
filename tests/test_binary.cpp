/// tests/test_binary.cpp — the binary frame protocol (the framework's private
/// "ANSI"): varint-packed {css, ops}, dramatically smaller than JSON, same one
/// frame shape (a full paint is a `paint` op).

#include <waya/surface/node.hpp>
#include <waya/surface/diff.hpp>
#include <waya/surface/wire.hpp>
#include <waya/surface/binary.hpp>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

using namespace waya::surface;

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do{ if(c) ++g_pass; else { ++g_fail; \
  std::cerr<<"FAIL "<<__FILE__<<':'<<__LINE__<<"  "#c"\n"; } }while(0)

// A reference varint reader mirroring the JS client — so we test the exact
// bytes the browser will parse.
struct Reader {
    const unsigned char* b; std::size_t p = 0;
    std::uint64_t vi(){ std::uint64_t x=0; int s=0; unsigned char c;
        do { c=b[p++]; x |= (std::uint64_t)(c&0x7F)<<s; s+=7; } while(c&0x80); return x; }
    std::string str(){ std::uint64_t n=vi(); std::string r((const char*)b+p, n); p+=n; return r; }
};

struct Frame { std::string css; std::vector<std::tuple<int,std::string,std::string>> ops; };

static Frame decode(const std::string& buf) {
    Reader r{(const unsigned char*)buf.data()};
    Frame f; f.css = r.str();
    std::uint64_t nop = r.vi();
    for (std::uint64_t i=0;i<nop;++i) {
        int k = r.b[r.p++];
        std::uint64_t d = r.vi(); std::string path;
        for (std::uint64_t j=0;j<d;++j){ if(j) path+='.'; path += std::to_string(r.vi()); }
        std::string payload = (k==5) ? "" : r.str();
        f.ops.emplace_back(k, path, payload);
    }
    return f;
}

static NodeRef view(int n){
    return col(
        text("Count") | fg(0x3b82f6) | font(28) | bold,
        text(n) | fg(0xe2e8f0)
    ) | gap(16) | pad(24);
}

int main() {
    // ── a delta decodes to the right op ─────────────────────────────────────
    {
        auto bin = encode_delta(diff(view(42), view(43)));
        auto f = decode(bin);
        CHECK(f.ops.size() == 1);
        CHECK(std::get<0>(f.ops[0]) == (int)Op::set_text);
        CHECK(std::get<1>(f.ops[0]) == "1");     // path (second child)
        CHECK(std::get<2>(f.ops[0]) == "43");
    }

    // ── a full paint is a single `paint` op (7) carrying the root html ──────
    {
        auto bin = encode_full(*view(0));
        auto f = decode(bin);
        CHECK(f.ops.size() == 1);
        CHECK(std::get<0>(f.ops[0]) == 7);        // paint
        CHECK(std::get<1>(f.ops[0]) == "");       // root path
        CHECK(std::get<2>(f.ops[0]).find("<div") != std::string::npos);
        CHECK(!f.css.empty());                    // the stylesheet
    }

    // ── deep path packs as index sequence and round-trips ───────────────────
    {
        auto a = box(box(box(text("x"))));
        auto b = box(box(box(text("y"))));
        auto bin = encode_delta(diff(a, b));
        auto f = decode(bin);
        CHECK(f.ops.size() == 1);
        CHECK(std::get<1>(f.ops[0]) == "0.0.0");
        CHECK(std::get<2>(f.ops[0]) == "y");
    }

    // ── binary is MUCH smaller than JSON for the same delta ─────────────────
    {
        auto patch = diff(view(1), view(2));
        auto bin = encode_delta(patch);
        auto json = delta_frame(patch);
        std::cout << "  counter delta: binary=" << bin.size()
                  << "B  json=" << json.size() << "B\n";
        CHECK(bin.size() < json.size());
        CHECK(bin.size() <= 10);                   // ~7 bytes
    }

    std::cout << "test_binary: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}

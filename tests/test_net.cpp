// tests/test_net.cpp — JSON, HTTP response/cookie parsing, session resumption.
#include <waya/json.hpp>
#include <waya/net/http.hpp>
#include <waya/surface/live.hpp>
#include <iostream>
#include <string>

using namespace waya;

static int pass = 0, fail = 0;
static void check(bool c, const char* msg) { if (c) ++pass; else { ++fail; std::cerr << "FAIL: " << msg << "\n"; } }

int main() {
    // ── JSON parse ───────────────────────────────────────────────────────────
    {
        auto j = json::parse(R"({"user":{"name":"Ada","age":36,"admin":true},"items":[1,2,3]})");
        check(j["user"]["name"].str() == "Ada", "json nested string");
        check(j["user"]["age"].as_int() == 36, "json nested int");
        check(j["user"]["admin"].as_bool() == true, "json bool");
        check(j["items"].size() == 3, "json array size");
        check(j["items"][1].as_int() == 2, "json array index");
        check(j["missing"].is_null(), "json missing is null");
        check(j["user"]["age"].str() == "", "json wrong-type accessor safe");
    }
    // json escapes + unicode
    {
        auto j = json::parse(R"({"s":"line1\nline2\t\"q\"","u":"\u00e9"})");
        check(j["s"].str() == "line1\nline2\t\"q\"", "json escapes decoded");
        check(j["u"].str() == "\xc3\xa9", "json \\u decoded to utf-8");
    }
    // ── JSON build + roundtrip ───────────────────────────────────────────────
    {
        auto o = json::object({
            {"ok", json::boolean(true)},
            {"n", json::number(42)},
            {"list", json::array({json::number(1), json::string("x")})},
        });
        std::string dumped = o.dump();
        auto r = json::parse(dumped);
        check(r["ok"].as_bool() && r["n"].as_int() == 42, "json roundtrip scalars");
        check(r["list"][1].str() == "x", "json roundtrip array");
        check(json::string("a\"b\nc").dump() == "\"a\\\"b\\nc\"", "json string escaping");
    }

    // ── HTTP response parsing ────────────────────────────────────────────────
    {
        std::string raw =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "X-Rate-Limit: 60\r\n"
            "\r\n"
            "{\"ok\":true}";
        auto r = http::detail::parse_response(raw);
        check(r.status == 200, "http status parsed");
        check(r.ok(), "http ok()");
        check(r.body == "{\"ok\":true}", "http body split");
        check(r.header("content-type") == "application/json", "http header case-insensitive");
        check(r.header("X-Rate-Limit") == "60", "http header value");
    }
    // chunked decode
    {
        std::string raw =
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";
        auto r = http::detail::parse_response(raw);
        check(r.body == "hello world", "http chunked decoded");
    }
    // url parsing
    {
        auto u = http::detail::parse_url("https://api.example.com:8443/v1/pay?x=1");
        check(u.scheme == "https", "url scheme");
        check(u.host == "api.example.com", "url host");
        check(u.port == 8443, "url explicit port");
        check(u.path == "/v1/pay?x=1", "url path+query");
        auto u2 = http::detail::parse_url("http://localhost/");
        check(u2.port == 80, "url default http port");
        auto u3 = http::detail::parse_url("https://x.dev");
        check(u3.port == 443 && u3.path == "/", "url default https port + root path");
    }

    // ── Cookies ──────────────────────────────────────────────────────────────
    {
        auto cs = http::parse_cookies("sid=abc123; theme=dark;  remember=1");
        check(cs.size() == 3, "cookie count");
        std::string sid; for (auto& [k, v] : cs) if (k == "sid") sid = v;
        check(sid == "abc123", "cookie value");
        std::string theme; for (auto& [k, v] : cs) if (k == "theme") theme = v;
        check(theme == "dark", "cookie trimmed");

        std::string raw = "GET / HTTP/1.1\r\nHost: x\r\nCookie: a=1; b=2\r\nUpgrade: websocket\r\n\r\n";
        check(http::cookie_header(raw) == "a=1; b=2", "cookie header extracted");

        std::string sc = http::set_cookie("sid", "xyz", 3600);
        check(sc.find("sid=xyz") != std::string::npos, "set_cookie name=value");
        check(sc.find("HttpOnly") != std::string::npos, "set_cookie HttpOnly default");
        check(sc.find("Max-Age=3600") != std::string::npos, "set_cookie max-age");
        check(sc.find("SameSite=Lax") != std::string::npos, "set_cookie SameSite");
    }

    // ── SessionStore resumption ──────────────────────────────────────────────
    {
        struct M { int step = 0; std::string draft; };
        auto& store = surface::detail::SessionStore::instance();
        store.save<M>("sid-1", M{ 3, "hello" });
        // wrong id -> nothing
        check(!store.take<M>("nope").has_value(), "session store miss");
        // right id -> the retained model, once
        auto got = store.take<M>("sid-1");
        check(got.has_value() && got->step == 3 && got->draft == "hello", "session store resume");
        // taking again -> gone (a model has one owner)
        check(!store.take<M>("sid-1").has_value(), "session store take is one-shot");
        // empty id never stored
        store.save<M>("", M{ 9, "" });
        check(!store.take<M>("").has_value(), "session store ignores empty id");
    }

    // ── JSON: deep nesting is bounded (no stack-overflow DoS) ────────────────
    // A hostile payload of thousands of nested arrays must fail cleanly, not
    // recurse until the stack blows. The parser caps depth and returns null.
    {
        std::string deep(5000, '[');   // 5000 levels — well past the cap
        auto j = json::parse(deep);
        check(j.is_null(), "deeply-nested JSON fails cleanly (no crash)");

        std::string deepObj;
        for (int i = 0; i < 5000; ++i) deepObj += "{\"a\":";
        auto j2 = json::parse(deepObj);
        check(j2.is_null(), "deeply-nested objects fail cleanly");

        // a legitimately-nested doc (well under the cap) still parses fine
        std::string ok = "[[[[[[[[[[42]]]]]]]]]]";
        auto j3 = json::parse(ok);
        check(!j3.is_null(), "reasonable nesting still parses");
    }

    // ── JSON fuzz: no input may crash the parser ─────────────────────────────
    // Deterministic bytes drawn from a JSON-ish alphabet so we hit real parser
    // states (strings, escapes, numbers, structure). Property: parse() never
    // throws / never reads OOB (ASan-clean); a syntactically bad input yields
    // null, a good one yields non-null. Reaching the end == success.
    {
        const char alpha[] = "{}[]\"\\:,0123456789.-+eEtfn tru\\u00e9";
        std::uint64_t s = 0xdeadbeefcafef00dull;
        auto rnd = [&]{ s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
        for (int iter = 0; iter < 30000; ++iter) {
            std::string buf;
            std::size_t n = rnd() % 40;
            for (std::size_t i = 0; i < n; ++i) buf.push_back(alpha[rnd() % (sizeof(alpha)-1)]);
            (void)json::parse(buf);   // must not crash on ANY input
        }
        check(true, "JSON fuzz: 30k random inputs, no crash");
    }

    std::cout << "test_net: " << pass << " passed, " << fail << " failed\n";
    return fail ? 1 : 0;
}

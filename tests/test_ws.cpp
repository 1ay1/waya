/// tests/test_ws.cpp — WebSocket handshake + frame codec (RFC 6455).

#include <waya/net/ws.hpp>

#include <iostream>
#include <string>

using namespace waya::ws;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) ++g_pass; else { ++g_fail; \
    std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << "  " #cond "\n"; } } while (0)

int main() {
    // ── RFC 6455 §1.3 reference handshake ───────────────────────────────────
    CHECK(accept_key("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    // ── try_handshake parses an upgrade request → 101 response ──────────────
    {
        std::string req =
            "GET / HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n";
        auto resp = try_handshake(req);
        CHECK(resp.has_value());
        CHECK(resp->find("101 Switching Protocols") != std::string::npos);
        CHECK(resp->find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos);
    }

    // ── a plain HTTP request is NOT a handshake ─────────────────────────────
    CHECK(!try_handshake("GET / HTTP/1.1\r\nHost: x\r\n\r\n").has_value());

    // ── text frame encoding: FIN + opcode 1, unmasked, short length ─────────
    {
        auto f = encode_text("hi");
        CHECK((unsigned char)f[0] == 0x81);   // FIN + text
        CHECK((unsigned char)f[1] == 2);       // length, no mask bit (server)
        CHECK(f.substr(2) == "hi");
    }
    // ── 16-bit length path ──────────────────────────────────────────────────
    {
        std::string big(300, 'x');
        auto f = encode_text(big);
        CHECK((unsigned char)f[1] == 126);     // extended length marker
        CHECK(f.size() == 4 + 300);
    }

    // ── decode a masked client frame (roundtrip) ────────────────────────────
    {
        // Build a masked "hello" frame the way a browser would.
        std::string payload = "hello";
        unsigned char mask[4] = {0x37, 0xfa, 0x21, 0x3d};
        std::string frame;
        frame.push_back((char)0x81);
        frame.push_back((char)(0x80 | payload.size()));
        for (int i = 0; i < 4; ++i) frame.push_back((char)mask[i]);
        for (std::size_t i = 0; i < payload.size(); ++i)
            frame.push_back((char)(payload[i] ^ mask[i & 3]));

        std::size_t used = 0;
        auto fr = decode(frame, used);
        CHECK(fr.ok);
        CHECK(fr.opcode == 0x1);
        CHECK(fr.payload == "hello");
        CHECK(used == frame.size());
    }

    // ── decode reports incomplete on a partial frame ────────────────────────
    {
        std::size_t used = 0;
        auto fr = decode(std::string{(char)0x81}, used);   // only 1 byte
        CHECK(!fr.ok);
    }

    // ── close / pong / ping encoders ────────────────────────────────────
    {
        auto c = encode_close();
        CHECK((unsigned char)c[0] == 0x88);
        auto p = encode_pong("x");
        CHECK((unsigned char)p[0] == 0x8A);
        // PING (0x89) — the keepalive the server sends on idle so proxies/tunnels
        // don't drop the socket.
        auto pi = encode_ping();
        CHECK((unsigned char)pi[0] == 0x89);
        CHECK((unsigned char)pi[1] == 0x00);        // empty payload
        auto pi2 = encode_ping("hb");
        CHECK((unsigned char)pi2[0] == 0x89 && (unsigned char)pi2[1] == 0x02);
        CHECK(pi2.substr(2) == "hb");
    }

    // ── adversarial: an oversized 64-bit length is rejected pre-allocation ───
    // A hostile client can declare a near-UINT64_MAX payload. decode() must
    // reject it (opcode -2) BEFORE resize(), or it's an OOM DoS.
    {
        std::string f;
        f.push_back((char)0x81);            // FIN + text
        f.push_back((char)0xFF);            // masked + 127 (64-bit length)
        for (int i = 0; i < 8; ++i) f.push_back((char)0xFF);   // len = 0xFFFF...
        f.append("mask");
        std::size_t used = 0;
        auto fr = decode(f, used);
        CHECK(!fr.ok);
        CHECK(fr.opcode == -2);             // distinct protocol-error signal
        CHECK(used == 0);
    }
    // just over the cap is rejected; just under is accepted structurally
    {
        auto declare = [](std::uint64_t len){
            std::string f; f.push_back((char)0x81); f.push_back((char)0xFF);
            for (int i = 7; i >= 0; --i) f.push_back((char)((len >> (i*8)) & 0xFF));
            f.append("mask");
            std::size_t used = 0; return decode(f, used);
        };
        CHECK(declare(kMaxFrame + 1).opcode == -2);         // over: rejected
        CHECK(declare(kMaxFrame).opcode != -2);             // at cap: not a proto error
    }

    // ── adversarial: truncated extended-length headers never read OOB ────────
    {
        std::size_t used = 0;
        // 126 marker but < 4 bytes → incomplete, not a crash
        CHECK(!decode(std::string{(char)0x81, (char)126, (char)0x01}, used).ok);
        // 127 marker but < 10 bytes → incomplete
        CHECK(!decode(std::string{(char)0x81, (char)127, 0,0,0,0}, used).ok);
        // masked bit set but mask bytes missing → incomplete
        CHECK(!decode(std::string{(char)0x81, (char)0x82, (char)0x01}, used).ok);
        // RFC 6455: an UNMASKED client frame is a protocol violation. A complete
        // unmasked text frame ("hi", len 2, no mask bit) must be rejected with a
        // fatal opcode (-2), not decoded.
        {
            std::string unmasked{(char)0x81, (char)0x02, 'h', 'i'};   // FIN+text, len 2, unmasked
            auto fr = decode(unmasked, used);
            CHECK(!fr.ok && fr.opcode == -2);
        }
    }

    // ── fuzz: no byte sequence up to 24 bytes may crash decode() ─────────────
    // Deterministic LCG-driven bytes; the property is "decode never throws,
    // never reads OOB (ASan-clean), and only ever succeeds on a consistent
    // frame". Runs thousands of shapes in a blink.
    {
        std::uint64_t s = 0x9e3779b97f4a7c15ull;
        auto rnd = [&]{ s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
        int ok_frames = 0;
        for (int iter = 0; iter < 20000; ++iter) {
            std::string buf;
            std::size_t n = rnd() % 24;
            for (std::size_t i = 0; i < n; ++i) buf.push_back((char)(rnd() & 0xFF));
            std::size_t used = 0;
            auto fr = decode(buf, used);           // must not crash / OOB
            if (fr.ok) { ok_frames++; CHECK(used <= buf.size()); }
        }
        CHECK(ok_frames >= 0);                     // reached here == no crash
    }

    std::cout << "test_ws: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}

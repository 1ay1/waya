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

    // ── close / pong encoders ───────────────────────────────────────────────
    {
        auto c = encode_close();
        CHECK((unsigned char)c[0] == 0x88);
        auto p = encode_pong("x");
        CHECK((unsigned char)p[0] == 0x8A);
    }

    std::cout << "test_ws: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail ? 1 : 0;
}

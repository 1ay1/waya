#pragma once
/// \file ws.hpp
/// A minimal RFC 6455 WebSocket — handshake + frame codec — so the live runtime
/// can PUSH patches over a persistent connection instead of one request per
/// event. Dependency-free (POSIX sockets); the crypto is a tiny inline SHA-1 +
/// base64 for the handshake accept key.
///
/// Scope: text frames, server→client and client→server, close, ping/pong. Enough
/// for the live runtime. Not a general WS library.

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

// This file is a PURE codec (handshake + frame encode/decode) and makes no
// syscalls, so it needs no platform sockets headers — it compiles unchanged on
// POSIX and Windows. The actual socket I/O lives in the runtime (live.hpp).

namespace waya::ws {

// ── SHA-1 (for the Sec-WebSocket-Accept key) ────────────────────────────────
namespace detail {
struct Sha1 {
    std::uint32_t h[5] = {0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    static std::uint32_t rol(std::uint32_t v, int b){ return (v<<b)|(v>>(32-b)); }
    void block(const unsigned char* p){
        std::uint32_t w[80];
        for(int i=0;i<16;i++) w[i]=(p[i*4]<<24)|(p[i*4+1]<<16)|(p[i*4+2]<<8)|p[i*4+3];
        for(int i=16;i<80;i++) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        std::uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(int i=0;i<80;i++){
            std::uint32_t f,k;
            if(i<20){f=(b&c)|((~b)&d);k=0x5A827999;}
            else if(i<40){f=b^c^d;k=0x6ED9EBA1;}
            else if(i<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else {f=b^c^d;k=0xCA62C1D6;}
            std::uint32_t t=rol(a,5)+f+e+k+w[i];
            e=d;d=c;c=rol(b,30);b=a;a=t;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    std::string digest(std::string_view msg){
        std::string m(msg);
        std::uint64_t bits = m.size()*8;
        m.push_back((char)0x80);
        while(m.size()%64!=56) m.push_back(0);
        for(int i=7;i>=0;i--) m.push_back((char)((bits>>(i*8))&0xFF));
        for(std::size_t i=0;i<m.size();i+=64) block((const unsigned char*)m.data()+i);
        std::string out(20,0);
        for(int i=0;i<5;i++){ out[i*4]=(h[i]>>24)&0xFF;out[i*4+1]=(h[i]>>16)&0xFF;
            out[i*4+2]=(h[i]>>8)&0xFF;out[i*4+3]=h[i]&0xFF; }
        return out;
    }
};

std::string base64(std::string_view in);
} // namespace detail

/// Compute the Sec-WebSocket-Accept value from the client key.
std::string accept_key(std::string_view client_key);

/// Extract the Sec-WebSocket-Key header value from a raw HTTP request, if this
/// is a WebSocket upgrade. Returns the handshake RESPONSE to send, or nullopt.
std::optional<std::string> try_handshake(std::string_view req);

// ── Frame codec ──────────────────────────────────────────────────

/// Encode a text frame (server→client: no mask). FIN + opcode 0x1.
std::string encode_text(std::string_view payload);

/// Encode a BINARY frame (FIN + opcode 0x2) — for the packed frame protocol.
std::string encode_binary(std::string_view payload);

/// Encode a close frame.
std::string encode_close();

/// A PONG frame (opcode 0xA) answering a client ping.
std::string encode_pong(std::string_view payload);

/// A PING frame (opcode 0x9). The server sends these on an idle connection so
/// proxies/tunnels/load-balancers (nginx, Cloudflare, ngrok, ALB — all with
/// ~60s idle timeouts) don't tear the socket down; a compliant client answers
/// with a pong automatically. Payload is kept tiny (≤125 bytes, no ext length).
std::string encode_ping(std::string_view payload = "");

/// One decoded incoming frame.
struct Frame { int opcode = -1; std::string payload; bool ok = false; };

/// The largest client frame payload we will decode. A frame declaring more than
/// this is rejected outright (fr.ok stays false, and the caller drops the
/// connection) BEFORE any allocation — so a hostile 64-bit length can't drive an
/// OOM. 16 MiB is far above any legitimate live-runtime message.
inline constexpr std::uint64_t kMaxFrame = 16u * 1024u * 1024u;

/// Decode a single client→server frame from a buffer (client frames are masked).
/// Returns {ok=false} if incomplete OR malformed/oversized. `consumed` gets the
/// bytes used. On an oversized/overflowing length it sets fr.opcode = -2 as a
/// distinct "protocol error, close the connection" signal.
Frame decode(std::string_view buf, std::size_t& consumed);

} // namespace waya::ws

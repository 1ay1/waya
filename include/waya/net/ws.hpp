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

inline std::string base64(std::string_view in){
    static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; int val=0,bits=-6;
    for(unsigned char c:in){ val=(val<<8)+c; bits+=8;
        while(bits>=0){ o.push_back(T[(val>>bits)&0x3F]); bits-=6; } }
    if(bits>-6) o.push_back(T[((val<<8)>>(bits+8))&0x3F]);
    while(o.size()%4) o.push_back('=');
    return o;
}
} // namespace detail

/// Compute the Sec-WebSocket-Accept value from the client key.
inline std::string accept_key(std::string_view client_key){
    static constexpr std::string_view GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    detail::Sha1 s;
    return detail::base64(s.digest(std::string(client_key)+std::string(GUID)));
}

/// Extract the Sec-WebSocket-Key header value from a raw HTTP request, if this
/// is a WebSocket upgrade. Returns the handshake RESPONSE to send, or nullopt.
inline std::optional<std::string> try_handshake(std::string_view req){
    auto has = [&](std::string_view h){ return req.find(h) != std::string_view::npos; };
    if(!has("Upgrade: websocket") && !has("Upgrade: WebSocket")) return std::nullopt;
    auto kpos = req.find("Sec-WebSocket-Key:");
    if(kpos == std::string_view::npos) return std::nullopt;
    kpos += 18;
    while(kpos<req.size() && (req[kpos]==' '||req[kpos]=='\t')) ++kpos;
    auto end = req.find("\r\n", kpos);
    std::string_view key = req.substr(kpos, end-kpos);
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + accept_key(key) + "\r\n\r\n";
}

// ── Frame codec ─────────────────────────────────────────────────────────────

/// Encode a text frame (server→client: no mask). FIN + opcode 0x1.
inline std::string encode_text(std::string_view payload){
    std::string f; f.push_back((char)0x81);
    std::size_t n = payload.size();
    if(n<126){ f.push_back((char)n); }
    else if(n<65536){ f.push_back((char)126); f.push_back((char)((n>>8)&0xFF)); f.push_back((char)(n&0xFF)); }
    else { f.push_back((char)127); for(int i=7;i>=0;--i) f.push_back((char)((n>>(i*8))&0xFF)); }
    f.append(payload);
    return f;
}

/// Encode a BINARY frame (FIN + opcode 0x2) — for the packed frame protocol.
inline std::string encode_binary(std::string_view payload){
    std::string f; f.push_back((char)0x82);
    std::size_t n = payload.size();
    if(n<126){ f.push_back((char)n); }
    else if(n<65536){ f.push_back((char)126); f.push_back((char)((n>>8)&0xFF)); f.push_back((char)(n&0xFF)); }
    else { f.push_back((char)127); for(int i=7;i>=0;--i) f.push_back((char)((n>>(i*8))&0xFF)); }
    f.append(payload);
    return f;
}

/// Encode a close frame.
inline std::string encode_close(){ return std::string{(char)0x88,(char)0x00}; }
inline std::string encode_pong(std::string_view payload){
    std::string f; f.push_back((char)0x8A); f.push_back((char)payload.size());
    f.append(payload); return f;
}

/// One decoded incoming frame.
struct Frame { int opcode = -1; std::string payload; bool ok = false; };

/// Decode a single client→server frame from a buffer (client frames are masked).
/// Returns {ok=false} if incomplete. `consumed` gets the bytes used.
inline Frame decode(std::string_view buf, std::size_t& consumed){
    Frame fr; consumed = 0;
    if(buf.size() < 2) return fr;
    int opcode = buf[0] & 0x0F;
    bool masked = buf[1] & 0x80;
    std::uint64_t len = buf[1] & 0x7F;
    std::size_t pos = 2;
    if(len == 126){ if(buf.size()<4) return fr; len=((unsigned char)buf[2]<<8)|(unsigned char)buf[3]; pos=4; }
    else if(len == 127){ if(buf.size()<10) return fr; len=0; for(int i=0;i<8;i++) len=(len<<8)|(unsigned char)buf[2+i]; pos=10; }
    unsigned char mask[4]={0,0,0,0};
    if(masked){ if(buf.size()<pos+4) return fr; for(int i=0;i<4;i++) mask[i]=buf[pos+i]; pos+=4; }
    if(buf.size() < pos+len) return fr;
    std::string payload; payload.resize(len);
    for(std::uint64_t i=0;i<len;i++) payload[i]= masked ? (buf[pos+i]^mask[i&3]) : buf[pos+i];
    fr.opcode = opcode; fr.payload = std::move(payload); fr.ok = true;
    consumed = pos + len;
    return fr;
}

} // namespace waya::ws

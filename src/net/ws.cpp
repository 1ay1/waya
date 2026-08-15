/// \file src/net/ws.cpp
/// Out-of-line definitions for the RFC 6455 WebSocket codec declared in
/// waya/net/ws.hpp. Compiled ONCE into waya_runtime instead of being re-emitted
/// by every translation unit that includes the header.

#include "waya/net/ws.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace waya::ws {

namespace detail {

std::string base64(std::string_view in){
    static const char* T="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; int val=0,bits=-6;
    for(unsigned char c:in){ val=(val<<8)+c; bits+=8;
        while(bits>=0){ o.push_back(T[(val>>bits)&0x3F]); bits-=6; } }
    if(bits>-6) o.push_back(T[((val<<8)>>(bits+8))&0x3F]);
    while(o.size()%4) o.push_back('=');
    return o;
}

} // namespace detail

std::string accept_key(std::string_view client_key){
    static constexpr std::string_view GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    detail::Sha1 s;
    return detail::base64(s.digest(std::string(client_key)+std::string(GUID)));
}

std::optional<std::string> try_handshake(std::string_view req){
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

std::string encode_text(std::string_view payload){
    std::string f; f.reserve(payload.size() + 10);   // header (≤10B) + payload, no realloc
    f.push_back((char)0x81);
    std::size_t n = payload.size();
    if(n<126){ f.push_back((char)n); }
    else if(n<65536){ f.push_back((char)126); f.push_back((char)((n>>8)&0xFF)); f.push_back((char)(n&0xFF)); }
    else { f.push_back((char)127); for(int i=7;i>=0;--i) f.push_back((char)((n>>(i*8))&0xFF)); }
    f.append(payload);
    return f;
}

std::string encode_binary(std::string_view payload){
    std::string f; f.reserve(payload.size() + 10);   // header (≤10B) + payload, no realloc
    f.push_back((char)0x82);
    std::size_t n = payload.size();
    if(n<126){ f.push_back((char)n); }
    else if(n<65536){ f.push_back((char)126); f.push_back((char)((n>>8)&0xFF)); f.push_back((char)(n&0xFF)); }
    else { f.push_back((char)127); for(int i=7;i>=0;--i) f.push_back((char)((n>>(i*8))&0xFF)); }
    f.append(payload);
    return f;
}

std::string encode_close(){ return std::string{(char)0x88,(char)0x00}; }

std::string encode_pong(std::string_view payload){
    std::string f; f.push_back((char)0x8A); f.push_back((char)payload.size());
    f.append(payload); return f;
}

std::string encode_ping(std::string_view payload){
    std::string f; f.push_back((char)0x89); f.push_back((char)(payload.size() & 0x7F));
    f.append(payload); return f;
}

Frame decode(std::string_view buf, std::size_t& consumed){
    Frame fr; consumed = 0;
    if(buf.size() < 2) return fr;
    int opcode = buf[0] & 0x0F;
    bool masked = buf[1] & 0x80;
    std::uint64_t len = buf[1] & 0x7F;
    std::size_t pos = 2;
    if(len == 126){ if(buf.size()<4) return fr; len=((unsigned char)buf[2]<<8)|(unsigned char)buf[3]; pos=4; }
    else if(len == 127){ if(buf.size()<10) return fr; len=0; for(int i=0;i<8;i++) len=(len<<8)|(unsigned char)buf[2+i]; pos=10; }
    // Reject an oversized frame BEFORE allocating. This also makes the
    // `buf.size() < pos+len` check below immune to uint64 overflow: len is now
    // bounded well under SIZE_MAX, so pos+len cannot wrap.
    if(len > kMaxFrame){ fr.opcode = -2; return fr; }
    // RFC 6455 §5.1: every client→server frame MUST be masked. An unmasked frame
    // is a protocol violation (a broken proxy or a hand-rolled/hostile client);
    // reject it rather than reading an unmasked body. -2 = fatal (close).
    if(!masked){ fr.opcode = -2; return fr; }
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

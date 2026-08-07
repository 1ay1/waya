/// matrix/store.cpp — the reducer + seed. The tick drives three things:
///   1. rain columns fall (and respawn at the top when they run off)
///   2. the terminal types the next scripted line, char by char
///   3. the breach meter climbs; alert escalates as it fills
#include "store.hpp"

#include <array>
#include <random>

namespace mtx {

namespace {
constexpr int kCols = 48;      // rain columns
constexpr int kRows = 30;      // rain rows (cells tall)

std::mt19937& rng() { static std::mt19937 r{0xC0FFEE}; return r; }

// the fake-hack script the terminal types out, line by line
const std::array<const char*, 14> kScript = {{
    "> initiating uplink to mainframe...",
    "> bypassing ICE firewall [layer 1/3] ........ OK",
    "> bypassing ICE firewall [layer 2/3] ........ OK",
    "> bypassing ICE firewall [layer 3/3] ........ OK",
    "> injecting polymorphic payload 0xDEADBEEF",
    "> spoofing MAC 00:1B:44:11:3A:B7",
    "> routing through 7 proxy nodes ............. DONE",
    "> decrypting AES-256 keystore (brute) ....... 41%",
    "> ACCESS GRANTED :: root@mainframe",
    "> exfiltrating /var/secrets/*.db",
    "> wiping audit logs ......................... DONE",
    "> planting persistence rootkit .............. DONE",
    "> [!] intrusion detected \u2014 escalating evasion",
    "> the matrix has you. follow the white rabbit.",
}};
} // namespace

Model Model::boot() {
    Model m;
    m.cols.resize(kCols);
    std::uniform_real_distribution<float> spd(0.5f, 1.6f);
    std::uniform_int_distribution<int>    len(14, 30);   // long streams
    // Scatter heads ACROSS the whole screen (0..kRows) so the very first paint
    // is already a full rain field — not an empty screen that fills in slowly.
    std::uniform_real_distribution<float> y0(0.f, (float)kRows);
    for (int i = 0; i < kCols; ++i) {
        m.cols[i].head  = y0(rng());
        m.cols[i].speed = spd(rng());
        m.cols[i].len   = len(rng());
        m.cols[i].seed  = rng()();
    }
    m.log.clear();
    m.typing.clear();
    m.type_pos = 0;
    m.script_line = 0;
    m.breach = 0;
    m.nodes_pwned = 0;
    m.alert = 0;
    m.running = true;
    return m;
}

std::pair<Model, Cmd<Msg>> update(Model m, Msg msg) {
    return std::visit([&](auto&& e) -> std::pair<Model, Cmd<Msg>> {
        using T = std::decay_t<decltype(e)>;
        if constexpr (std::is_same_v<T, Tick>) {
            m.frame++;
            // 1) rain
            std::uniform_real_distribution<float> spd(0.5f, 1.6f);
            std::uniform_int_distribution<int>    len(14, 30);
            for (auto& c : m.cols) {
                c.head += c.speed;
                if (c.head - c.len > kRows) {       // fell off the bottom -> respawn
                    c.head  = 0;
                    c.speed = spd(rng());
                    c.len   = len(rng());
                    c.seed  = rng()();
                }
            }
            // 2) terminal typing (2 chars/tick for a brisk feel)
            if (m.script_line < (int)kScript.size()) {
                const std::string line = kScript[m.script_line];
                m.type_pos = std::min(line.size(), m.type_pos + 2);
                m.typing = line.substr(0, m.type_pos);
                if (m.type_pos >= line.size()) {
                    m.log.push_back(m.typing);
                    if ((int)m.log.size() > 9) m.log.erase(m.log.begin());
                    m.typing.clear();
                    m.type_pos = 0;
                    m.script_line++;
                    m.nodes_pwned = std::min(7, m.nodes_pwned + 1);
                }
            }
            // 3) breach meter + alert
            if (m.breach < 100) m.breach = std::min(100, m.breach + 1);
            m.alert = m.breach < 60 ? 0 : m.breach < 90 ? 1 : 2;
        } else if constexpr (std::is_same_v<T, Toggle>) {
            m.running = !m.running;
        } else if constexpr (std::is_same_v<T, Reboot>) {
            m = Model::boot();
        } else if constexpr (std::is_same_v<T, Escalate>) {
            m.breach = std::min(100, m.breach + 12);
        }
        return { std::move(m), Cmd<Msg>::none() };
    }, msg);
}

} // namespace mtx

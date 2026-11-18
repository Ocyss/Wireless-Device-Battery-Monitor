// AULA SC580 2.4 GHz receiver, hardware-tested (089D:062F, model signature M137).
// Source: https://www.aulacn.com/index/index/driverdownload.html -> SC580 3311,
// AULA SC580 Setup v1.0.3 20250409.exe, statically inspected, never installed:
// MS.ini Protocol=3; CDevDM::GetPower @ 004586A0, SendOne @ 00458190.
// Only read-state command 30 is sent. See docs/aula-sc580.md for framing/limits.
#include "drivers.h"

namespace {
bool BatteryReply(const std::vector<BYTE>& r, DeviceStatus& s) {
    // HidDev strips the unnumbered report ID. Header, payload[27], checksum.
    if (r.size() != 32 || r[0] != 0x30 || r[1] != 0 || r[2] != 1 || r[3] < 11 || r[3] > 27)
        return false;
    BYTE sum = 0;
    for (size_t i = 4; i < 31; ++i) sum = (BYTE)(sum + r[i]);
    if (sum != r[31] || r[4] != 'M' || r[5] != '1' || r[6] != '3' || r[7] != '7' ||
        r[13] > 100 || r[14] > 1) return false;
    s.connected = r[14] != 0;
    s.percent = s.connected ? r[13] : -1;
    s.charging = s.connected && r[12] == 1;  // vendor UI animates charging only for state 1
    return true;
}
}  // namespace

void PollAulaSc580(PollContext& ctx) {
    constexpr USHORT vid = 0x089D, pid = 0x062F;
    if (ctx.IsClaimed(vid, pid)) return;
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
        return c.iface == 2 && c.page() == 0xFF02 && c.usage() == 0 && c.inLen() == 33 && c.outLen() == 33;
    })) {
        HidDev d;
        if (!d.Open(*c)) continue;
        d.Flush();
        // Report ID 0 + command 30, status 0, read/packet 01, length 0,
        // zero payload and checksum. HidDev pads to the 33-byte report length.
        if (!d.Write({ 0, 0x30, 0, 1, 0 }, 200)) return;
        const ULONGLONG end = GetTickCount64() + 800;
        for (;;) {
            int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
            std::vector<BYTE> r;
            if (left <= 0 || d.Read(r, left) < 0) return;
            DeviceStatus s;
            if (!BatteryReply(r, s)) continue;
            s.key = L"aula:SC580";
            s.name = L"AULA SC580";
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            ctx.Add(s, vid, pid);
            return;
        }
    }
}

#include "../src/drivers/drivers.h"
#include <cassert>
#include <cstdio>

namespace {
std::vector<std::vector<BYTE>> pending, writes;
int opens = 0;
bool openOk = true, writeOk = true;
void Seal(std::vector<BYTE>& r) {
    BYTE sum = 0;
    for (size_t i = 4; i < 31; ++i) sum = (BYTE)(sum + r[i]);
    r[31] = sum;
}
std::vector<BYTE> Reply(BYTE percent = 100, BYTE charge = 0, BYTE online = 1) {
    // Actual captured 100% reply, after stripping report ID 0.
    std::vector<BYTE> r = { 0x30, 0, 1, 11, 'M', '1', '3', '7', 1, 0x16, 0x11, 0xAA,
                           charge, percent, online };
    r.resize(32); Seal(r); return r;
}
HidColl Collection() {
    HidColl c; c.vid = 0x089D; c.pid = 0x062F; c.iface = 2;
    c.caps.UsagePage = 0xFF02; c.caps.Usage = 0;
    c.caps.InputReportByteLength = c.caps.OutputReportByteLength = 33;
    return c;
}
PollContext Poll(std::vector<std::vector<BYTE>> frames, HidColl c = Collection()) {
    pending = std::move(frames); writes.clear(); opens = 0;
    PollContext ctx; ctx.colls = { c }; PollAulaSc580(ctx); return ctx;
}
}
bool HidDev::Open(const HidColl&) { ++opens; return openOk; }
void HidDev::Close() {}
void HidDev::Flush() {}
bool HidDev::Write(const std::vector<BYTE>& r, int timeout) {
    assert(timeout == 200);
    assert(r == std::vector<BYTE>({ 0, 0x30, 0, 1, 0 }));
    writes.push_back(r); return writeOk;
}
int HidDev::Read(std::vector<BYTE>& r, int timeout) {
    assert(timeout > 0 && timeout <= 800);
    if (pending.empty()) return -1;
    r = pending.front(); pending.erase(pending.begin()); return (int)r.size();
}
int main() {
    assert(Reply()[31] == 0x1F);
    auto live = Poll({ Reply() });
    assert(live.out.size() == 1 && live.out[0].percent == 100 && !live.out[0].charging);
    assert(live.out[0].kind == DeviceKind::Mouse && live.out[0].name == L"AULA SC580");
    assert(live.IsClaimed(0x089D, 0x062F));
    for (BYTE pct : { 0, 47, 95, 100 }) {
        auto ctx = Poll({ Reply(pct, 1) });
        assert(ctx.out.size() == 1 && ctx.out[0].percent == pct && ctx.out[0].charging);
    }
    auto offline = Poll({ Reply(100, 1, 0) });
    assert(offline.out.size() == 1 && !offline.out[0].connected && offline.out[0].percent == -1);
    assert(!offline.out[0].charging);
    assert(!Poll({ Reply(100, 2) }).out[0].charging);
    assert(Poll({ Reply(101) }).out.empty());
    assert(Poll({ Reply(50, 0, 2) }).out.empty());
    // Independently corrupt header, signature, checksum and truncation; ignore
    // those reports but still accept a subsequent valid matching response.
    for (int index : { 0, 1, 2, 3, 4, 5, 6, 7, 31 }) {
        auto bad = Reply(); bad[index] ^= (index == 3 ? 0x20 : 1);
        if (index < 31) Seal(bad);
        assert(Poll({ bad }).out.empty());
        assert(Poll({ bad, Reply(47) }).out[0].percent == 47);
    }
    auto shortLength = Reply(); shortLength[3] = 10; Seal(shortLength);
    assert(Poll({ shortLength }).out.empty());
    auto corrupt = Reply(); corrupt[20] ^= 1;
    assert(Poll({ corrupt }).out.empty());
    for (size_t size : { 0, 14, 31, 33 }) {
        auto r = Reply(); r.resize(size); assert(Poll({ r }).out.empty());
    }
    assert(Poll({}).out.empty());
    for (int field = 0; field < 7; ++field) {
        auto c = Collection();
        if (field == 0) ++c.vid;
        if (field == 1) ++c.pid;
        if (field == 2) ++c.iface;
        if (field == 3) ++c.caps.UsagePage;
        if (field == 4) ++c.caps.Usage;
        if (field == 5) --c.caps.InputReportByteLength;
        if (field == 6) --c.caps.OutputReportByteLength;
        assert(Poll({ Reply() }, c).out.empty() && opens == 0);
    }
    openOk = false; assert(Poll({ Reply() }).out.empty() && writes.empty()); openOk = true;
    writeOk = false; assert(Poll({ Reply() }).out.empty()); writeOk = true;
    PollContext claimed; claimed.colls = { Collection() }; claimed.Claim(0x089D, 0x062F);
    opens = 0; PollAulaSc580(claimed); assert(opens == 0);
    std::puts("aula_sc580_driver_test: ok");
}

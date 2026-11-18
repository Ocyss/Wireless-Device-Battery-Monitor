# AULA SC580 receiver

Hardware-tested in 2.4 GHz mode on Windows, without installing vendor software.
The receiver is `089D:062F`, USB release `0108`, product `Wireless-Receiver`.
Only interface 2, usage `FF02:0000`, with 33-byte input/output reports is queried.
The separate `FF01:0001` collection is not used.

## Protocol source

The [official AULA download center](https://www.aulacn.com/index/index/driverdownload.html)
links the [SC580 3311 v1.0.3 package](https://cdn.aulacn.com/mouse/AULA%20SC580%20Setup%20v1.0.3%2020250409.exe).
Static inspection of its `device/mouse/sc580/MS.ini` identifies `Protocol=3`,
the receiver IDs above and `Identify=4d,31,33,37` (`M137`). The 3315 package's
configuration lists the same identifiers, but that sensor variant has not been
hardware-tested. Neither installer nor vendor application was executed.

In its 32-bit `CBMouse.exe`, `CDevDM::GetPower` at `004586A0` invokes read-state
command `30` through the packet builder at `00458190` and transport at `00456B80`.
`CDevDM::PrivateSyncCfg` at `00457AD0` also decodes the online flag.
`CPowerDlg::SetPower` at `004438E0` treats charging state 1 as charging.

The request is 33 bytes including unnumbered HID report ID 0:

```text
00 30 00 01 00 [28 zero bytes]
```

After removing report ID 0, the 32-byte reply is:

| Byte | Meaning |
| --- | --- |
| 0 | Command `30` |
| 1 | Result, `00` for success |
| 2 | Read/packet field `01` |
| 3 | Payload length, 11 through 27 accepted |
| 4..7 | Model signature `4D 31 33 37` (`M137`) |
| 12 | Charging state; 1 means charging |
| 13 | Battery percentage, 0 through 100 |
| 14 | Online flag, 0 or 1 |
| 31 | Sum of bytes 4 through 30, modulo 256 |

Captured live reply (100%, not charging, online):

```text
30 00 01 0B 4D 31 33 37 01 16 11 AA 00 64 01 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1F
```

The driver validates the frame, checksum and model signature before claiming the
device. It sends no configuration, pairing, lighting or firmware commands.
Write timeout is 200 ms; one absolute 800 ms read deadline covers all replies.
An explicit offline flag hides the device rather than displaying stale battery.
No response leaves it unclaimed for the normal fallback handling.

## Validation and limits

The initial live read returned 100%, online, not charging. No vendor UI comparison
was performed. Charging, an explicit offline reply, lower battery levels and the
3315 sensor variant have only synthetic test coverage, not hardware confirmation.
USB cable and Bluetooth modes are outside this support change. Battery resolution
over a discharge cycle has not been measured; the driver preserves the reported
percentage without rounding or voltage conversion.

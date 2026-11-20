# SPDX-License-Identifier: MIT
# Read-only: prints the bytes at the three patch sites in the running bluetoothd.
#   sudo lldb -b -n bluetoothd -o 'command script import tools/adv_check_lldb.py' -o detach
import lldb


def __lldb_init_module(debugger, internal_dict):
    target = debugger.GetSelectedTarget()
    process = target.GetProcess()
    header = target.GetModuleAtIndex(0).GetObjectFileHeaderAddress().GetLoadAddress(target)
    slide = header - 0x100000000
    error = lldb.SBError()
    for name, addr, patched in [("extended", 0x10033abf0, "b0015dc3"),
                                ("multiple", 0x10033b716, "b0015dc3"),
                                ("instances", 0x100357fa0, "b0045dc3")]:
        have = process.ReadMemory(addr + slide, 4, error)
        text = have.hex() if have else str(error)
        print("%-10s %s  %s" % (name, text, "PATCHED" if text == patched else "not patched"))

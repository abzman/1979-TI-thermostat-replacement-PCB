#!/usr/bin/env python3
"""Static TMS0970 listing from the baked PLA decode (same decode as the C++ core)."""
import sys
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bake_tables import parse_pla, IPLA, MPLA
R = (sys.argv[1] if len(sys.argv) > 1 else '.') + '/'
rom = open(R + 'tmc0910b', 'rb').read()
ipla = parse_pla(R + 'tms0970_common2_instr.pla', *IPLA)
mpla = parse_pla(R + 'tms0970_tithermos_micro.pla', *MPLA)

MD = ['CKM','CKP','YTP','MTP','ATN','NATN','MTN','15TN','CKN','NE','C8','CIN','AUTA','AUTY','STO']
FD = ['LDP','TDO','COMX','LDX','SBIT','RBIT','SETR','RETN']
def rev(v, n): return int(format(v, '0%db' % n)[::-1], 2)

MN = {  # frozenset of micro flags -> mnemonic (TMS1000 family names)
 frozenset(['ATN','AUTY']):'TAY', frozenset(['YTP','AUTA']):'TYA', frozenset(['AUTA']):'CLA',
 frozenset(['STO']):'TAM', frozenset(['STO','YTP','CIN','C8','AUTY']):'TAMIYC',
 frozenset(['STO','YTP','15TN','C8','AUTY']):'TAMDYN', frozenset(['STO','AUTA']):'TAMZA',
 frozenset(['MTP','AUTY']):'TMY', frozenset(['MTP','AUTA']):'TMA', frozenset(['MTP','AUTA','STO']):'XMA',
 frozenset(['MTP','ATN','C8','AUTA']):'AMAAC', frozenset(['NATN','MTP','CIN','C8','AUTA']):'SAMAN',
 frozenset(['MTP','CIN','C8','AUTA']):'IMAC', frozenset(['MTP','15TN','C8','AUTA']):'DMAN',
 frozenset(['ATN','CIN','AUTA']):'IA', frozenset(['YTP','CIN','C8','AUTY']):'IYC',
 frozenset(['ATN','15TN','C8','AUTA']):'DAN', frozenset(['YTP','15TN','C8','AUTY']):'DYN',
 frozenset(['CKP','AUTY']):'TCY', frozenset(['YTP','CKN','NE']):'YNEC', frozenset(['CKM','YTP','CIN','AUTY']):'TCMIY',
 frozenset(['ATN','CKP','C8']):'ALEC', frozenset(['MTP','ATN','NE']):'MNEA', frozenset(['MTP','NE']):'MNEZ',
 frozenset(['YTP','ATN','NE']):'YNEA', frozenset(['CKP','NE']):'KNEZ', frozenset(['CKP','AUTA']):'TKA',
 frozenset(['MTP','CKN','NE']):'TBIT1', frozenset(['CKP','ATN','C8','AUTA']):'A#AAC',
 frozenset(['CKP','ATN','NE']):'ALEC?',
}
def dis(addr):
    op = rom[addr]
    imask = ipla[op]
    fixed = []
    if op & 0x80: fixed.append('CALL' if op & 0x40 else 'BR')
    for b in range(8):
        if imask & (0x80 << b): fixed.append(FD[b])
    msel = imask & 0x1f
    if (imask & 0x40) and not (imask & 0x20): msel = (op & 0xf) | ((op >> 1) & 0x10)
    msel = rev(msel, 5)
    mm = (mpla[msel] ^ 0x09fe) & 0x7fff
    micro = [MD[i] for i in range(15) if mm & (1 << i)]
    c4 = rev(op & 0xf, 4)
    if 'BR' in fixed or 'CALL' in fixed:
        return '%s %02X' % (fixed[0], op & 0x3f)
    if fixed:
        s = '/'.join(fixed)
        if 'LDP' in fixed or 'LDX' in fixed: s += ' %X' % c4
        if 'SBIT' in fixed or 'RBIT' in fixed: s += ' bit%d' % (op & 3)
        return s
    k = frozenset(micro)
    m = MN.get(k)
    if m:
        if m in ('TCY','TCMIY','A#AAC','KNEZ','ALEC','YNEC'): m += ' %X' % c4
        if m == 'TKA' : m = 'TKA(K)'
        return m
    return '{' + ','.join(micro) + '}' + (' c%X' % c4 if ('CKP' in micro or 'CKM' in micro or 'CKN' in micro) else '')

if __name__ == '__main__':
    for a in range(0, 1024):
        print('%03X: %02X  %s' % (a, rom[a], dis(a)))

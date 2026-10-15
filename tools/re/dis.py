import sys,re,struct,subprocess; import os; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__))); import xref
a,b=sys.argv[1],sys.argv[2]
out=subprocess.run(['objdump','-d','-M','intel','--no-show-raw-insn',f'--start-address={a}',f'--stop-address={b}',xref.EXE],capture_output=True,text=True).stdout.splitlines()[7:]
for l in out:
    m=re.search(r'# 0x([0-9a-f]+)',l); s=''
    if m:
        v=int(m.group(1),16); t=xref.cstr(v,50); o=xref.va2off(v)
        if t.isprintable() and len(t)>3: s=repr(t)
        elif o and ('xmm' in l or 'DWORD' in l): s='f=%g'%struct.unpack_from('<f',xref.mm,o)[0]
    print(l.rstrip()[:100],s)

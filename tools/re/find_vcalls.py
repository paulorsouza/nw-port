import re,sys
src=open(sys.argv[1],errors='replace').read()
funcs=re.split(r'^//==== ',src,flags=re.M)[1:]
rows=[]
for f in funcs:
    addr=f.split('\n',1)[0].strip()
    c40=len(re.findall(r'\+ 0x40\)\)\s*\(',f)); c48=len(re.findall(r'\+ 0x48\)\)\s*\(',f))
    c50=len(re.findall(r'\+ 0x50\)\)\s*\(',f)); c58=len(re.findall(r'\+ 0x58\)\)\s*\(',f))
    p30='+ 0x30)' in f; p58='+ 0x58)' in f; p60='+ 0x60)' in f; p38='+ 0x38' in f
    score=(c40>0)+(c48>0)+p60+p58+p30+p38
    if (c40 or c48) and p60: rows.append((score,addr,c40,c48,c50,c58,p30,p38,p58,p60,len(f)))
for r in sorted(rows,reverse=True)[:40]: print(r)

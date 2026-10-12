"""Imprime uma .behaviortree.json (nwbt --x-objects json) como árvore compacta.

    python3 tools/bt_dump.py extract/serverpak/json/sharedassets/ai/behaviortrees/wolf/wolf_base.behaviortree.json [profundidade_de_subárvores]

Extração: Server.pak (depot 1063731) linkado em extract/serverpak/game/assets; nwbt unpack -g extract/serverpak/game
--x-objects json --crc-file .nwbt/rtti/nwt-crc.json --uuid-file .nwbt/rtti/nwt-types.json "sharedassets/ai/**".
TreeReference traz a subárvore embutida (serializedtree): cada *_base.behaviortree é a IA inteira da criatura.
Condições: opN = operador do jogo ainda não decifrado (pelo uso: 1 ==, 3 <, 4 >, 5 <=?, 6 >=); LOG1 = OR?.
"""
import json,sys
SKIP={'__type','baseclass1','child','children','id','name','baseutility','interruptable','serializedtree'}
def param(p):
    if not isinstance(p,dict): return p
    if p.get('__type')=='BehaviorParameter':
        k=p.get('blackboardkeyname')
        if k: return '$'+k
        if p.get('isdefaultvalue'): return None
        return p.get('storage',{}).get('m_data')
    return p
OPS={}
def opnd(p):
    k=p.get('blackboardkeyname')
    return '$'+k if k else repr(p.get('storage',{}).get('m_data'))
def cond(g):
    el=g.get('conditions',{}).get('element')
    if el is None: return None
    if isinstance(el,dict): el=[el]
    parts=[f"{opnd(c['leftoperand'])} op{c['operator']} {opnd(c['rightoperand'])}" for c in el]
    j=' AND ' if g.get('grouplogicaloperator',0)==0 else f" LOG{g['grouplogicaloperator']} "
    return '('+j.join(parts)+')'
def flat(o,pre=''):
    """campos não default, achatados"""
    out={}
    if not isinstance(o,dict): return out
    for k,v in o.items():
        if k in SKIP: continue
        if isinstance(v,dict):
            if v.get('__type')=='ConditionGroup':
                c=cond(v)
                if c: out[pre+k.replace('conditions','')]=c
                continue
            if v.get('__type')=='BehaviorParameter':
                x=param(v)
                if x not in (None,'',0,False): out[pre+k]=x
            elif v.get('__type')=='Crc32': continue
            else: out.update(flat(v,pre+k+'.'))
        elif isinstance(v,list): continue
        elif v not in (None,'',0,False,-1,'0','00000000-0000-0000-0000-000000000000') and k not in('guid','subid','type','hint'):
            out[pre+k]=v
    return out
def chain(e):
    """tipos (concreto→base), TreeAsset, child, children, campos"""
    types=[];fields={};ta=None;child=None;children=None;sub=None
    while isinstance(e,dict):
        t=e.get('__type');types.append(t)
        if t=='TreeAsset': ta=e;break
        f=flat(e)
        for k,v in f.items(): fields.setdefault(k,v)
        if 'child' in e: child=e['child']
        if 'children' in e: children=e['children']
        if 'serializedtree' in e: sub=e['serializedtree']
        e=e.get('baseclass1')
    return types,ta,fields,child,children,sub
def node(w,d,out,depth):
    e=w.get('element') if isinstance(w,dict) else None
    if not e: return
    types,ta,fields,child,children,sub=chain(e)
    nm=ta.get('name') if ta else ''
    util=ta.get('baseutility') if ta else 0
    extra=[]
    if util and util>1e-6: extra.append(f'u={util:g}')
    if ta and ta.get('interruptable') is False: extra.append('noint')
    fs=' '.join(f'{k}={v}' for k,v in fields.items())
    out.write('  '*d+f'{types[0]} "{nm}"'+(' ['+' '.join(extra)+']' if extra else '')+(' '+fs if fs else '')+'\n')
    if sub:
        st=sub.get('element',{})
        out.write('  '*(d+1)+f'<< {st.get("name")} >>\n')
        if depth: node(st.get('root'),d+1,out,depth-1)
    if child: node(child,d+1,out,depth)
    if children:
        for c in children.get('element',[]): node(c,d+1,out,depth)
d=json.load(open(sys.argv[1]))
node(d['root'],0,sys.stdout,int(sys.argv[2]) if len(sys.argv)>2 else 99)

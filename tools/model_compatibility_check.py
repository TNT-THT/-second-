"""Offline authoring tool: explicit model descriptor only, never a directory scan of the user's PC.
Min/max values are NOT stored in model3.json; the native adapter intersects Core min/max at load.
"""
import argparse,hashlib,json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def normal(p):
    p=re.split(r'[:/]',p)[-1].lower();p=re.sub('[^a-z0-9]','',p);return re.sub(r'^(parameter|param)','',p)
def fingerprint(path,mapping,semantics=()):
    model=json.loads(path.read_text(encoding='utf-8-sig'));base=path.resolve().parent;refs=model['FileReferences']
    def ref(name):
        if not name:return {}
        p=(base/name).resolve()
        if base not in p.parents or not p.is_file() or p.stat().st_size>2**21:return {}
        return json.loads(p.read_text(encoding='utf-8-sig'))
    display=ref(refs.get('DisplayInfo',path.name.replace('.model3','.cdi3')));ids=[x['Id'] for x in display.get('Parameters',[])];bind={o['Destination']['Id'] for p in ref(refs.get('Physics')).get('PhysicsSettings',[]) for o in p.get('Output',[]) if o['Destination'].get('Target')=='Parameter'};found={};missing=[]
    for channel,aliases in mapping['channels'].items():
        choices=[p for p in ids if normal(p) in {normal(a) for a in aliases} and (channel not in mapping['physics_semantics'] or p in bind)]
        exact=[a for a in aliases if a in choices];chosen=exact[0] if exact else choices[0] if len(choices)==1 else None
        if chosen:found[channel]=chosen
        else:missing.append(channel)
    expr=[x['Name'] for x in refs.get('Expressions',[])];policy_missing=[x for x in semantics if x not in found];artifact=refs.get('Moc');m=(base/artifact).resolve() if artifact else None;moc_hash=hashlib.sha256(m.read_bytes()).hexdigest() if m and base in m.parents and m.is_file() and m.stat().st_size<8*1024*1024 else None
    return {'model':path.name,'moc_sha256':moc_hash,'channels':found,'physics':sorted(bind),'missing':missing,'expressions':expr,'neutral':'neutral' in [x.lower() for x in expr], 'policy_unmatched':policy_missing,'fallback_tier':3 if len(found)>=8 else 2 if len(found)>=3 else 1 if found else 0,'fingerprint':hashlib.sha256('\n'.join(sorted(found)).encode()).hexdigest()[:16]}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('model',type=Path);p.add_argument('--mapping',type=Path,default=root/'config/intent_mapping.json');p.add_argument('--policy',type=Path,default=root/'config/policy.json');p.add_argument('--json',action='store_true');args=p.parse_args()
    try:q=fingerprint(args.model,json.loads(args.mapping.read_text()),json.loads(args.policy.read_text())['semantics'])
    except (OSError,KeyError,ValueError,TypeError) as e:p.exit(2,f'invalid model descriptor: {e}\n')
    if args.json:print(json.dumps(q,indent=2,ensure_ascii=False))
    else:
        print(q['model'],'| tier',q['fallback_tier'],'| fingerprint',q['fingerprint']);print('MOC SHA-256:',q['moc_sha256']);print('Bound:',', '.join(q['channels']));print('Physics:',', '.join(q['physics']) or 'none');print('WARN missing:',', '.join(q['missing']) or 'none');print('WARN unmatched strategy:',', '.join(q['policy_unmatched']) or 'none');print('Neutral:', 'native expression' if q['neutral'] else 'parameter defaults; no invented expression')

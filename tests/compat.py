from pathlib import Path
import json,sys
base=Path(__file__).resolve().parents[1];sys.path.insert(0,str(base/'tools'));from model_compatibility_check import fingerprint
m=json.loads((base/'config/intent_mapping.json').read_text());check=lambda p:fingerprint(p,m,['breath','head_tilt','fringe','skirt'])
a=check(base/'assets/models/haru/Haru.model3.json');assert len(a['channels'])==17 and 'skirt' in a['policy_unmatched'] and not a['neutral'] and 'ParamHairFront' in a['physics']
for name,size in [('no-physics',3),('no-expressions',3),('one-channel',1)]:
    r=check(base/'tests/fixtures'/name/'rig.model3.json');assert len(r['channels'])==size and r['fallback_tier']>0;r2=check(base/'tests/fixtures'/name/'rig.model3.json');assert r==r2
# A second official model proves the fingerprint is not Haru-specific and that strategy files carry semantics.
h=check(base/'tests/models/hiyori/Hiyori.model3.json')
assert len(h['channels'])==17 and h['fingerprint']!=a['fingerprint']
assert 'skirt' in h['channels'] and 'sidelock' not in h['channels']
assert 'ParamSkirt' in h['physics'] and 'ParamHairSide' not in h['physics']
assert 'sidelock' in a['channels'] and 'skirt' not in a['channels'] and 'ParamHairSide' in a['physics']
assert not h['policy_unmatched'] and a['policy_unmatched']==['skirt']
assert h==check(base/'tests/models/hiyori/Hiyori.model3.json')
print('Haru + Hiyori + 3 incomplete descriptors passed; semantics rematch per model, unsupported bindings never written.')

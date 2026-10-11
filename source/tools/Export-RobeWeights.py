"""Generate a vertex mask from existing robe bones; never save the source blend."""
import bpy, pathlib
import os
blend=os.environ.get('HP2VR_AVATAR_BLEND')
if not blend: raise SystemExit('Set HP2VR_AVATAR_BLEND to HarryModel1_VR_Rigged.blend. See BUILDING.md.')
bpy.ops.wm.open_mainfile(filepath=blend)
m=bpy.data.objects['HarryModel1_VR']
assert len(m.data.vertices)==1530
groups={g.index for g in m.vertex_groups if g.name.startswith(('bone_RR','bone_RL'))}
weights=[]
for v in m.data.vertices:
    # Source +Y is forward; exported avatar uses -X forward. Use the whole
    # RR/RL panel weights. Distinct rear robe groups remain excluded.
    front=1.0 # Whole front/side panels; previous positional feather left the hem closed.
    weights.append(min(1.,sum(w.weight for w in v.groups if w.group in groups))*front)
assert 10 < sum(w>.001 for w in weights) < 180
out=pathlib.Path(__file__).resolve().parents[1]/'src'/'HP2RobeWeights.h'
out.write_text('#pragma once\n// Generated from existing RR/RL robe bones; source avatar unchanged.\nstatic const float hp2FrontRobeWeights[1530]={\n'+',\n'.join(','.join(f'{w:.7f}f' for w in weights[i:i+16]) for i in range(0,len(weights),16))+'\n};\n')
print('Front robe vertices:',sum(w>.001 for w in weights),'of',len(weights))

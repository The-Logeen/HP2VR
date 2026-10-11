import bpy, collections
import os
blend=os.environ.get('HP2VR_AVATAR_BLEND')
if not blend: raise SystemExit('Set HP2VR_AVATAR_BLEND to HarryModel1_VR_Rigged.blend. See BUILDING.md.')
bpy.ops.wm.open_mainfile(filepath=blend)
m=bpy.data.objects['HarryModel1_VR']
print('ROBE_GROUPS', [g.name for g in m.vertex_groups])
for g in m.vertex_groups:
    vs=[v for v in m.data.vertices if v.co.z<.85 and any(w.group==g.index and w.weight>.2 for w in v.groups)]
    if vs: print('LOWER_GROUP',g.name,len(vs),[tuple(round(f,3) for f in v.co) for v in vs[:3]])
print('MATERIALS',[s.name for s in m.material_slots])

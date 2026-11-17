# Realistic GLB asset shortlist (free sources)

This project uses a lightweight GLB loader. Prefer individual, self-contained,
static GLBs with PNG base-color textures. These links are source pages, not
assets bundled in this archive; download the model, inspect the license, then
rename it to the exact runtime filename in `app/src/main/assets/models/`.

| Runtime filename | Recommended source / asset | License and notes |
|---|---|---|
| `reception.glb` | [WELCOME 160 – Reception Desk (Innerscene)](https://www.innerscene.com/tools/library/3d-parts/welcome-160-reception-desk-21c678a3) | CC0; embedded PBR; ~18k triangles. Best match for the lobby counter. |
| `sofa.glb` | [Lobby sofa cluster (Innerscene)](https://www.innerscene.com/tools/library/3d-parts/lobby-sofa-cluster-original-hotel-furnishing-da8ddd34) | CC0; GLB; group is wide (about 3.65 × 2.20 × 1.05 m), so inspect/crop to one sofa before using in a 2 × 1 m slot. |
| `bed_single.glb`, `bed_double.glb` | [Grab3D – Hotel Room](https://grab3d.com/models/furniture/hotel-room/) | CC0 and realistic hotel-room styling, but it is a whole room diorama, not a drop-in bed. Extract the bed in Blender and export as standalone GLB. |
| `chair.glb`, `table.glb`, `plant.glb` | [Poly Haven source catalog via Mixos](https://www.mixos.io/free-models/furniture) and [iMeshh free models](https://imeshh.com/assets/free-models) | Catalogs include CC0 furniture, plants, seating and tables. Check each asset page and export/download GLB; don't assume every model in a catalog has identical terms. |
| `player.glb`, `guest.glb` | [Poly Pizza](https://poly.pizza/) | Free GLB library; each model is CC0 or CC-BY. Check the individual model license and attribution requirement. Use static, unrigged meshes for this runtime. |

## License tracking

Keep a `THIRD_PARTY_ASSETS.md` entry for every shipped file:
- exact asset title and creator;
- source URL;
- license (CC0 / CC-BY / other);
- date downloaded;
- modifications (scale, decimation, texture conversion).

CC0 sources above allow commercial use as described on their source pages.
CC-BY assets require attribution. Do not ship an asset until its individual
license and redistribution permission are confirmed.

## Important rendering caveat

The source models may advertise PBR materials, but the current renderer only
uses vertex colors (the loader can bake PNG base color into those vertices).
Metallic/roughness and normal maps are not displayed. Full realistic PBR needs
a renderer upgrade (GPU texture upload + tangent-space normal mapping +
metallic/roughness shader); merely adding a GLB does not enable PBR.

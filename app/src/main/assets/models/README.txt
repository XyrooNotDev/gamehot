Hotel Tycoon — Realistic GLB asset slots
========================================

The C++ runtime already tries these exact base names, in this order:
  <name>.glb, <name>.gltf, then <name>.obj

Furniture (must match FurnitureCatalog.cpp):
  bed_single.glb   single hotel bed (target footprint 1 x 2 m)
  bed_double.glb   double hotel bed (target footprint 2 x 2 m)
  reception.glb    reception/front desk (target footprint 2 x 1 m)
  chair.glb        guest chair (target footprint 1 x 1 m)
  table.glb        side/coffee table (target footprint 1 x 1 m)
  plant.glb        potted indoor plant (target footprint 1 x 1 m)
  sofa.glb         lobby sofa (target footprint 2 x 1 m)

Characters:
  player.glb       unrigged, static player mesh
  guest.glb        unrigged, static guest mesh

Place files directly in this folder. No Gradle/CMake source-list changes are
needed: AndroidAssetIO reads app/src/main/assets at runtime. Missing files
automatically fall back to the procedural C++ meshes.

IMPORTANT ENGINE COMPATIBILITY
------------------------------
The current lightweight loader is not a full PBR renderer. It supports glTF
2.0 GLB/GLTF geometry, node transforms, baseColorFactor, and PNG base-color
textures; texture color is sampled and baked into vertex colors. It does NOT
currently render metallic-roughness, normal, occlusion or emissive maps, JPEG
textures, skinning, or skeletal animation. For reliable Android use:
- use self-contained GLB (no external .bin or image dependencies);
- apply transforms and triangulate meshes before export;
- use PNG base-color textures, preferably 512–1024 px;
- keep props around 5k–20k triangles and characters around 10k–25k;
- use static meshes (no rigging/animation);
- orient the model front toward +Z and keep its floor at y=0;
- avoid transparent materials and multiple complicated material variants.

The current loader fits models into catalog footprints; character models are
normalized to 1 m width and 1.35 m height. Check scale and orientation in-game.

FREE SOURCE STARTING POINTS
---------------------------
See ASSET_SOURCES.md in the project root. Verify each individual asset's
license and keep its license/creator note with the shipped build.

"""
Generates /Game/GFC/M_GFCModel, the material the runtime 3D viewer applies to
user models. Run headless (no editor UI needed):

    .\\Scripts\\MakeAssets.ps1

Parameters (set per glTF material by AGFCModelViewer):
    BaseColorTexture (Texture2D, default white), BaseColorFactor (vector),
    Metallic, Roughness (scalars).
Two-sided, because glTF winding can't be trusted for uploads, with a small
emissive lift so unlit sides aren't black on phones without sky lighting.
"""
import unreal

PATH, NAME = "/Game/GFC", "M_GFCModel"
full = f"{PATH}/{NAME}"

if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)

tools = unreal.AssetToolsHelpers.get_asset_tools()
mat = tools.create_asset(NAME, PATH, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("two_sided", True)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)

lib = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty

tex = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
tex.set_editor_property("parameter_name", "BaseColorTexture")
tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

factor = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 150)
factor.set_editor_property("parameter_name", "BaseColorFactor")
factor.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))

mul = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 0)
lib.connect_material_expressions(tex, "RGB", mul, "A")
lib.connect_material_expressions(factor, "", mul, "B")
lib.connect_material_property(mul, "", MP.MP_BASE_COLOR)

lift = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -150, 250)
lift.set_editor_property("const_b", 0.18)
lib.connect_material_expressions(mul, "", lift, "A")
lib.connect_material_property(lift, "", MP.MP_EMISSIVE_COLOR)

metal = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 300)
metal.set_editor_property("parameter_name", "Metallic")
metal.set_editor_property("default_value", 0.0)
lib.connect_material_property(metal, "", MP.MP_METALLIC)

rough = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 400)
rough.set_editor_property("parameter_name", "Roughness")
rough.set_editor_property("default_value", 0.8)
lib.connect_material_property(rough, "", MP.MP_ROUGHNESS)

lib.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(full)
unreal.log(f"GFC: created {full}")

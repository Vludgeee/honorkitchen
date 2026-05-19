# Создаёт только M_EnemySpriteUnlit (без импорта PNG).
# UnrealEditor-Cmd: -ExecutePythonScript=".../create_enemy_sprite_material.py"
import unreal

MAT_PATH = "/Game/Materials/M_EnemySpriteUnlit"
mel = unreal.MaterialEditingLibrary


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH):
        unreal.EditorAssetLibrary.delete_asset(MAT_PATH)

    if not unreal.EditorAssetLibrary.does_directory_exist("/Game/Materials"):
        unreal.EditorAssetLibrary.make_directory("/Game/Materials")

    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_EnemySpriteUnlit",
        "/Game/Materials",
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not mat:
        unreal.log_error("Failed to create M_EnemySpriteUnlit")
        return

    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("two_sided", True)
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    except Exception:
        pass

    if hasattr(mel, "delete_all_material_expressions"):
        mel.delete_all_material_expressions(mat)

    tex_param = mel.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0
    )
    tex_param.set_editor_property("parameter_name", "SpriteTexture")
    tex_param.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    emissive_target = unreal.MaterialProperty.MP_EMISSIVE_COLOR
    if hasattr(unreal.MaterialProperty, "MP_EMISSIVE_COLOR"):
        mel.connect_material_property(tex_param, "", emissive_target)
    else:
        mel.connect_material_property(tex_param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    opacity_target = unreal.MaterialProperty.MP_OPACITY_MASK
    if not hasattr(unreal.MaterialProperty, "MP_OPACITY_MASK"):
        opacity_target = unreal.MaterialProperty.MP_OPACITY
    mel.connect_material_property(tex_param, "A", opacity_target)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
    unreal.log(f"OK: {MAT_PATH} saved={unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH)}")


main()

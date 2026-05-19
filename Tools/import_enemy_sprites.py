# PNG -> Texture2D + M_EnemySpriteUnlit (параметр SpriteTexture)
# Output Log: Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_enemy_sprites.py
import os
import shutil
import unreal

PROJECT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
SOURCE_DIRS = [
    TRIM_CACHE := os.path.join(PROJECT, "Content", "Enemies", "Sprites", "_TrimmedCache"),
    os.path.join(PROJECT, "Content", "Enemies", "Sprites", "_SourcePng"),
    r"C:\Users\vlads\Desktop\Восстановление Курсора\enemys",
]

MAPPING = {
    "tomatosaur front.png": ("Tomatosaur", "Tomatosaur_front"),
    "tomatosaur back.png": ("Tomatosaur", "Tomatosaur_back"),
    "tomatosaur attack.png": ("Tomatosaur", "Tomatosaur_attack"),
    "karavaychick front.png": ("Karavaychick", "Karavaychick_front"),
    "karavaychick back.png": ("Karavaychick", "Karavaychick_back"),
    "karavaychick attack.png": ("Karavaychick", "Karavaychick_attack"),
    "vilokvost.png": ("Vilokhvost", "Vilokvost"),
}

MAT_PATH = "/Game/Materials/M_EnemySpriteUnlit"
TRIM_CACHE = os.path.join(PROJECT, "Content", "Enemies", "Sprites", "_TrimmedCache")


def texture_dimensions(tex):
    if not tex:
        return None, None
    try:
        imported = tex.get_editor_property("imported_size")
        if imported:
            return int(imported.x), int(imported.y)
    except Exception:
        pass
    try:
        return int(tex.get_surface_width()), int(tex.get_surface_height())
    except Exception:
        pass
    return None, None


def trim_png_file(src_path, canonical_name):
    """Обрезка по альфе + RGB=0 где alpha=0; файл сохраняется как {canonical_name}.png."""
    try:
        from PIL import Image
    except ImportError:
        unreal.log_warning("pip install Pillow — обрезка PNG пропущена")
        return src_path

    im = Image.open(src_path).convert("RGBA")
    bbox = im.split()[3].getbbox()
    if not bbox:
        return src_path

    cropped = im.crop(bbox)
    px = list(cropped.getdata())
    cleaned = []
    for r, g, b, a in px:
        if a < 8:
            cleaned.append((0, 0, 0, 0))
        else:
            cleaned.append((r, g, b, a))
    cropped.putdata(cleaned)

    os.makedirs(TRIM_CACHE, exist_ok=True)
    out_path = os.path.join(TRIM_CACHE, f"{canonical_name}.png")
    cropped.save(out_path, "PNG")
    unreal.log(
        f"trim {os.path.basename(src_path)} -> {canonical_name}.png: "
        f"{im.size[0]}x{im.size[1]} -> {cropped.size[0]}x{cropped.size[1]}"
    )
    return out_path


def find_source_dir():
    for d in SOURCE_DIRS:
        if os.path.isdir(d) and any(f.lower().endswith(".png") for f in os.listdir(d)):
            return d
    return None


def ensure_sprite_material():
    mel = unreal.MaterialEditingLibrary

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
        return None

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
    # UE 5.3: на TextureSampleParameter2D нет свойства SRGB — sRGB задаётся на Texture2D.

    # Unlit: цвет только через Emissive (Base Color на MSM_Unlit не светится).
    emissive_target = unreal.MaterialProperty.MP_EMISSIVE_COLOR
    if hasattr(unreal.MaterialProperty, "MP_EMISSIVE_COLOR"):
        mel.connect_material_property(tex_param, "", emissive_target)
    else:
        mel.connect_material_property(tex_param, "", unreal.MaterialProperty.MP_BASE_COLOR)

    opacity_target = unreal.MaterialProperty.MP_OPACITY_MASK
    if not hasattr(unreal.MaterialProperty, "MP_OPACITY_MASK"):
        opacity_target = unreal.MaterialProperty.MP_OPACITY
    try:
        mel.connect_material_property(tex_param, "A", opacity_target)
    except Exception:
        alpha_mask = mel.create_material_expression(
            mat, unreal.MaterialExpressionComponentMask, -80, 120
        )
        alpha_mask.set_editor_property("R", False)
        alpha_mask.set_editor_property("G", False)
        alpha_mask.set_editor_property("B", False)
        alpha_mask.set_editor_property("A", True)
        mel.connect_material_expressions(tex_param, "", alpha_mask, "")
        mel.connect_material_property(alpha_mask, "", opacity_target)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
    unreal.log("Rebuilt M_EnemySpriteUnlit (unlit emissive + alpha mask)")
    return mat


def import_png(src_path, dest_path, asset_name):
    full = f"{dest_path}/{asset_name}"
    for old in (full, f"{dest_path}/T_{asset_name}", f"{dest_path}/{asset_name.replace('_', '')}"):
        if unreal.EditorAssetLibrary.does_asset_exist(old):
            unreal.EditorAssetLibrary.delete_asset(old)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src_path)
    task.set_editor_property("destination_path", dest_path)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if hasattr(task, "destination_name"):
        task.set_editor_property("destination_name", asset_name)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def find_texture_asset(dest_path, asset_name, interchange_name):
    """Ищем текстуру только по ожидаемым именам (не трогаем attack при импорте front)."""
    for name in (asset_name, interchange_name):
        path = f"{dest_path}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return unreal.load_asset(path)
    cache_path = f"/Game/Enemies/Sprites/_TrimmedCache/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(cache_path):
        unreal.log_warning(f"using _TrimmedCache for {asset_name} — copy to {dest_path}")
        return unreal.load_asset(cache_path)
    return None


def tune_texture(tex):
    if not tex:
        return
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("srgb", True)
    try:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    except Exception:
        pass
    for comp in (
        unreal.TextureCompressionSettings.TC_DEFAULT,
        unreal.TextureCompressionSettings.TC_ALPHA,
    ):
        try:
            tex.set_editor_property("compression_settings", comp)
            break
        except Exception:
            continue
    try:
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    except Exception:
        pass
    try:
        tex.set_editor_property("never_stream", True)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(tex)


src_dir = find_source_dir()
if not src_dir:
    unreal.log_error("No PNG folder. Copy files to Content/Enemies/Sprites/_SourcePng")
    raise SystemExit(1)

if not ensure_sprite_material():
    unreal.log_error("M_EnemySpriteUnlit not created — sprites will stay OFF until fixed")
    raise SystemExit(1)

unreal.log(f"PNG source: {src_dir}")

files = {f.lower(): f for f in os.listdir(src_dir) if f.lower().endswith(".png")}
imported = 0

for key, (folder, tex_name) in MAPPING.items():
    real_name = files.get(key) or files.get(f"{tex_name.lower()}.png")
    if not real_name:
        unreal.log_warning(f"Missing: {key} or {tex_name}.png")
        continue

    dest_path = f"/Game/Enemies/Sprites/{folder}"
    if not unreal.EditorAssetLibrary.does_directory_exist(dest_path):
        unreal.EditorAssetLibrary.make_directory(dest_path)

    src_full = os.path.join(src_dir, real_name)
    import_path = trim_png_file(src_full, tex_name)
    interchange_name = os.path.splitext(os.path.basename(import_path))[0]
    import_png(import_path, dest_path, tex_name)

    disk_dest = os.path.join(PROJECT, "Content", *dest_path.replace("/Game/", "").split("/"))
    disk_cache = os.path.join(TRIM_CACHE, f"{tex_name}.uasset")
    disk_target = os.path.join(disk_dest, f"{tex_name}.uasset")
    if not os.path.isfile(disk_target) and os.path.isfile(disk_cache):
        os.makedirs(disk_dest, exist_ok=True)
        shutil.copy2(disk_cache, disk_target)
        unreal.log(f"copied {tex_name}.uasset -> {folder}/")

    tex = find_texture_asset(dest_path, tex_name, interchange_name)
    asset_path = f"{dest_path}/{tex_name}"
    if tex:
        try:
            tune_texture(tex)
            w, h = texture_dimensions(tex)
            if w and h:
                unreal.log(f"OK {asset_path} size={w}x{h}")
            else:
                unreal.log(f"OK {asset_path}")
        except Exception as exc:
            unreal.log_warning(f"tune_texture {asset_path}: {exc}")
    else:
        unreal.log_error(f"FAILED load {asset_path}")
    imported += 1

unreal.log(f"DONE: {imported} textures, material at {MAT_PATH}. Ctrl+Alt+F11, then Play.")

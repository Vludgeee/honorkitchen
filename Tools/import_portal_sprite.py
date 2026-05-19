# PortalSprite.png -> /Game/Portal/PortalSprite (uses M_EnemySpriteUnlit from import_enemy_sprites.py)
# Output Log: Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_portal_sprite.py
import os
import shutil
import unreal

PROJECT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
SOURCE_PATHS = [
    r"C:\Users\vlads\Desktop\Восстановление Курсора\enemys\PortalSprite.png",
    os.path.join(PROJECT, "Content", "Portal", "_SourcePng", "PortalSprite.png"),
]
DEST_PATH = "/Game/Portal"
ASSET_NAME = "PortalSprite"
MAT_PATH = "/Game/Materials/M_EnemySpriteUnlit"
TRIM_CACHE = os.path.join(PROJECT, "Content", "Portal", "_TrimmedCache")
MAX_IMPORT_DIM = 512


def texture_dimensions(tex):
    """UE 5.3: у Texture2D нет get_size_x()."""
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


def trim_png(src_path):
    try:
        from PIL import Image
    except ImportError:
        unreal.log_warning("pip install Pillow — trim skipped")
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
    out_path = os.path.join(TRIM_CACHE, f"{ASSET_NAME}.png")
    if max(cropped.size) > MAX_IMPORT_DIM:
        cropped.thumbnail((MAX_IMPORT_DIM, MAX_IMPORT_DIM), Image.Resampling.NEAREST)

    cropped.save(out_path, "PNG")
    unreal.log(
        f"trim PortalSprite: {im.size[0]}x{im.size[1]} -> {cropped.size[0]}x{cropped.size[1]}"
    )
    return out_path


def find_source():
    for p in SOURCE_PATHS:
        if os.path.isfile(p):
            return p
    return None


def import_png(src_path):
    full = f"{DEST_PATH}/{ASSET_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", src_path)
    task.set_editor_property("destination_path", DEST_PATH)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    if hasattr(task, "destination_name"):
        task.set_editor_property("destination_name", ASSET_NAME)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def tune_texture(tex):
    if not tex:
        return
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    tex.set_editor_property("srgb", True)
    try:
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    except Exception:
        pass
    try:
        tex.set_editor_property("never_stream", True)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(tex)


src = find_source()
if not src:
    unreal.log_error("PortalSprite.png not found. Put it in enemys/ or Content/Portal/_SourcePng/")
    raise SystemExit(1)

if not unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH):
    unreal.log_error(f"{MAT_PATH} missing — run import_enemy_sprites.py first")
    raise SystemExit(1)

if not unreal.EditorAssetLibrary.does_directory_exist(DEST_PATH):
    unreal.EditorAssetLibrary.make_directory(DEST_PATH)

import_path = trim_png(src)
import_png(import_path)

tex = unreal.load_asset(f"{DEST_PATH}/{ASSET_NAME}")
if tex:
    tune_texture(tex)
    w, h = texture_dimensions(tex)
    if w and h:
        unreal.log(f"OK {DEST_PATH}/{ASSET_NAME} size={w}x{h}")
    else:
        unreal.log(f"OK {DEST_PATH}/{ASSET_NAME} (imported)")
else:
    unreal.log_error(f"FAILED {DEST_PATH}/{ASSET_NAME}")

# Keep a copy in project for git
disk_src = os.path.join(PROJECT, "Content", "Portal", "_SourcePng", "PortalSprite.png")
os.makedirs(os.path.dirname(disk_src), exist_ok=True)
if os.path.normcase(src) != os.path.normcase(disk_src):
    shutil.copy2(src, disk_src)

unreal.log("DONE portal sprite. Ctrl+Alt+F11, then Play.")

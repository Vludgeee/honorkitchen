# PNG -> Texture2D for pickup sprites (world billboard + hotbar icon)
# Output Log: Py "D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_pickup_sprites.py"
import os
import shutil
import unreal

PROJECT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
SOURCE_DIRS = [
    r"C:\Users\vlads\Desktop\Восстановление Курсора\enemys",
    os.path.join(PROJECT, "Content", "Enemies", "Sprites", "_SourcePng"),
]
DEST_DISK = os.path.join(PROJECT, "Content", "UI", "PickupSprites")
DEST_UE = "/Game/UI/PickupSprites"

# Имена файлов: Battery.png, Crumb.png, Salt.png, Water.png, MedKit.png (регистр не важен)
MAPPING = {
    "Battery.png": "T_PickupBattery",
    "Crumb.png": "T_PickupCrumb",
    "Salt.png": "T_PickupSalt",
    "Water.png": "T_PickupWater",
    "MedKit.png": "T_PickupMedkit",
}


def find_source_file(lower_name: str):
    for root in SOURCE_DIRS:
        if not os.path.isdir(root):
            continue
        for file_name in os.listdir(root):
            if file_name.lower() == lower_name:
                return os.path.join(root, file_name)
    return None


def import_texture(src_path: str, asset_name: str) -> bool:
    os.makedirs(DEST_DISK, exist_ok=True)
    dst_path = os.path.join(DEST_DISK, f"{asset_name}.png")
    shutil.copy2(src_path, dst_path)

    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_UE):
        unreal.EditorAssetLibrary.make_directory(DEST_UE)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", dst_path)
    task.set_editor_property("destination_path", DEST_UE)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(f"{DEST_UE}/{asset_name}")
    if not isinstance(asset, unreal.Texture2D):
        unreal.log_error(f"FAIL {asset_name}: imported asset is not Texture2D")
        return False

    try:
        asset.set_editor_property("srgb", True)
        asset.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        asset.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        asset.set_editor_property("never_stream", True)
    except Exception:
        pass

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    unreal.log(f"OK {DEST_UE}/{asset_name} from {src_path}")
    return True


def main():
    ok = 0
    for source_name, asset_name in MAPPING.items():
        src = find_source_file(source_name.lower())
        if not src:
            unreal.log_error(f"Missing source PNG: {source_name}")
            continue
        if import_texture(src, asset_name):
            ok += 1

    unreal.log(f"Pickup sprites import finished: {ok}/{len(MAPPING)}")


if __name__ == "__main__":
    main()


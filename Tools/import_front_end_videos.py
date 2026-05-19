# Импорт роликов меню: копирует MP4 в Content/Movies (путь для FileMediaSource / рантайм).
# Output Log: Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_front_end_videos.py
import os
import shutil
import unreal

PROJECT_ROOT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
MOVIES_DIR = os.path.join(PROJECT_ROOT, "Content", "Movies")

FILES = {
    "HonorKitchenBackground.mp4": r"C:\Users\vlads\Videos\Movavi Library\HonorKitchen фон.mp4",
    "HonorKitchenLoading.mp4": r"C:\Users\vlads\Videos\Movavi Library\HonorKitchen загрузка.mp4",
}


def main():
    os.makedirs(MOVIES_DIR, exist_ok=True)
    for dest_name, src in FILES.items():
        dest = os.path.join(MOVIES_DIR, dest_name)
        if os.path.isfile(src):
            shutil.copy2(src, dest)
            unreal.log(f"Copied {src} -> {dest}")
        elif os.path.isfile(dest):
            unreal.log(f"Using existing {dest}")
        else:
            unreal.log_error(f"Missing video: {src} and {dest}")

    unreal.log("Front-end videos ready in Content/Movies/. Rebuild and run.")


if __name__ == "__main__":
    main()

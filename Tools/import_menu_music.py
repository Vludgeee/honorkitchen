# Импорт музыки меню: MP3 -> WAV (UE 5.3 не импортирует .mp3), затем Sound Wave.
# Output Log: Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_menu_music.py
import unreal
import os
import subprocess

PROJECT_ROOT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
CONTENT_DIR = os.path.join(PROJECT_ROOT, "Content", "Audio", "Menu")
ASSET_NAME = "Clarinet"
SOURCE_MP3 = os.path.join(CONTENT_DIR, f"{ASSET_NAME}.mp3")
SOURCE_WAV = os.path.join(CONTENT_DIR, f"{ASSET_NAME}.wav")
EXTERNAL_MP3 = r"D:\vlads\Documents\Demo\Clarinet.mp3"
DEST_PATH = "/Game/Audio/Menu"
FULL_ASSET = f"{DEST_PATH}/{ASSET_NAME}"


def ensure_wav_on_disk() -> str:
	os.makedirs(CONTENT_DIR, exist_ok=True)

	if os.path.isfile(SOURCE_WAV) and os.path.getsize(SOURCE_WAV) > 10000:
		unreal.log(f"Using existing WAV: {SOURCE_WAV}")
		return SOURCE_WAV

	mp3 = SOURCE_MP3
	if not os.path.isfile(mp3) and os.path.isfile(EXTERNAL_MP3):
		unreal.log(f"Copying MP3 from {EXTERNAL_MP3}")
		import shutil
		shutil.copy2(EXTERNAL_MP3, mp3)

	if not os.path.isfile(mp3):
		unreal.log_error(f"No MP3 found. Put Clarinet.mp3 in {CONTENT_DIR} or at {EXTERNAL_MP3}")
		raise SystemExit(1)

	ffmpeg = None
	for candidate in ("ffmpeg",):
		try:
			subprocess.run([candidate, "-version"], capture_output=True, check=True)
			ffmpeg = candidate
			break
		except (FileNotFoundError, subprocess.CalledProcessError):
			pass

	if not ffmpeg:
		try:
			import imageio_ffmpeg
			ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
		except Exception:
			ffmpeg = None

	if not ffmpeg:
		unreal.log_error(
			"Cannot convert MP3 to WAV. Either:\n"
			f"  1) Export WAV into {SOURCE_WAV}\n"
			"  2) Install ffmpeg, or run: pip install imageio-ffmpeg"
		)
		raise SystemExit(1)

	unreal.log(f"Converting {mp3} -> {SOURCE_WAV}")
	subprocess.run(
		[ffmpeg, "-y", "-i", mp3, "-ac", "2", "-ar", "44100", SOURCE_WAV],
		check=True,
	)
	if not os.path.isfile(SOURCE_WAV) or os.path.getsize(SOURCE_WAV) < 10000:
		unreal.log_error("WAV conversion failed or file too small.")
		raise SystemExit(1)
	unreal.log(f"WAV ready ({os.path.getsize(SOURCE_WAV)} bytes)")
	return SOURCE_WAV


def is_valid_sound_wave(asset) -> bool:
	if not asset or not isinstance(asset, unreal.SoundWave):
		return False
	try:
		return asset.get_editor_property("duration") > 1.0
	except Exception:
		return False


if not unreal.EditorAssetLibrary.does_directory_exist(DEST_PATH):
	unreal.EditorAssetLibrary.make_directory(DEST_PATH)

wav_path = ensure_wav_on_disk()

existing = unreal.load_asset(FULL_ASSET) if unreal.EditorAssetLibrary.does_asset_exist(FULL_ASSET) else None
if existing and not is_valid_sound_wave(existing):
	unreal.log_warning(f"Removing invalid asset ({existing.get_class().get_name()})")
	unreal.EditorAssetLibrary.delete_asset(FULL_ASSET)
	existing = None

if not is_valid_sound_wave(existing):
	sound_factory = unreal.SoundFactory()
	task = unreal.AssetImportTask()
	task.set_editor_property("filename", wav_path)
	task.set_editor_property("destination_path", DEST_PATH)
	task.set_editor_property("destination_name", ASSET_NAME)
	task.set_editor_property("automated", True)
	task.set_editor_property("save", True)
	task.set_editor_property("replace_existing", True)
	task.set_editor_property("factory", sound_factory)
	unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
	unreal.log(f"Imported WAV as SoundWave: {FULL_ASSET}")

asset = unreal.load_asset(FULL_ASSET)
if is_valid_sound_wave(asset):
	asset.set_editor_property("looping", True)
	unreal.EditorAssetLibrary.save_loaded_asset(asset)
	dur = asset.get_editor_property("duration")
	unreal.log(f"SUCCESS: {ASSET_NAME} SoundWave, duration={dur:.1f}s, looping=True")
else:
	cls = asset.get_class().get_name() if asset else "None"
	unreal.log_error(f"Import failed — got {cls}. Open {wav_path} via Import in Content Browser.")

# Импорт атмосферных WAV/MP3 -> SoundWave в Content/Audio/Atmosphere/
# Источник: C:\Users\vlads\Desktop\Восстановление Курсора\EnemysSounds\Atmosphere
# Output Log: Py "D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_atmosphere_sounds.py"
import glob
import os
import shutil
import subprocess
import unreal

PROJECT_ROOT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
SOURCE_DIR = r"C:\Users\vlads\Desktop\Восстановление Курсора\EnemysSounds\Atmosphere"
CONTENT_ATMOSPHERE = os.path.join(PROJECT_ROOT, "Content", "Audio", "Atmosphere")
DEST_UE = "/Game/Audio/Atmosphere"

# (имя файла в SOURCE_DIR, имя ассета UE, loop)
IMPORTS = [
	("KitchenReady_Master.wav", "KitchenReady_Master", False),
	("KitchenDUDUDU_Master.wav", "KitchenDUDUDU_Master", False),
	("KitchenGlitch1_Master.wav", "KitchenGlitch1_Master", False),
	("KitchenScrip_Master.wav", "KitchenScrip_Master", False),
	("KitchenTuTuTuuu_Master.wav", "KitchenTuTuTuuu_Master", False),
	("sound_19805.mp3", "sound_19805", True),
]


def _ffmpeg_runs(path):
	if not path or not os.path.isfile(path):
		return False
	try:
		subprocess.run([path, "-version"], capture_output=True, check=True, timeout=20)
		return True
	except (FileNotFoundError, subprocess.CalledProcessError, OSError):
		return False


def _iter_ffmpeg_candidates():
	env_path = os.environ.get("FFMPEG_PATH", "").strip()
	if env_path:
		yield env_path
	which = shutil.which("ffmpeg")
	if which:
		yield which
	try:
		import imageio_ffmpeg
		yield imageio_ffmpeg.get_ffmpeg_exe()
	except Exception:
		pass
	home = os.path.expanduser("~")
	for pattern in (
		os.path.join(home, "AppData", "Local", "Packages", "Python*", "LocalCache", "local-packages", "Python*", "site-packages", "imageio_ffmpeg", "binaries", "*.exe"),
		os.path.join(home, "AppData", "Local", "Programs", "Python", "Python*", "Lib", "site-packages", "imageio_ffmpeg", "binaries", "*.exe"),
	):
		for exe in sorted(glob.glob(pattern), reverse=True):
			yield exe


def find_ffmpeg():
	seen = set()
	for candidate in _iter_ffmpeg_candidates():
		if not candidate:
			continue
		key = os.path.normcase(os.path.abspath(candidate))
		if key in seen:
			continue
		seen.add(key)
		if _ffmpeg_runs(candidate):
			unreal.log("import_atmosphere_sounds: ffmpeg = {}".format(candidate))
			return candidate
	return None


def audio_to_wav(src_path, wav_path, ffmpeg):
	if os.path.isfile(wav_path) and os.path.getsize(wav_path) > 8000:
		src_mtime = os.path.getmtime(src_path)
		if os.path.getmtime(wav_path) >= src_mtime:
			return
	ext = os.path.splitext(src_path)[1].lower()
	if ext == ".wav":
		shutil.copy2(src_path, wav_path)
		return
	if not ffmpeg:
		raise RuntimeError("ffmpeg required for {}".format(src_path))
	subprocess.run([ffmpeg, "-y", "-i", src_path, "-ac", "2", "-ar", "44100", wav_path], check=True)


def is_valid_sound_wave(asset):
	if not asset or not isinstance(asset, unreal.SoundWave):
		return False
	try:
		return asset.get_editor_property("duration") > 0.05
	except Exception:
		return False


def import_wav(wav_path, asset_name, b_loop):
	full_asset = "{}/{}".format(DEST_UE, asset_name)
	if not unreal.EditorAssetLibrary.does_directory_exist(DEST_UE):
		unreal.EditorAssetLibrary.make_directory(DEST_UE)

	existing = unreal.load_asset(full_asset) if unreal.EditorAssetLibrary.does_asset_exist(full_asset) else None
	if existing and not is_valid_sound_wave(existing):
		unreal.EditorAssetLibrary.delete_asset(full_asset)
		existing = None

	if not is_valid_sound_wave(existing):
		task = unreal.AssetImportTask()
		task.set_editor_property("filename", wav_path)
		task.set_editor_property("destination_path", DEST_UE)
		task.set_editor_property("destination_name", asset_name)
		task.set_editor_property("automated", True)
		task.set_editor_property("save", True)
		task.set_editor_property("replace_existing", True)
		task.set_editor_property("factory", unreal.SoundFactory())
		unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

	asset = unreal.load_asset(full_asset)
	if is_valid_sound_wave(asset):
		asset.set_editor_property("looping", b_loop)
		try:
			asset.set_editor_property("is_ui_sound", False)
		except Exception:
			pass
		unreal.EditorAssetLibrary.save_loaded_asset(asset)
		dur = asset.get_editor_property("duration")
		unreal.log("OK {} loop={} dur={:.2f}s".format(full_asset, b_loop, dur))
		return True

	unreal.log_error("FAIL {}".format(full_asset))
	return False


def main():
	if not os.path.isdir(SOURCE_DIR):
		unreal.log_error("Source folder missing: {}".format(SOURCE_DIR))
		raise SystemExit(1)

	ffmpeg = find_ffmpeg()
	needs_ffmpeg = any(not f.lower().endswith(".wav") for f, _, _ in IMPORTS)
	if needs_ffmpeg and not ffmpeg:
		unreal.log_error("ffmpeg not found — нужен для sound_19805.mp3 (pip install imageio-ffmpeg или FFMPEG_PATH)")
		raise SystemExit(1)

	os.makedirs(CONTENT_ATMOSPHERE, exist_ok=True)
	ok = 0
	for src_name, asset_name, b_loop in IMPORTS:
		src = os.path.join(SOURCE_DIR, src_name)
		if not os.path.isfile(src):
			unreal.log_error("Missing source: {}".format(src))
			continue
		wav_name = "{}.wav".format(asset_name)
		wav_path = os.path.join(CONTENT_ATMOSPHERE, wav_name)
		audio_to_wav(src, wav_path, ffmpeg)
		if import_wav(wav_path, asset_name, b_loop):
			ok += 1

	unreal.log("Atmosphere sounds import finished: {}/{}".format(ok, len(IMPORTS)))


if __name__ == "__main__":
	main()

# Импорт SFX врагов: MP3 -> WAV -> SoundWave в Content/Audio/Enemies/
# Источник: C:\Users\vlads\Desktop\Восстановление Курсора\EnemysSounds
# Output Log: Py "D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_enemy_sounds.py"
import glob
import os
import shutil
import subprocess
import unreal

PROJECT_ROOT = r"D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject"
SOURCE_DIR = r"C:\Users\vlads\Desktop\Восстановление Курсора\EnemysSounds"
CONTENT_AUDIO = os.path.join(PROJECT_ROOT, "Content", "Audio", "Enemies")

# (подпапка Content, исходный mp3, имя ассета UE, loop)
IMPORTS = [
	("TomatoSaurus", "TomatoSaurusIdle.mp3", "TomatoSaurusIdle", True),
	("TomatoSaurus", "TomatoSaurusIdle (2).mp3", "TomatoSaurusIdle_2", True),
	("TomatoSaurus", "TomatoSaurusIdle (3).mp3", "TomatoSaurusIdle_3", True),
	("TomatoSaurus", "TomatoSaurusChase.mp3", "TomatoSaurusChase", False),
	("TomatoSaurus", "TomatoSaurusChase (2).mp3", "TomatoSaurusChase_2", False),
	("TomatoSaurus", "TomatoSaurusPunch.mp3", "TomatoSaurusPunch", False),
	("TomatoSaurus", "TomatoSaurusPunch (2).mp3", "TomatoSaurusPunch_2", False),
	("TomatoSaurus", "TomatoSaurusDamage.mp3", "TomatoSaurusDamage", False),
	("Karavaychik", "KaravaychickIdle.mp3", "KaravaychickIdle", True),
	("Karavaychik", "KaravaychickDetected.mp3", "KaravaychickDetected", False),
	("Karavaychik", "KaravaychickChase.mp3", "KaravaychickChase", False),
	("Karavaychik", "KaravaychickPunch.mp3", "KaravaychickPunch", False),
	("Karavaychik", "KaravaychickPunch (2).mp3", "KaravaychickPunch_2", False),
	("Karavaychik", "KaravaychickDamage.mp3", "KaravaychickDamage", False),
	("Vilokhvost", "VilokhvostIdle.mp3", "VilokhvostIdle", True),
	("Vilokhvost", "VilokhvostIdle (2).mp3", "VilokhvostIdle_2", True),
	("Vilokhvost", "VilokhvostIdle (3).mp3", "VilokhvostIdle_3", True),
	("Vilokhvost", "VilokhvostDetected.mp3", "VilokhvostDetected", False),
	("Vilokhvost", "VilokhvostDetected3.mp3", "VilokhvostDetected3", False),
	("Vilokhvost", "VilokhvostPunch.mp3", "VilokhvostPunch", False),
]


def _ffmpeg_runs(path: str) -> bool:
	if not path or not os.path.isfile(path):
		return False
	try:
		subprocess.run([path, "-version"], capture_output=True, check=True, timeout=20)
		return True
	except (FileNotFoundError, subprocess.CalledProcessError, OSError):
		return False


def _iter_ffmpeg_candidates():
	"""UE Editor Python != системный pip; ищем ffmpeg в PATH, env и site-packages Windows."""
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
	globs = [
		os.path.join(
			home,
			"AppData",
			"Local",
			"Packages",
			"Python*",
			"LocalCache",
			"local-packages",
			"Python*",
			"site-packages",
			"imageio_ffmpeg",
			"binaries",
			"*.exe",
		),
		os.path.join(
			home,
			"AppData",
			"Local",
			"Programs",
			"Python",
			"Python*",
			"Lib",
			"site-packages",
			"imageio_ffmpeg",
			"binaries",
			"*.exe",
		),
		os.path.join(
			home,
			"AppData",
			"Roaming",
			"Python",
			"Python*",
			"site-packages",
			"imageio_ffmpeg",
			"binaries",
			"*.exe",
		),
	]
	for pattern in globs:
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
			unreal.log(f"import_enemy_sounds: ffmpeg = {candidate}")
			return candidate
	return None


def mp3_to_wav(mp3_path: str, wav_path: str, ffmpeg: str) -> None:
	if os.path.isfile(wav_path) and os.path.getsize(wav_path) > 8000:
		return
	subprocess.run([ffmpeg, "-y", "-i", mp3_path, "-ac", "2", "-ar", "44100", wav_path], check=True)


def is_valid_sound_wave(asset) -> bool:
	if not asset or not isinstance(asset, unreal.SoundWave):
		return False
	try:
		return asset.get_editor_property("duration") > 0.05
	except Exception:
		return False


def import_wav(wav_path: str, dest_path: str, asset_name: str, b_loop: bool) -> bool:
	full_asset = f"{dest_path}/{asset_name}"
	if not unreal.EditorAssetLibrary.does_directory_exist(dest_path):
		unreal.EditorAssetLibrary.make_directory(dest_path)

	existing = unreal.load_asset(full_asset) if unreal.EditorAssetLibrary.does_asset_exist(full_asset) else None
	if existing and not is_valid_sound_wave(existing):
		unreal.EditorAssetLibrary.delete_asset(full_asset)
		existing = None

	if not is_valid_sound_wave(existing):
		task = unreal.AssetImportTask()
		task.set_editor_property("filename", wav_path)
		task.set_editor_property("destination_path", dest_path)
		task.set_editor_property("destination_name", asset_name)
		task.set_editor_property("automated", True)
		task.set_editor_property("save", True)
		task.set_editor_property("replace_existing", True)
		task.set_editor_property("factory", unreal.SoundFactory())
		unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

	asset = unreal.load_asset(full_asset)
	if is_valid_sound_wave(asset):
		asset.set_editor_property("looping", b_loop)
		# 3D в мире (затухание задаётся в HonorKitchenMonsterAudio при воспроизведении).
		try:
			asset.set_editor_property("is_ui_sound", False)
			asset.set_editor_property("override_attenuation", False)
		except Exception:
			pass
		unreal.EditorAssetLibrary.save_loaded_asset(asset)
		dur = asset.get_editor_property("duration")
		unreal.log(f"OK {full_asset} loop={b_loop} dur={dur:.2f}s")
		return True

	unreal.log_error(f"FAIL {full_asset}")
	return False


def main():
	if not os.path.isdir(SOURCE_DIR):
		unreal.log_error(f"Source folder missing: {SOURCE_DIR}")
		raise SystemExit(1)

	ffmpeg = find_ffmpeg()
	if not ffmpeg:
		unreal.log_error(
			"ffmpeg not found. Варианты:\n"
			"  1) В PowerShell: pip install imageio-ffmpeg (уже есть у вас)\n"
			"  2) Задать переменную FFMPEG_PATH на .exe из imageio_ffmpeg\\binaries\\\n"
			"  3) Добавить ffmpeg в PATH (winget install ffmpeg)"
		)
		raise SystemExit(1)

	ok = 0
	for folder, src_name, asset_name, b_loop in IMPORTS:
		src_mp3 = os.path.join(SOURCE_DIR, src_name)
		if not os.path.isfile(src_mp3):
			unreal.log_error(f"Missing source: {src_mp3}")
			continue

		dest_disk = os.path.join(CONTENT_AUDIO, folder)
		os.makedirs(dest_disk, exist_ok=True)
		wav_path = os.path.join(dest_disk, f"{asset_name}.wav")
		mp3_to_wav(src_mp3, wav_path, ffmpeg)

		dest_ue = f"/Game/Audio/Enemies/{folder}"
		if import_wav(wav_path, dest_ue, asset_name, b_loop):
			ok += 1

	unreal.log(f"Enemy sounds import finished: {ok}/{len(IMPORTS)}")

	player_wav = os.path.join(SOURCE_DIR, "Death_Master.wav")
	player_disk = os.path.join(PROJECT_ROOT, "Content", "Audio", "Player")
	player_dest_ue = "/Game/Audio/Player"
	if os.path.isfile(player_wav):
		os.makedirs(player_disk, exist_ok=True)
		shutil.copy2(player_wav, os.path.join(player_disk, "Death_Master.wav"))
		if import_wav(os.path.join(player_disk, "Death_Master.wav"), player_dest_ue, "Death_Master", False):
			unreal.log("OK player death sound Death_Master")
		else:
			unreal.log_error("FAIL player death sound Death_Master")
	else:
		unreal.log_error(f"Missing Death_Master.wav in {SOURCE_DIR}")


if __name__ == "__main__":
	main()

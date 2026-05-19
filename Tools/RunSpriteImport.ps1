# Запускать при ОТКРЫТОМ Unreal Editor (Output Log):
#   Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_enemy_sprites.py
Write-Host "1. Откройте MyProject в Unreal Editor"
Write-Host "2. Output Log -> выполните:"
Write-Host '   Py D:/vlads/Documents/Unreal Projects/HonorKitchen3/MyProject/Tools/import_enemy_sprites.py'
Write-Host "3. Ctrl+Alt+F11 (Live Coding)"
Write-Host "4. Play"
Write-Host ""
$mat = "D:\vlads\Documents\Unreal Projects\HonorKitchen3\MyProject\Content\Materials\M_EnemySpriteUnlit.uasset"
if (Test-Path $mat) {
    Write-Host "OK: M_EnemySpriteUnlit exists" -ForegroundColor Green
} else {
    Write-Host "MISSING: M_EnemySpriteUnlit — белые квадраты до запуска Py!" -ForegroundColor Red
}

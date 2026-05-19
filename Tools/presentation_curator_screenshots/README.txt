Скриншоты для презентации куратору (колледж)
==========================================

Положите PNG/JPG с такими именами (без расширения в имени — только файл):

  01_title.png          — титул / логотип / кадр из игры
  02_since_demo.png     — опционально: кадр «как было» или меню
  03_ue_editor.png      — Unreal Editor (по желанию)
  04_vs.png             — Visual Studio (по желанию)
  05_gameplay.png       — геймплей / кухня / генерация
  06_inventory.png      — хотбар / предметы
  07_enemies.png        — враги на уровне

Если файла нет, скрипт создаст серую заглушку с подсказкой (нужен Pillow).

Запуск генерации презентации из корня проекта MyProject:

  py -3 Tools\gen_curator_presentation_pptx.py

или

  python Tools/gen_curator_presentation_pptx.py

Результат: Docs\CURATOR_COLLEGE_PRESENTATION.pptx

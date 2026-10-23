# COLA-TUNE — YoungCola Edition

Auto-Tune VST3 für Underground Rap. Gebaut mit JUCE 8 (C++).

## Features
- Pitch-Erkennung (YIN) + pitch-synchroner Shifter -> sauberer Hard-Tune ohne Phasen-Artefakte
- **Key + Scale** (Chromatic, Major, Minor, Harmonic Minor, Pent. Minor/Major, Blues, Dorian, Phrygian)
- **12 klickbare Piano-Tasten**: einzelne Noten an/aus (eigene Skalen)
- **Retune Speed** (1 ms = harter T-Pain/Trap-Effekt, 100+ ms = natürlich), Amount, Mix, Detune (±50 ct), Output
- Live-Pitch-Graph (erkannt vs. korrigiert), Notenanzeige mit Cent-Abweichung
- Animierte Oberfläche: Cola-Bubbles, Glitch-Logo, Spektrum-Ring, Foto reagiert auf den Pegel

## Auf GitHub bauen (kein lokales Setup nötig)
1. Neues Repo auf github.com erstellen, diesen Ordner hochladen (inkl. `.github/`).
2. Tab **Actions** -> "Build COLA-TUNE" läuft automatisch bei jedem Push.
3. Fertige `.vst3` unter dem Run -> **Artifacts** (Windows + macOS) herunterladen.
4. Release: `git tag v1.0.0 && git push --tags` -> Zip-Dateien landen automatisch unter **Releases**.

## Installieren
- Windows: `COLA-TUNE.vst3` nach `C:\Program Files\Common Files\VST3\`
- macOS: nach `/Library/Audio/Plug-Ins/VST3/` (ggf. `xattr -cr COLA-TUNE.vst3`, da unsigniert)

## Lokal bauen
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ColaTune_VST3
```

## Hinweis
Das Foto in `Assets/youngcola.png` ist im Plugin eingebettet. Tausche die Datei einfach aus, um das Bild zu ändern.
Stelle sicher, dass YoungCola der Veröffentlichung seines Bildes zugestimmt hat.

# AGENTS — bu klasörde çalışan yapay zekâ için kısa sözleşme

Sistem: bir Flash (AS3) oyununun 3DS'e satır satır C çevirisi. Ayrıntı: `PORTING_GUIDE.md` (önce oku).

## Okuma sırası
1. `PORTING_GUIDE.md`: nasıl çevrilir, nelere dikkat edilir, doğrulama merdiveni.
2. `CLAUDE.md`: bu oyunun (Kingdom) tur tur geçmişi, çözülmüş sorunlar, açık işler.
3. `PARITY_REPORT.md`: güncel parite durumu.

## Değişmez kurallar
1. `game/` ve `flixel/` yalnızca `.as` kaynağı ve **SWF bayt kodu** ile birebir olur. SWF ≠ kaynak ise SWF kazanır
   (`python tools/port.py abc <Sınıf> <metot>`), yorumuna `SWF FIX:` yaz.
2. 3D, GPU, hız, HUD, süs: yalnızca `flash/` ve `platform/`. `python tools/port.py check --only boundary` kırmızıysa dur.
3. Her değişiklikten sonra `python tools/port.py check --quick`. Çıkış kodu 1 ise bırakma, düzelt.
4. Çalıştırmadığın testi "geçti" diye yazma. Doğrulayamadığını son raporda "doğrulanmadı" olarak yaz.
5. Aynı emülatörü (Azahar) başka bir iş kullanıyor olabilir. Onu kapatma; `port.py azahar` zaten başkasının
   süreçlerine dokunmaz. Başka bir çalışmanın klasörüne (`../classic/` vb.) bakma ve dokunma.
6. Bash araçları ters bölüyü yutar: C dizelerini (`"\n"`) yalnızca Edit/Write ile düzenle.
7. Emin değilsen tahmin etme: neyi bilmediğini ve neye baktığını yazıp dur.

## En sık kullanılan komutlar
```
python tools/port.py doctor          # ortam hazır mı
python tools/port.py check --quick   # hızlı doğrulama (boundary, parity_ops, swf-diff, oracles)
python tools/port.py check           # tam doğrulama (derleme, regresyon, statik dahil)
python tools/port.py abc Wall needsWork
python tools/port.py build-3ds && python tools/port.py deploy
python tools/port.py azahar "seed 0.5; freeze 600; progress D2 A7 X1000 B2 P0 F1 H2 W000011 C0 G4 S00" --out shot.png
```

## Son rapor biçimi
```
YAPILAN: <gerçekleşen değişiklikler, dosya adlarıyla>
DOĞRULAMA: <çalıştırdığın komutlar ve gerçek sonuçlar>
DOĞRULANMAYAN: <emülatörde/donanımda/Ruffle'da bakılmayanlar>
AÇIK: <kalan iş>
```

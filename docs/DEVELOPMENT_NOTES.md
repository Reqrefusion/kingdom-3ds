> Internal working notes of the development (in Turkish). They mention paths of the original workspace (`../source`, `../work`, `captures/`); the repository itself needs none of them.

# Kingdom → Nintendo 3DS (satır satır C çevirisi)

Bu klasör, noio/kingdom'ın (commit `56d4e622461b72df8fa8f69d09fbd63dab7a1346`,
AS3 + Flixel 2.55) 3DS'e satır satır C çevirisi. Ayrıntılar: `README.md`
(mimari, kurallar, testler), `FEATURE_MATRIX.md` (orijinal / eski kod / yeni
kod tablosu), `STATIC_COMPARISON.md`, Codex'in incelemeleri
`CODEX_TRANSFER_REPORT.md`, `CODEX_FULL_REUSE_SCAN.md`,
`CODEX_WATER_REUSE_NOTES.md` (salt okuma inceleme; çalıştırma iddiası yok).

Orijinal AS3 kaynağı:
`../work/kingdom-source/56d4e622461b72df8fa8f69d09fbd63dab7a1346/kingdom-56d4e622461b72df8fa8f69d09fbd63dab7a1346`
(aşağıda `$SRC`). Eski native port: `../source`, testleri `../tests`.

## Kingdom: Classic portu (8 Kasım 2026)

- Unity 5.3.4f1 `Assembly-CSharp.dll` decompile edildi; davranış
  spesifikasyonu ve serileştirilmiş prefab/ScriptableObject değerleri
  `../classic_ref/` altında. Envanter:
  `../classic_ref/CLASSIC_UNITY_INVENTORY.md`; plan:
  `../classic_ref/CLASSIC_PORT_PLAN.md`.
- Yeni port `../classic/` altında **ayrı proje**. `game/`, `flixel/`,
  `platform/` değişmez; `translit/flash|flixel` salt okunur kütüphanedir.
- İlk dilim gece savaşı: Director → EnemyManager → Portal → Troll/Archer.
  İnce `unity_compat` (`Uc`) katmanı + `Kc*` oyun modülleri; Flixel benzeri
  çekirdek.
- **M1 tamam (8 Kasım 2026):** `Uc` çekirdeği, Hermite eğri değerlendiricisi,
  veri/oracle üreteçleri, MSVC host ve C01/C02/C16 testleri hazır. C01 7.728
  örnekte 0 hata; C02 1.968 gün sayımında 0 hata; C16 aynı seed'de 16.437
  baytlık trace birebir. 60 kare smoke iki koşuda aynı SHA-256. Ayrıntı:
  `../classic/README.md`; komutlar `../classic_ref/CLASSIC_PORT_PLAN.md` §7.
- **M2 tamam (8 Kasım 2026):** `Director` + `EnemyManager` + `Portal` +
  `Kingdom` stub'ı; C03–C06 ve erken C14 geçiyor. C03 30 günde 337 olayı
  (PlayTrack dahil) Python `oracle_m2.py` ile birebir karşılaştırır; gün
  30/saat 4,098, 757 spawn. `classic_m2` aynı seed'de kare ve olay
  CSV'lerini birebir üretir.
- **M3 tamam (8 Kasım 2026):** `Mover`, `Damageable`, `Troll`, `Wall`,
  `Player` stub'ı; C07–C09 ve C13 geçiyor. Toplam **3.232 kontrol, 0 hata**.
  Troll charge'ı duvarda 25→24 hasar, bounce (-3,10, 1,03); knight bounce
  (-7,08, 1,01); tough troll 4 vuruş; `LimitSpeed(1,0)` hızı sınırlıyor.
  Portalın doğurduğu troller artık FSM ile güncelleniyor.
- **M4 tamam (8 Kasım 2026):** `Archer` + `Arrow` + hedef seçimi; C10–C12
  geçiyor. Toplam **3.261 kontrol, 0 hata**. 2 deneme; atışlar 0,400/2,117 s;
  kuvvet 8,50→9,06; 5 birimde ok 31. karede isabet; Flee'de atış yok.
  `extract_assets.py` 118 ses, 931 sprite, 91 animasyon, 329 doku ve 25
  controller manifesti üretiyor.
- **M5 tamam (8 Kasım 2026):** Kingdom sınırları, dalga yenilgisi ve `KCSG`
  v1 kayıt. C14–C15 geçiyor; 600 kare kayıt + 600 kare devam kayıtsız
  trace ile **0 uyuşmazlık**. Toplam **3.270 kontrol, 0 hata**. Save v1
  duvarları/Troll iç timer'larını kapsamıyor (belgeli).
- **M6 tamam (8 Kasım 2026):** 74 düşman + 200 ok stres koşucusu; C17.
  Host'ta ortalama **0,0102 ms/kare**, en kötü **0,68 ms** (bütçe 2 ms).
  Normal portal ritmi de 1800 karede 74'e ulaşıyor. Toplam **3.350 kontrol,
  0 hata**.
- **M8 ilk dilim (8 Kasım 2026):** `KcGame` savaş döngüsü: 4 duvar, 4 okçu,
  2 portal, Regular gün 10 dalgası; `KcEconomy`, `KcShop`, işçiyle duvar
  inşası, kale yükseltmesi, işçi/çiftçi/şövalye ve gün döngüsü. `battle`
  dalgayı **2.186. karede** temizliyor: 31 ölüm, 60 ok, 30 coin, 20 çiftçi
  coin'i, 17 şövalye vuruşu. C19–C28 eklendi; toplam **3.436 kontrol, 0 hata**.
- **Görsel (8 Kasım 2026):** PC `render_battle` ve 3DS `kingdom_classic.3dsx`
  aynı KcGame + gerçek sprite atlasını kullanıyor. Atlas 512×512 tiled/linear
  **lossless**; 3DS Azahar’da 58 fps; troll/archer/horse AnimationClip kareleri
  (5 klip, 3 horse event). 3DS’te H (D-pad Right) oyuncu x=0→6,96; mağaza/
  kale/duvar için X/Y/B tuş kodu bağlı. `UNITY_FEATURES.md` sprite/Animator/
  Physics2D notlarını toplar. Kalan: tam clip zamanlaması, ses/particle.
- **3DS/Azahar:** artık park değil; ilk görsel doğrulama yapıldı, tam oyun
  akışı için çalışma sürüyor.
- Classic dalgaları portal kuyruğuyla 0.2–0.4 s aralıkla gelir; 74 trol aynı
  karede doğmaz (bu, Flash translit'in `phaseNightTen` senaryosu).
- Kayıt: `BinaryFormatter` taşınmaz; sürümlü `classic_save.bin`.

## GitHub deposu (12. tur, kullanıcı isteği): `C:\Users\<you>\Documents\GitHub\kingdom-3ds`
Kullanıcı oyunu GitHub'da yayınlayacak (remote `Reqrefusion/kingdom-3ds`, boş repo, `lume-3ds`/`samorost1-3ds` örnek).
- **Varlıklar `.3dsx` içinde (RomFS)**: `build_3ds.py --romfs translit_assets` (3dsxtool); `main.c` `romfsInit()` ile `romfs:/`
  okur (SD `kingdom_translit/` yedek). `.3dsx` ≈ 42 MB. Azahar'da SD'deki varlıklar silinerek denendi: durum izi
  birebir aynı, ses hatasız. `deploy_sd.py` artık yalnızca `.3dsx` kopyalar (`--with-sd-data`, `--clean-old-data`).
- **Varlıklar orijinal GitHub deposundan**: `tools/fetch_assets.py` `noio/kingdom` commit `56d4e62…`'yi indirir, 73 dosyanın
  SHA-256'sını `assets.sha256.json` ile doğrular, `convert_assets.py --audio` çalıştırır (ffmpeg + Pillow gerekir), orijinal
  `LICENSE.txt`'yi `translit_assets/`a koyar (lisans her kopyada bulunmalı). GitHub'dan indirilen çıktı mevcut
  varlıklarla birebir aynı çıktı. Dönüştürücü artık LF yazar (Windows CRLF yüzünden `game/level_fields.c`,
  `WeatherPresets.*` "farklı" görünüyordu).
- **Bağımsız derleme**: eski prototipe (`../source`) bağımlılık `vendor/engine/` altına 5 küçük dosya olarak alındı
  (`numeric.h`, `jobs_state.c/.h`, `jobs_backend.h`, `3ds/math.h`); `run_native_oracles.py` eski `tests/check.py` yoksa
  yalnızca doğrudan kontrolleri çalıştırır (`direct_only`).
- **Tek komut**: `python tools/build_release.py --sdk C:/devkitPro` (fetch → build → `release/<product>.3dsx`);
  `port.json` `product`/`title`/`data_root` (repo sürümünde `kingdom`, `sdmc:/3ds/kingdom-3ds`).
- **Repo üretimi**: `python tools/export_repo.py --dest <repo>` (`tools/repo_template/` + kaynak klasörleri + docs;
  `.git`'e dokunmaz, yalnızca kendi yazdığı dosyaları siler, commit/push YAPMAZ, yerel kullanıcı adını belge yollarından
  siler). Repo içinde temiz derleme (`build_release.py`) ve `port.py check` geçti: regresyon aynı, 161 doğrudan test,
  statik %88,0; repo çıktısı Azahar'da çalıştı (900. kare durum izi aynı). CI (`.github/workflows/build-3dsx.yml`)
  örnek repolardaki desenle yazıldı ama GitHub'da ÇALIŞTIRILMADI.
- **Lisans uyarısı**: orijinal Kingdom lisansı özel (ticari olmayan, "mobil cihaz" yayını kısıtı, lisans her kopyada);
  türev çeviri aynı lisansa tabi, bu yüzden repo `LICENSE` = orijinal metin (diğer repolardaki GPL-3.0 UYGUN DEĞİL).
  `vendor/engine/` dosyaları kullanıcının Lume motorundan; lisansı kullanıcı onaylamalı.
- Azahar çakışması: diğer çalışma Azahar'ı kullanırken `azahar_autotest.ps1` durur; artık `autotest.txt`'yi geride
  bırakmaz (abort yolunda dosya kalıp oyunu donuk autotest moduna sokuyordu, bir kez oldu, silindi).

## 3D su (12. tur, kullanıcı: "su yansıması en göz çeken efekt, 3D'si de harika olmalı")
Ölçülen derinlik profili (host `--gpu 1 --stereo 5`, sol-sağ blok eşleştirme): gökyüzü/ağaç +3..+5, zemin 0, su
satırları waterline'da 0 → ekran altında −3..−4, ön sis −2/−4/−5. Fiziksel not: aynada yansıma, nesnenin KENDİ
derinliğinde görünür (yansıma noktaları nesnenin 3B konumunun hemen altındadır); su yüzeyini öne doğru eğmek (kullanıcının
istediği "arkadan öne") yansımayı katı biçimde eğer, katmanlar arası göreli derinlik korunur. Bu yüzden menüye
**"3D water: mirror / soft tilt / tilt"** (varsayılan tilt) eklendi: `Settings.water3d` (settings.cfg `water N`),
`Gpu_waterSlope` = 0 / 2 / 4 px, platform her karede ayarlar (`main.c`), host `--water-slope N`. Mirror = ek kayma yok
(yansıma nesnenin derinliğinde), Soft = 2 px, Tilt = 4 px. Menü artık 7 satır (aralık 20 px). Gerçek 3B ekranda gözle
karşılaştırılmadı: kullanıcı menüden seçip beğendiğini bırakabilir. Denenip bırakılan fikirler: ripple desenini düzleme
bağlama (sağ gözde harita örneğini dx kadar kaydırma, fark ≤ 1 px, karmaşıklık yüksek), 5 px eğim (yüzey kenar kapsamı
gerektirir). İzleme araçları: `captures/ana/anaglyph_*.png` (kırmızı-camgöbeği), disparite haritası betiği bu turdaki
komutlarla yeniden üretilir.

## Sistemi başka oyunlar için kullanma (12. tur, kullanıcı isteği)
Kullanıcı bu sistemi ileride başka Flash oyunlarını port etmek için, **daha düşük seviyeli yapay zekâlarla**
kullanacak. Bunun için: `PORTING_GUIDE.md` (AS3→C çeviri sözlüğü, gerçek örnek, dikkat edilecekler, doğrulama
merdiveni, yeni oyun iş akışı), `AGENTS.md` (kısa sözleşme), `tools/port.py` (tek giriş: `doctor`, `check [--quick]`,
`swf-diff`, `abc`, `build-3ds`, `deploy`, `azahar`, `ruffle`), `port.json` (proje ayarları),
`tools/known_diffs.json` (gerekçeyle kabul edilmiş SWF/kaynak/C farkları: yenisi = `check` kırmızı).
`python tools/port.py check` bu turda 7/7 PASS (sınır, derleme, regresyon, 167 doğrudan test, statik %88,0,
parity_ops, swf-diff). Bilerek bozulan bir karşılaştırma (`Citizen.c` `>` → `>=`) `parity_ops` adımında yakalandı.
Çekim/png çıktıları `captures/` altında (eski `azahar_*.png` kanıtları da oraya taşındı, silinmedi).

## Kullanıcı (Furkan) ne istiyor

- Türkçe konuş. Gereksiz soru sorma, onay/iş isteme; kendin derle, ölç, düzelt.
- **Neden iki kod var:** eski native kod (`../source`) çalışıyordu ve hızlıydı
  ama oyun mantığında sorunlar vardı. Bu yeni çeviri, kaynakla eşleşmeyi
  kolaylaştırmak için var. Oyun mantığı buradan, platform/GPU/yansıma
  kazanımları eski koddan alınır.
- Öncelik: **oynanabilirlik (fps)** + **orijinal gibi görünmesi** + **oyun
  mantığının orijinalle aynı olması**.
- **Statik eşleşme** yüksek kalsın: `game/` ve `flixel/` ifadelerini
  değiştirme. Hızlandırma/uyarlama `flash/`, `platform/`, `as3/` altında ya da
  "GPU adapter" / "ADAPTATION" etiketli ek satırlarla yapılır.
- İsteğe bağlı özellikler (gün ortası kayıt, dokunarak hedefleme, alt kamera
  ayarı) sonraya bırakılabilir (kullanıcı Codex'e böyle söyledi).

## Araç tuzakları (önemli, zaman kaybettirdi)

- **Bash aracı ters bölüleri yutuyor:** heredoc/`python -c`/`perl`/`sed`
  komutlarında `\\` → `\` oluyor, `"\n"` gerçek satır sonuna dönüşüyor.
  C string'lerinde kaçış (`"\\d"`, `"\r\n"`, `'\n'`) içeren düzenlemeleri
  **yalnızca Edit/Write araçlarıyla** yap. Bash ile yaptıysan `od -c` ile kontrol et.
- Python çıktısı cp1254 konsolda patlar: `PYTHONIOENCODING=utf-8` kullan.
- gcc/make/WSL/MSYS **yok**. Var olanlar: devkitARM (`C:/devkitPro`), Python 3.14
  (Pillow, numpy), MSVC `C:\FreeCADDev\VS\BuildTools2022` (vcvars64.bat;
  cl `14.44.35207`). Küçük host C testi örneği:
  `cmd /c "call C:\FreeCADDev\VS\BuildTools2022\VC\Auxiliary\Build\vcvars64.bat >nul && cl /nologo /std:c11 /I<translit> t.c"`
  (`as3/as3.h` yardımcıları böyle test edildi).
- Azahar computer-use izin listesinde yok; pencere görüntüsü PowerShell
  PrintWindow ile alınır (`tools/azahar_autotest.ps1`). Kullanıcının diğer
  pencerelerini yakalama: SetForegroundWindow/CopyFromScreen kullanma.

## Derleme, kurulum, ölçüm

```
python tools/build_3ds.py --sdk C:/devkitPro      # out/kingdom_translit.3dsx
python tools/deploy_sd.py --sd-root "$APPDATA/Azahar/sdmc"
```
- Emülatör: `C:\Users\<you>\Downloads\azahar-windows-msvc-2126.2\azahar.exe <3dsx>`
  komut satırından açılır; oyun kayıttan doğrudan PlayState'e girer.
- `%APPDATA%\Azahar\sdmc\3ds\kingdom_translit\runtime.log`: her 30 karede
  `fps / game ms / draw ms / gpu ms / up KB / cmds`; her 300 karede özet ve
  `uploads per 300 frames:` (yeniden yüklenen bitmap'ler, `n` = dar doku).
- Hızlı fps ölçümü: Azahar'ı aç, 60 sn bekle, kapat, log'u oku
  (`Stop-Process` ile kapatmak sorun değil).

## Görsel / davranış doğrulaması: autotest (bu turda eklendi)

`platform/3ds/main.c`: SD'de `kingdom_translit/autotest.txt` varsa sabit
tohum, sabit Flixel adımı ve betikli girdiyle oynar, kayıt okumaz/yazmaz,
`freeze F` karesinde durur ve log'a host `trace.csv` sütunlarında
`trace F,...` yazar. Satırlar: `seed S`, `freeze F`, `cpu 0|1` (1 = CPU
compositor, orijinal çeviriye birebir, ~3,7 fps), `progress <kayıt metni>`
(o noktadan `new PlayState(kayıt)`), host betik satırları.

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools/azahar_autotest.ps1 -ScriptFile tests/baseline/input.txt -Freeze 900 -Wait 45 -Out shot.png
python tools/compare_shot.py shot.png tests/baseline/frame_00900.png cmp.png
# gece: GPU ile CPU yolunu karşılaştır (CPU için -Wait 230)
powershell ... -Script "seed 0.5`nfreeze 600`ncpu 1`nprogress D1 A4 X1920 B2 P2 F2 H2 W000011 C1 S00 G4" -Wait 230 -Out cpu.png
```
Betik Azahar düzenini geçici olarak üst/alt yapar, sonra `qt-config.ini`'yi
geri yükler ve `autotest.txt`'yi siler. Ölçülen değerler:
- baseline 900. kare: `trace` satırı `tests/baseline/trace.csv` ile birebir aynı;
  görüntü farkı 3,8/255.
- gece (yukarıdaki progress, 600. kare): GPU–CPU farkı 55,6 → 8,8/255.

## Durum (6 Kasım 2026, dördüncü tur: Sonnet — sonu)

Yapılanlar:
- **Açılış akışı** (`platform/3ds/main.c`): artık otomatik resume yok; orijinal
  MenuState (başlık) açılır. A / dokunma = yeni oyun (`new PlayState()`,
  öğretici: "Throw some coins [DOWN] near them." + kamera köydeki dilencilere
  kayar, sonra yay/orak/duvar tavsiyeleri). Y = kayıttan devam
  (`new PlayState(kayıt)`, orijinalin taç kaybı yolu; öğretici atlanır).
  Alt ekranda ipucu satırı. Kanıt: `tests/ride_village.txt` ile autotest.
- **Codex'in küçük hataları**: `session.c` stereo `n >= 2`; `replace_file`
  (tmp → bak → rename, başarısızsa geri alma) kayıt ve ayarlarda;
  `Session_savedProgress` bozuk dosyayı reddeder, `.bak`'a döner; `new_game`
  `.bak`'ı da siler; `flash/gpu.c` `push()` realloc kontrolü.
- **Host derlemesi MSVC ile**: `python tools/build_host.py` (vswhere → vcvars64,
  libpng yok: `platform/host/main.c` kendi PNG yazıcısını kullanır,
  `KINGDOM_HOST_LIBPNG` tanımlıysa libpng). `--scenarios` oracle ikilisini
  derler. **`python tests/regress.py --exe ./kingdom_host.exe` → OK**: CPU modu
  baseline ile *piksel piksel aynı*, `trace.csv` 1801 satırın hepsi aynı
  (MSVC, Linux/gcc baseline'ını üretiyor); GPU yolu 0–4,5/255 (sınır 6).
  **`python tests/run_native_oracles.py` (varsayılan msvc)**: Flixel-exact 4/6,
  one-callback 6/6; iki fark belgelenmiş çift quadtree geri çağrısı.
- Statik eşleşme: çekirdek oyun sınıflarının (Citizen, Troll, Player, Wall,
  Shop, Castle, Bunny, Coin, Farmland, Weather, Water, Sky, PlayState)
  eşleşmeyen satırları tek tek okundu: yalnızca alan başlatmaları, `play()`
  varsayılan argümanı, log metni, Mochi/debug. Mantık sapması yok.
  Karşılığı olmayan 161 kaynak fonksiyonunda oyun sınıfı yok (ColorMatrix
  kullanılmayan işlemler, FlxTilemap yol bulma/`overlapsWithCallback` vb.).
  Zemin çarpışması orijinalde de tilemap değil düz `collider` sprite'ı
  (PlayState.as ~356), bu yüzden `separateX/Y` tilemap dalları gerekmiyor.
- Zaman adımlaması `FlxGame.onEnterFrame` ile aynı (accumulator, `_step`
  tamsayı ms); kontroller LEFT/RIGHT/DOWN(justPressed) + oyun bitince fare.
- Görsel: gece parlaklık (blend alfa) ve flaşın hava matrisinden geçmesi
  düzeltildi; 8 fazın hepsinde GPU–CPU farkı ~3/255 (bkz. B).
- Hız: Azahar'da 60 sn ortalama gündüz 57,7 fps, gece 55,5 fps (alt ekran
  yansıması dahil). Önceki yavaşlama alt ekran yansımasının ilk hâliydi.
  Kalan: her karede 74x11 (Utils.gradientOverlay `tempBitmap`) yeni doku
  yükleniyor, gece 25 KB/kare; doku havuzu denenebilir (kazanç ~0,5 ms).
- Savaş yükü (2026-11-08, iki koşu aynı trace): `D10 A43 X1920 B2 P1 F6 H7
  W140041 C2 S02 G0` = 16 NPC + 74 trol (4 patron, 20 güçlü, 10 zıplayan,
  40 sıradan). Dalga öncesi 50,7–54,1 fps; 74 trolün tek karede doğuşu
  **2,71–2,73 s** (30 karelik pencere 8,8 fps); hemen sonrası 34,5–36,0 fps;
  oturmuş savaş 40,3–42,7 fps (300 kare özeti). Darboğaz GPU değil: game
  update 8,1 → 11,5 ms, draw 5,1–5,8 ms, gpu 1,7 ms, upload 3–25 KB.
  Soğuk açılış/kayıt geri yükleme ayrı: 4,8 fps, tek kare 5,2 s.


## Fonksiyon paritesi turu (5. tur)
- Araç düzeltmeleri (`tools/side_by_side.py`): aşırı yüklü varsayılan argümanlar
  (`play(name,false)` vb.) ve aynı sınıf içi statik çağrı niteleyicisi doğru
  ele alınıyor: 88,5 → 89,7 (kod değişmeden). Bash araç tuzağı yüzünden
  `` regex'i bir kez 0x08 karakterine dönüşmüştü; Python kaynağında
  `chr(8)` taraması yap.
- Orijinal ActionScript'ten çevrilen, oyunun hiç çağırmadığı fonksiyonlar:
  `Utils` (5), `FlxU` (10), `ColorMatrix` (43). Bazı ayrıntılar:
  `FlxU.formatMoney(1234567.89)` kaynak `.88` verir (IEEE); `ColorMatrix.YIQ2RGB`
  kaynağında `1, ,-0.0000…` fazladan virgülü NaN + kayma üretir, aynen taşındı.
  Her biri `tests/translit_scenarios.c` içinde kontrol ediliyor.
- 6. tur: Flixel'in oyunca çağrılmayan üyeleri de çevrildi: FlxRect/FlxPoint,
  FlxCamera (destroy artık gerçekten çağrılıyor: `FlxG.resetCameras`),
  FlxText özellikleri, FlxSprite.pixelsOverlapPoint/drawLine, FlxGroup.setAll,
  FlxBasic.toString, FlxAnim/FlxTilemapBuffer.destroy, FlxU yansıma yardımcıları
  (`game/class_table.c` sınıf tablosu), FlxG (framerate, plugin'ler, shuffle,
  resetState/resetGame...), FlxTilemap (veri erişimi, setTile, overlaps*, CSV).
  Kaynak hatası aynen taşındı: FlxTilemap.overlaps/overlapsAt içindeki tanımsız
  `length` (döngü hiç çalışmaz, FlxObject.overlaps'taki gibi).
  Statik: 4125/4684 (%88,1), 523 fonksiyon (yeni fonksiyonlar paydayı büyüttü).
- Karşılığı olmayan 25 kaynak fonksiyonu kaldı; her biri bütün bir alt sistem
  ister ve oyunca hiç çağrılmaz: FlxPath (FlxObject.followPath/advancePath/
  stopFollowingPath/updatePathMotion/drawDebug, FlxTilemap.findPath/walkPath/
  simplifyPath/raySimplifyPath/ray/computePathDistance), FlxReplay
  (FlxG.record/load/reloadReplay, stop*), Mochi (GameOverState), FlxInputText
  (PlayState.setWeatherFromInput), FlxSound.loadStream/gotID3 (+FlxG.stream),
  FlxG.fullscreen. Gerekirse bir sonraki adım: FlxPath (279 satır) ve onunla
  11 fonksiyon.
- Eski belgeler (`../docs/PARITY_WORKLIST.md`) tarandı: V009 (para pencereleri),
  V016 (kale), V026 (öğretici metin kuyruğu), V028 (kamera) kartlarının
  beklentileri çeviride kontrol olarak yazıldı, hepsi geçiyor; V027/V031-V038
  kalan kartlar ses/kayıt/stereo (platform) ağırlıklı.


## 7. tur: Ruffle ile orijinal karşılaştırma, ses, 1 px kayma, NPC hattı
- **Orijinal SWF**: `../work/original_swf/` (Flashpoint veri paketi, SHA-256
  87795a69…, kullanıcı onayıyla indirildi; sürüm v1.1.10 Kickstarter yapısı).
  `tools/ruffle_shot.ps1` Ruffle'da çalıştırır, tıklama/tuş gönderir, pencere
  görüntüsü alır (oyun alanı 864x480, üstte 24 px menü). Karşılaştırma kareleri:
  `tests/ruffle_compare.txt` aynı betiği host/Azahar'a verir.
- **Bulundu ve düzeltildi (görüntü)**: `Gpu_copyPixels` kesirli hedef noktayı
  kaydediyordu, CPU `copyPixels` aşağı yuvarlıyor (floor) → kamera kaydırmasının
  kesri ≥ 0,5 iken tilemap/iskele/copyPixels sprite'ları 1 px kayıyordu.
  Host GPU–CPU farkı 1,5 → 0,25; `regress.py` GPU eşiği 6 → 1,5 (0,2–0,7).
  Azahar–CPU 2,9→1,9 ve 4,3→2,6. Önceki turlardaki "normal 1,5–4,5" fark bu
  hataydı.
- **Ses**: `ndspInit` 0xD880A7FA = `sdmc:/3ds/dspfirm.cdc` yok. Azahar HLE
  için `tools/deploy_sd.py` sahte 32 KB dosya oluşturur (yalnızca Azahar/Citra
  SD'sine; gerçek konsol kendi DSP dökümünü ister). Ses yoksa alt ekranda
  "NO SOUND (dspfirm.cdc)" yazar; `runtime.log` ndspInit sonucunu ve çalma
  sayaçlarını içerir. Çalma çağrıları (ortam döngüleri) yapılıyor; sesi
  duyma doğrulaması kullanıcıda.
- **Buğday (Reed)**: kaynakla aynı, Azahar'da animasyon çalışıyor (60 karede
  361 piksel değişir). **Yansıma**: Ruffle orijinaline göre ortalama/std/eğim
  istatistikleri aynı düzeyde (std 5,4–6, eğim 2,0–2,5).
- **NPC zekası**: `verify_npc_pipeline` (translit_scenarios.c): dilenciye DOWN ile
  para → Poor vatandaş → stoklu dükkâna yürür → çiftçi (tarla hasadı, para
  ödülü) / yalnız yay stoklu → avcı (menzildeki tavşanı vurur). Hepsi PASS.
- **Gece karşılaştırması (8. tur)**: Ruffle'da tıklamadan sonra 5/85/98/113 sn
  (`tools/ruffle_shot.ps1 -Plan "wait 6;click;wait 5;shot s1;wait 80;shot n1;wait 10;shot n2;wait 14;shot n3"`),
  host'ta aynı anlar (`tests/night_click.txt`, `--frames 300/5300/5900/6780`).
  Sonuç: sis başlangıcı neredeyse piksel aynı; akşam gökyüzü, gece mavisi, ay,
  yansıma, "They will noodle..." metni ve kamera troll'e kayması aynı;
  fark yalnızca rastgele troll konumu (Flash `Math.random` aynı değil) ve
  "Night comes, be careful." metni host'ta ~1-2 sn geç (Ruffle saat farkı olabilir,
  doğrulanmadı). Daybreak jetonu (`PlayState.daybreak`) kaynakta da oyuncunun
  ayağına düşer. Ruffle çekerken **fareye/klavyeye dokunma, pencereyi kapatma**;
  iki betik aynı anda çalışırsa biri diğerinin Ruffle'ını öldürür (`finally`).
- **Ses mantığı testi (`verify_sound`, 18 kontrol, hepsi PASS)**:
  `tests/gen_sound_expected.py` orijinal `PlayState.as`'tan faz tablosunu
  (45 satır + 5'lik sonsuz gece döngüsü: hava, süre, tween, callback var mı,
  müzik) `tests/sound_expected.h`'ye üretir; test çeviriyi bununla karşılaştırır,
  sonra gerçek `PlayState_nextPhase` ile tabloyu ve döngüyü yürür (müzik
  başlıyor/önceki duruyor, tween süresi varsayılan 0,7 x süre, duraklatılmışsa
  hiçbir şey olmaz), ağız ortam sesleri (cicada/owls/birds yaşam döngüsü,
  gün saatine göre hacim denklemleri 16 noktada, 0..1 kıskaç), yakınlık
  sesi (yarım ekran = yarı hacim, pan) ve tüm ses dosyalarının
  `translit_assets` içinde olması. Mutasyonla doğrulandı: bir müzik adı ve bir
  hacim denklemi bozulunca test FAIL veriyor. Kaynak değişirse betiği yeniden
  çalıştır. `platform/host/media.c` artık `HostMedia_stops()` sayar.
  Test **duyulabilirliği** kanıtlamaz (host sessiz arka uç); Azahar/konsol dinlemesi
  yine kullanıcıda.
- **ORİJİNAL SWF'İ ÇÖZEN ARAÇ (9. tur, ÇOK ÖNEMLİ)**: `tools/abc_dump.py <swf> <Sınıf> <metot>`
  SWF'teki AVM2 bayt kodunu okunur hâle getirir (Flashpoint'te decompiler yok, kendi
  ayrıştırıcımız; bilinmeyen opcode `op_xx` yazar). `tools/swf_vs_source.py <swf> <kaynak dizini>`
  her metodun karşılaştırma yönlerini (<,<=,>,>=), sayısal sabitlerini ve kullandığı adları
  kaynak snapshot'la karşılaştırır. **Kaynak snapshot ≠ SWF olabilir; gerçek davranış SWF.**
  Fark listesi (tüm sınıflar): `Wall.needsWork`, `PlayState.spawnTrolls`, MenuState
  (Kickstarter düğmesi), `saveProgress`/`getProgress`/`sendProgressAnalytics` (analitik).
  Citizen, Troll, Bunny, Farmland, Shop, Coin, Castle, Player metotları karşılaştırma
  yönü/sabit/ad kümesi bakımından kaynakla uyuşuyor (`&&`/`||`, `+`/`-` farkını araç
  görmez; şüphede `abc_dump.py` ile oku). SWF'in `Citizen.pickNewGoal`'ı kaynakla aynı:
  çiftçi yalnızca duvarları hedefler (tarlaya yalnızca rastgele dolaşırken uğrar).
- **NPC inşa/merkezde durma — ÇÖZÜLDÜ (kök neden SWF ile kanıtlandı)**: kaynak snapshot'ta
  `Wall.needsWork` = `(building || health < HEALTH) && t < HURT_COOLDOWN` (YAZIM HATASI);
  SWF'te `t > HURT_COOLDOWN`. `<` ile `t` büyüdükçe iş görünmez → çiftçi duvarı bitirmez →
  krallık 200 px'te kalır → herkes merkezde. `game/Wall.c` artık SWF kuralı (açık "SWF FIX"
  yorumu); Codex'in önerdiği `building || (... t < ...)` uyarlaması **uygulanmadı**, gerek yok.
  `PlayState.spawnTrolls` da SWF'teki `while (amount > 0)` (snapshot `while(amount)`).
  `verify_construction` (7 kontrol): eski `t`'li duvarı ve içeride duran çiftçiyi bitirme
  senaryoları, 1 sn bekleme kuralı. Düzeltme çıkarılınca FAIL. `verify_work_dispatch` duvar
  testleri `t = 2` ile (SWF kuralı: hasarlı/inşa duvar 1 sn sonra işlenir). `Citizen.checkWork`
  `exists/alive` bakmaz (SWF'te de), testlerde `empty_group` kullan. `verify_npc_pipeline`
  artık kendi tohumlarıyla başlar (önceki testlerin tükettiği rastgele sayıya bağlıydı).
  `run_native_oracles.py` doğrudan kontrol FAIL ise sıfır olmayan kodla çıkar.
  **Azahar'da doğrulandı**: autotest `progress D2 A7 X1000 B2 P0 F1 H2 W000011 C0 G4 S00` +
  `100 key DOWN down/104 key DOWN up`, 5400. kare: jeton 4→3, x=996 duvarı stage 0→1 ve
  `building=0 h=0`. `runtime.log` artık `walls [x= s= b= h=]` satırı da yazar.
- **KURAL (kullanıcı): 3D ve 3DS'e özel süslemeler oyunun PARİTESİ DIŞINDA kalır.** Stereo 3D, su
  derinliği, katman kenar uzantısı, GPU kaydedici yalnızca `flash/` (adaptör katmanı) ve `platform/`
  altında yaşar; `game/` hiç değişmez, `flixel/`'de yalnızca eskiden beri var olan tek satırlık kancalar
  (`Stereo_dx`/`Stereo_depth`) kalır. `python tools/check_parity_boundary.py` bunu denetler (yeni kanca
  çıkarsa 1 döner). Yeni 3D özelliği eklerken önce buraya bak, `game/`/`flixel/` içine dokunma.
- **3D su derinliği (10. tur)**: su yüzeyi arkadan öne gelen bir düzlem: iç göz kaydırması waterline'da 0,
  ekranın altında `-min(maxShift,4)` px (yakın satırlar sağ gözde daha çok sola). `Gpu_stereoWater`
  (platform her karede su bitmap'ini kaydeder) + `flash/gpu.c water_slope/water_row_dx`: dalgalı şeritler
  satır başına `cmd->dx`, sakin su dx basamaklarına bölünmüş bantlar. Host ölçümü (`--gpu 1 --stereo 5`,
  satır başına sağ-sol kayma): waterline 0 → ekran altı −3 px.
- **Alt ekran HUD (11. tur, kullanıcı isteği; parite dışı, yalnız `platform/3ds`)**: geliştirici istatistik
  yazısı (fps/ms/cmds) artık varsayılan olarak GİZLİ, **L+R basılı tutulunca** görünür (`runtime.log` yine yazar;
  autotest'ler logdan okur). Başlık ipucu (A/Y) ve "NO SOUND" uyarısı kalır. Oyun sırasında: sağ üstte büyük
  kese (`sack.png`, 5x, kare = para sayısına göre `sack_frame`, çerçeve ve sayı YOK) ve sağda para vereceğin
  şeyin resmi: `Player.selectedBuilding` (duvar/dükkân/kale), yoksa 96 px içindeki en yakın dilenci; resim
  karenin saydam olmayan sınırına göre ölçeklenir (alçak duvar tümseği de büyük görünür). Çizim
  `main.c draw_hud` + `gpu.c Gpu3ds_hudImage`; oyun durumunu yalnız okur. Azahar'da doğrulandı (duvar ve kese kareleri).
- **Su ripple detayı (11. tur, kullanıcı: "su efekti orijinalde daha iyiydi")**: GPU yolu DisplacementMapFilter'ı
  `Gpu_waterSeg` piksellik şeritlerle uyguluyordu (16: bloklu, gözle görülür). Artık 8 (New 3DS ve host) / 16
  (Old 3DS; `main.c` `APT_CheckNew3DS` ile seçer); harita doldurma adımı `Gpu_waterSeg / 2`
  (`flash/filters.c`; alt ekrandaki derin su x+8 sütunlarını okur, 4/8/16 hepsi uyumlu). Host ölçümü (CPU =
  orijinal semantik, su bölgesi ortalama fark, kare 300/600/900): 16 → 0,51/1,00/0,59; 8 → 0,29/0,60/0,33;
  4 → 0,18/0,35/0,16 (komut: 794 / 1514 / 2954 en çok). Genel GPU–CPU farkı 0,19-0,62 → 0,11-0,24. Azahar
  (New 3DS): 59,8 fps, çizim 5,4 ms (4,0-4,4 idi), 942 komut. Old 3DS'te ölçülmedi. Eski prototip (`source/render.c
  water_surface`) 12x3 ekran pikseli (≈ 8x2 oyun pikseli) hücreleri ve rüzgârsızda tek çizim kullanıyordu; yeni şerit
  8 px x 1 satır, bundan ince. UYARI: üst ekran varsayılanı "whole frame" = 1,39x (düzensiz piksel katlanması);
  ince su çizgileri 1,5x "fill" modunda (menüde Top screen) daha düzenli görünür.
- **Su: SWF bayt kodu ↔ kaynak ↔ C karşılaştırması (11. tur, kullanıcı isteği)**: `Water` kurucusu
  (`DisplacementMapFilter(map, zero, 1, 2, 256, 256, COLOR, baseColor, 0.5)`, `ColorTransform(...,alpha 0.3)`),
  `update` (`currentBase`, `rippleScale = int(wind*25)`, `ColorTransform(xs/128, ys/128, 1, 1, 128-xs+100*xs/128, ...)`),
  `draw` (`perlinNoise(32,4,1,12312,false,false,1|2,true,[offset])` yalnız `timer > 0.1`; `offset.y += 1/5`, `offset.x =
  scroll.x*1.5`; `fillRect(currentBase)`; `gradientOverlay([0x00000000,0x66000000], 90, 4)`; ters çevirip
  `draw(camera.buffer, matrix, transform)`; ışık döngüsü ±64, alfa `darkness*0.8*min(1,wind*10)`, `stamp(x - w/2 + 4,
  (y - l.y)*0.3)`; `applyFilter`) ve `Utils.gradientOverlay` SWF'te kaynak ve C ile AYNI; sabitler SWF'te
  `NOISE_BIAS = 100`, `WIND_RIPPLE_MULTIPLIER = 25`. Yani su ÇİZİM YÖNTEMİ (oyun kodu) bayt kodu düzeyinde eşit.
  Karşılaştırılamayan: Flash'ın yerleşik (native) işlemleri (`perlinNoise`, `DisplacementMapFilter`,
  `beginGradientFill`, `BitmapData.draw`): bunlar ABC'de yok, biz yazdık (`flash/filters.c`), yalnız Ruffle
  istatistikleriyle kıyaslandı. `tools/abc_dump.py` artık statik metotları da bulur ve `<swf> <Sınıf> --consts` ile
  sınıf sabitlerini yazar.
- **`classic/` BAŞKA BİR ÇALIŞMA (Unity Kingdom Classic portu, kendi AGENTS.md'si var)**: kullanıcı "bakma" dedi;
  oradaki dosyaları okuma/değiştirme, Azahar'ı onunla paylaşıyoruz. `tools/azahar_autotest.ps1` artık yalnızca
  kendi başlattığı Azahar'ı kapatır/yakalar, başka Azahar çalışıyorsa hata verip durur. Onun `qt-config.ini`
  değişikliği (3D modu/yerleşim) çakışabilir: çekimleri başka iş Azahar kullanmıyorken yap.
- **Alt ekran gökyüzü devamı (11. tur)**: `compose_deep` yansımanın son satırını (sahnenin en üst satırı) derin yüzeyin
  geri kalanına tekrarlar (düz yeşil blok yerine gökyüzü rengi). İlk Azahar karesinde sahne gökyüzü rengi sabit gri
  göründü; gece (faz 10) karesi alınamadı (başka çalışmanın Azahar'ı çakıştı). Gerekirse gökyüzü gradyanını yukarı
  doğru uzatma (koyulaşma) eklenebilir.
- **Alt ekran = üst ekranın yansıma devamı (11. tur, kullanıcı: "sığdırma değil, yansıması gibi")**:
  `platform/3ds/gpu.c draw_deep(viewW, s, sceneRows)`: ölçek üst ekranla AYNI (`topScale`, 1,39; eskiden
  sabit 1,5), yatay olarak üst ekranın ortasındaki 320 px görünür (ilk deneme yanlışlıkla 288 px'i 320'ye
  sığdırıyordu, geri alındı); dikeyde alt ekranın ilk satırı üst ekranın son sahne satırının altındaki
  yansıma satırı (`startRow = 160 - (water.y - scroll.y)`), yani yansıma kesintisiz sürer. `DEEP_ROWS` 256
  (dokunun tamamı). Azahar'da doğrulandı (at yansıması üstten alta aynı yer/boyut); `fill` modu (1,5x)
  ve gerçek donanımda gözle denenmedi.
- **3D katmanlar (11. tur)**: arka plan katmanları artık aşağı yuvarlanır: uzak tepe +4, yakın tepe +3, ağaç hattı +2,
  buğday (`Reed`) +1 (eskiden 4/4/3/3); gökyüzü/sis +5, su 0..−4, ön sis −2/−4/−5 (bkz. yukarıdaki maddeler).
- **Autotest tuzağı**: `progress` satırındaki `A<faz>` en az 1 olmalı (`phase = ph - 1`); `A0` fazı −1
  yapar, `PlayState_nextPhase` dizi dışına taşar ve Azahar'da "Undefined Instruction, PC 0x2C" ile çöker
  (pencere siyah, 0 FPS, günlükte fps satırı yok). Çöküşü sembole çevirmek için: Azahar günlüğünde
  `LR` değerini bul, `arm-none-eabi-addr2line -f -C -e build3ds/kingdom_translit.elf <LR-4>`.
  Sisli başlangıç fazı: `A1`.
- **3D sis derinliği (10. tur, kullanıcı: "sis de derinlik için önemli")**: ön plan sis yamaları
  (`Fog`, scrollFactor.x 1,5 / 2,5) kaynaktaki parallax'tan −2 / −5 px alıyordu. Suya eğim verince
  ekranın altında su −3/−4'e iniyor ve aynı yükseklikteki sis sudan *daha arkada* kalıyordu.
  `platform/common/stereo.c depth()`: scrollFactor > 1 olan sprite için kayma, yamanın orta yüksekliğinde
  suyun kaymasından (`flash/gpu.c Gpu_stereoWaterDxAt`) en az 1 px daha öne (daha negatif) alınır,
  −maxShift ile sınırlı. Host ölçümü (frame 300): yama y=106 → −4, y=76 → −2, en hızlı → −5. Uzaktaki
  `Haze` ve `Sky` +maxShift'te kalır (atmosferik arka plan). Hepsi parite dışı.
- **3D (stereo) sağ göz kenar boşluğu — düzeltildi (10. tur, kullanıcı yönlendirmesiyle)**: GPU
  yolunda sağ göz her komutu `cmd->dx` kadar kaydırır. Kenarı kaplayan bir katman kaydırınca o
  kenarda `|dx|` sütun açıkta kalır ve alttaki başka katman görünür (renk dikişi). Boşluğa yalnızca
  ekran boyunda (288 px) katmanlar (gökyüzü/sis türü) neden oluyordu (`TRANSLIT_DUMP=300` ile:
  komut 2 ve 17, dx 5); dünya katmanları (tilemap, ağaç hatları, sprite) zaten kenara kadar gerçek
  içerik çiziyor. İLK ÇÖZÜM YANLIŞTI (son görüntünün kenar sütununu esnetmek gerçek içeriği ezer):
  kaldırıldı. Doğru çözüm: `flash/gpu.c stereo_extend` (+ `stereo_fill`): kenarı kaplayan komut
  kendi kenarında genişletilir; kaynakta yanında gerçek piksel varsa o (dünya katmanı), yoksa
  yalnızca tam ekran boyunda katmanda kendi kenar sütunu esnetilir. Sol gözde (dx 0) uzantı ekran
  dışında kalır, görünmez. Host: `--gpu 1 --stereo 5` `frame_NNNNN_right.png` yazar ve macenta
  başlangıçla açıkta sütunları sayar (alttaki dolgular kapattığı için sayı 0 çıkar; asıl ölçü
  renk dikişidir: sütun-sütun renk farkı sol kenarda 16,6 → 1,3). Azahar: `tools/azahar_autotest.ps1
  ... -Stereo` (render_3d=1 yan yana): sağ gözde ilk sütunlar artık düz şerit değil (önceki yamada 8
  sütun birebir aynıydı), doğal içerik. Gerçek donanımda ve ters yan yana/anaglif modlarında gözle
  denenmedi.
- **Parite raporu**: `PARITY_REPORT.md` (SWF ↔ kaynak ↔ C; yeniden üretme komutları içinde).
  Yeni araç `tools/parity_ops.py <kaynak>`: C fonksiyonu ↔ kaynak metodu karşılaştırma yönü ve
  sabit paritesi (394 fonksiyon, açıklanmamış fark yok). `STATIC_COMPARISON.md` yeniden üretildi:
  4123/4684 (%88,0).
- **NPC davranış taraması (9. tur)**: `tests/npc_probe.c` (`build_host.py --main tests/npc_probe.c`)
  kayıttan devam + 36000 kare, oyuncu hareketsiz: NPC'ler %80-99 yürüyor, geniş dolaşıyor.
  Tarla işi yavaş ama tasarım gereği (8 saniyelik tarla dinlenmesi + 8 adım = hasat, çiftçi
  yalnızca üzerinden geçerken kürek vurur). Oyuncu savunmazsa gece 3'te troll'ler kaleyi
  yıkar (beklenen). Hunter'lar `checkGuard` ile kale nöbet yerlerine çıkıp durur (kaynakta
  da). Başka bir kaynak/SWF farkı bulunmadı.
- **Arkadaki sarı otta kahverengi kare — düzeltildi (9. tur)**: `reed.png` 96x32 = 3
  kare ama `Reed.update` formülü kare **3** üretebiliyor (3.0-3.3 → int 3). Flixel
  calcFrame dikdörtgeni (0,32): görüntü dışı; CPU `copyPixels` kırpıp hiçbir şey
  yapmıyor, yani `framePixels` önceki kareyi koruyor (orijinalde de ot kaybolmaz,
  son kare kalır). GPU yolu `Gpu_drawSprite` ile (0,32,32,32)'yi doku sayfasından
  okuyup komşu bitmap'i (kahverengi) çiziyordu. Düzeltme: `flixel/FlxSprite.c`
  calcFrame (GPU adapter) tamamen dışarıdaki dikdörtgende `_gpuSheetX/Y`'yi eski
  bırakır; `flash/gpu.c Gpu_copyPixels` artık kaynağı bitmap sınırına kırpar
  (copyPixels_impl gibi). `verify_reed_frames` (2 kontrol): düzeltme çıkarılınca FAIL
  (kare 3 → (0,32)). Regress OK (GPU farkı 0,19-0,45). Aynı sınıf hata başka
  3'ten fazla kare indisi üreten sprite'lar için de kapatıldı.
- (Önceki not, bağlam) **NPC "merkezde duruyorlar" şikâyeti**: `Citizen.update`
  (IDLE dalı) ve `pickNewGoal` kaynakla satır satır aynı; `FlxU.srand`/`FlxG.random`
  aynı. Host ve Azahar'da NPC'ler dolaşıyor (kayıttan devam, 1800. kare: 9 NPC
  x=1810..2438; Azahar görüntüsü aynı dağılım). Ölçülen neden: krallık sınırı
  (`kingdomLeft/Right`) yalnızca `stage>0 && !building` duvarlarla genişliyor;
  yeni oyunda iki merkez duvar daha inşa ediliyor, bu yüzden 25 sn sonra
  krallık 200 px (1820-2020) ve herkes castle çevresinde kalıyor; hunter'lar
  kalede nöbet yerlerine (`checkGuard`) çıkıp duruyor. Bu orijinalde de böyle
  görünüyor ama Ruffle'da aynı durumda doğrulanmadı. `verify_wander` (4 kontrol)
  bu davranışı sabitler. Kullanıcıdan hangi durumda (yeni oyun/devam, hangi gün,
  hangi meslek) görüldüğü öğrenilmeli; Ruffle'da aynı gün/duvar durumu için
  karşılaştırma gerekir.
- Kalan: gerçek donanımda ses (dspfirm.cdc dökümü), ışık/meşale sahneleri ve
  savaş için Ruffle yan yana.

## Azahar çökmesi — kök neden bulundu (8 Kasım)

Siyah-ekran/0 FPS çökmesinin nedeni: `progress` resume string'inde `A` alanı
`A0`/eksik olduğunda `phase = -1` oluyor; `nextPhase`'teki `(uint32_t)` cast
(`game/PlayState.c:735`) döngüsel dala düşürüp `PHASES_CYCLE[-1]` → callback
(!) yerine `PHASES_length` (45) okunup 45 adresine sıçrıyor. Kanıt zinciri,
disassembly/adres dökümü ve düzeltme önerileri:
[AZAHAR_CRASH_PHASE_RING_2026_11_07.md](AZAHAR_CRASH_PHASE_RING_2026_11_07.md).
Düzeltme: signed karşılaştırma + negatif faz için kontrollü hata yolu; ayrıca
autotest'in sildiği `autotest.txt` içeriğini runtime.log'a yazma önerisi var.

## İş kuyruğu (kalan; sırayla)

### A. Oracle'ları genişlet (kaynakla davranış kanıtı)
6/21 CSV oracle bağlı + `tests/translit_scenarios.c` içinde **doğrudan kaynak
türevi kontroller** (`translit_checks.txt`, `run_native_oracles.py` basar):
V012 iş dağıtımı (13 kontrol), muhafız (6), dükkan (12), tavşan yaşam döngüsü
(7), troll kurucu/reset/go/retreat/kill/getShot (16) — hepsi PASS. Bulunan bir
kaynak tuhaflığı (testte belgeli): Flixel `addAnimation` ekler, `play` ilk
eşleşeni seçer; büyük/küçük troll türü değişince küçük 'walk' büyüğünü
gölgeler (orijinalde de böyle; çeviri aynısını yapıyor).
Kalan CSV oracle adayları: night hunter patrol, night waves, troll edge-kill
reuse, game-over retry, harvest paylaşımı, V021 combat reset
(`CODEX_TRANSFER_REPORT.md` sonunda 15'lik liste). Yöntem: `../tests/check.py` içindeki `compare_*` fonksiyonunu
oku, beklentileri C'de yaz (kanıt ankrajlarını kaynak .as'tan al), gerçek
PlayState nesneleriyle çalıştır. Oyun ifadelerini değiştirme.
Uzun oynanış: `tests/long_autoplay.txt` (58000 kare gezinti + para bırakma)
host'ta `--frames 60000` çökmeden bitiyor: gün 1→2, trol saldırıları, taç
kaybı, GameOverState (17578. kare); GameOver'da tıklama → MenuState → tıklama
→ yeni PlayState (kaynakla aynı: GameOverState → MenuState).

### B. Faz taraması (GPU–CPU) — kök neden bulundu ve düzeltildi
`python tools/phase_sweep.py` (Azahar'a başka şey dokunmasın; aynı anda iki
tarama başlatma, birbirinin Azahar'ını öldürür). İlk tarama fog/gündüz
fazlarında GPU–CPU farkını 24–27/255 verdi. Neden: orijinalde hava matrisi
(`PlayState.draw` → `ambientTransform.applyFilter`) kamera tamponuna uygulanır,
**sonra** `FlxGame.draw → FlxCamera.drawFX` beyaz flaş/fade'i çizer
(`setProgress` her kayıttan devamda 3 sn beyaz flaş başlatır; ayrıca taç kaybı
fade'i). 3DS yolu matrisi sunum aşamasında *tüm* sahneye (flaş dahil)
uyguluyordu → flaş matristen geçip kararıyordu. Düzeltme: `Gpu_screenFilter`
kamera tamponundaki filtrenin liste indeksini `GpuList.filterAt` olarak işaretler;
`platform/3ds/gpu.c play()` filtreden sonra komut varsa sahneyi TEV ile
`filteredTex`'e süzer ve kalan komutları onun üstüne süzmeden çizer
(`sceneFiltered`); host referans oynatıcı aynısını `Gpu_playSoftwareFiltered`
ile yapar. Sonuç: A1 farkı 27 → 2,8. (Eski gece düzeltmesi: SCREEN/MULTIPLY alfa
denklemi, `blend()`.) Tarama sonuç tablosu aşağıda.
**Faz taraması sonucu (düzeltmeden sonra, GPU–CPU ortalama fark /255):** A1 2,80,
A3 2,97, A5 2,78, A11 2,57, A16 2,76, A21 2,65, A29 2,58, A38 2,62 — hepsi
~3 altında (önceden fog/gündüz fazlarında 24–27). Kalan fark TEV 8 bit
yuvarlaması ve GPU karışım yuvarlamasıdır.
`weathermode 1` (autotest) kontrast TEV aşamalarını atlar (teşhis).
TEV modeli ile gerçek ColorMatrix Python'da ±0,5/255 uyuşuyor
(`tests/weather_dump.c` matrisi döker: `python tools/build_host.py --main
tests/weather_dump.c --out build_host/weather_dump.exe`).

### C. Doku havuzu (isteğe bağlı hız)
`platform/3ds/gpu.c` `release_texture`/`texture()`: aynı boyutlu serbest
dokuları havuzla (74x11, 128x128, 320x192). Önce `uploads` satırıyla ölç.

### D. Bellek sahipliği (Codex bulgusu A)
`as3.h` `As3Array_join`/`AS3_CAT` ara string'leri serbest bırakılmıyor (gün
başına birkaç tane, önemsiz); `FlxG.c cache_set`, `FlxGroup_destroy` yeniden
kullanımı: durum geçişi (yeni oyun/retry) tekrarlarında canlı tahsisi ölç.

### E. Sonraya bırakılanlar
Gün ortası kayıt, dokunarak hedefleme, alt kamera ayarı (kullanıcı: sonra).

## Dikkat
- CPU modu (Gpu_recording=false) her zaman orijinal çeviriyle birebir kalmalı;
  GPU hızlandırmaları yalnızca `Gpu_recording` altında.
- Değiştirdiğin dosyaların gerçekten diske yazıldığını kontrol et.
- `parity_root/` oracle betiğinin geçici kopyası; teslim edilmez.
- Kullanıcının `progress.sav`'ını autotest dışında silme.

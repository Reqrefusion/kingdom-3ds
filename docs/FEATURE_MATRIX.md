# Özellik karşılaştırması: orijinal kaynak / eski kod / yeni kod

- **Orijinal**: noio/kingdom @ 56d4e62 (AS3 + Flixel 2.55, Flash Player)
- **Eski**: `../source` (native C yeniden yazım + paylaşılan motor, Citro2D)
- **Yeni**: `translit/` (AS3'ün satır satır C çevirisi + Flash katmanı)

"Karar" sütunu hangi taraftan ne alındığını / alınacağını gösterir.
`?` = eski kodda doğrulamadığım bilgi.

## Oyun mantığı

| Özellik | Orijinal | Eski kod | Yeni kod | Karar |
|---|---|---|---|---|
| Oyun kuralları (Citizen, Troll, Player, Coin, Shop, Wall, fazlar) | AS3 | Native yeniden yazım; 53 kartlık parite iş listesi, kısmi | Satır satır çeviri, statik %88,1 (4125/4684 ifade, 523 fonksiyon; oyunun çağırmadığı Utils/FlxU/ColorMatrix dahil) | Yeni |
| Çarpışma geri çağrıları (FlxQuadTree) | Bir çift, paylaştığı her yaprakta bir kez çağrılır; zemindeki vatandaş/trol için karede 2 kez | Çift başına 1 kez | Orijinal gibi (bağımsız Python modeliyle doğrulandı) | Yeni (orijinale sadık) |
| Sayı hassasiyeti | Number = double | float (testlerde 0,005 tolerans) | double (V006 sapma 3e-12) | Yeni |
| FlxG.random | FlxU.srand LCG | ? | Birebir | Yeni |
| Math.random | Flash'ın üreteci (kapalı) | ? | Tohumlu xorshift (ADAPTATION) | Yeni |
| Seviye verisi | fields.oel | level_data.inc | level_fields.c (oel'den üretiliyor) | Yeni |
| Hava ön ayarları | WeatherPresets.as | timeline_data.inc | WeatherPresets.c (as'tan üretiliyor) | Yeni |
| Hata ayıklama blokları (CHEATS, watch, minimap) | Var, kapalı | Yok | Çevrildi, aynı sabitle kapalı | Yeni |
| Mochi skor tablosu, URL açma | Var | Yok | Yok (3DS'te anlamsız) | İkisinde de yok |

## Görüntü

| Özellik | Orijinal | Eski kod | Yeni kod | Karar |
|---|---|---|---|---|
| Çizici | Flash yazılım BitmapData | Citro2D, atlas + komut listesi | Flash katmanı → GPU komut listesi → Citro2D (CPU yedekli) | Yeni yapı, eski koddaki GPU kalıpları |
| Sprite dokuları | — | Önceden hazırlanmış KTX atlas | Sprite sayfası bir kez yüklenir; kare = doku koordinatı | Eşdeğer; atlas istenirse eklenebilir |
| Renk tonu / alfa | colorTransform (CPU) | GPU tonu | GPU tonu (çarpanlar); ofset varsa CPU | Eski fikir alındı |
| Karanlık + ışık damgaları | darkness bitmap'ine stamp, multiply | KG_DARKNESS_APPLY (GPU) | GPU yüzeyi (aynı fikir) | Eskiden alındı |
| Su yansıması | Kamera tamponu ters + DisplacementMapFilter (perlin) | KG_WATER_* GPU, şeritler | GPU yüzeyi: yansıma + ışıklar yüzeyde, dalga orijinal perlin haritasından 16 px'lik şeritlerle (eşit kaydırmalı komşular birleşik); negatif ölçek hatası düzeltildi | Eskiden alındı, dalga verisi orijinalden |
| Hava renk matrisi | ColorMatrixFilter | TEV aşamaları (kg_weather_matrix) | Aynı TEV kodu, çevirinin Weather alanlarıyla | Eskiden alındı |
| Metin | Flash TextField | ? | TTF'den önceden rasterlanmış fontlar | Yeni |
| Üst ekran | 288x160 | 1.5x, 400x240 | Bütün kare (1.39x) veya 1.5x doldurma (ayar) | İkisi de var |
| Alt ekranda yansıma görünümü | Yok | Var (native çizim) | Var: Water'ın kendi komutları 160 satıra uzatılıp alt ekranda 1.5x, perlin dalgasıyla | Eskiden alındı (kullanıcı kararı) |
| Stereo 3D | Yok | Stereo sahne katmanları | Nesne başına derinlik (scrollFactor) | Eski fikir, yeni uygulama |

## Platform

| Özellik | Orijinal | Eski kod | Yeni kod | Karar |
|---|---|---|---|---|
| Ses | Flash Sound | NDSP ses havuzu | NDSP; uzun dosyalar SD'den akış; müzik aç/kapa | Yeni |
| Kayıt | savedProgress yalnızca bellekte (taç kaybında yeniden deneme) | progress.sav v16, gün ortası anlık görüntü, arka planda | Şafak kontrol noktası metni SD'ye yazılır, açılışta devam | Açık karar: gün ortası kayıt orijinalde yok |
| Duraklatma menüsü / ayarlar | — | Var | Var (müzik, 3D derinlik, ekran ölçeği, yeni oyun, çıkış) | İkisi de var |
| Kontroller | Klavye + fare | Dokunarak hedefe para verme, Circle Pad hareket başına bir kez | D-pad/Circle Pad, A = para, dokunma = fare | Dokunarak hedefleme orijinalde yok, açık karar |
| New 3DS hız modu, ikinci çekirdek | — | Motor iş kuyruğu | Hız modu + aynı motor iş kuyruğu | Eskiden alındı |
| Kurulum | — | Kayıt ve ayarları koruyor | Kayıt, ayar ve logları koruyor | Eskiden alındı |

## Performans (3DS)

| Ölçüm | Eski kod | Yeni kod |
|---|---|---|
| Kullanıcının gördüğü fps | Hızlı (ölçüm belgesi yok) | 3 → 4 → 8 → 9 → 36 → ~58–60 (Azahar, alt ekran yansımasıyla) |
| Host'ta kare başı CPU (x86) | ? | 4,3 ms (başlangıç) → 0,57 ms (bu sürüm) |
| Bu sürümde | | Su yüzeyi GPU'da, renk tonlu sprite'lar GPU tonuyla, şeritler ekrana kırpılı, alt ekranda kare süresi dökümü |

## Testler

| Test | Eski kod | Yeni kod |
|---|---|---|
| Davranış oracle'ları (check.py, AS3'ten türetilmiş) | 25 | 6 CSV oracle bağlı (4 doğrudan geçiyor, 2'si sadece çift geri çağrı farkı) + 31 doğrudan kaynak türevi kontrol (iş dağıtımı, muhafız, dükkan) hepsi geçiyor |
| C birim testleri (core_test.c) | Var | Native iç yapıya bağlı, taşınmadı |
| Kayıt fixture'ları, izole Azahar testi | Var | Azahar autotest (sabit tohum + betikli girdi, GPU–CPU karşılaştırması, faz taraması); kayıt yazma hatası testi yok |
| Statik karşılaştırma | Ledger (2 MB doküman) | tools/side_by_side.py, %88,1 (523 fonksiyon) |
| Uzun oynanış + bellek denetimi | ? | 25.000 kare ASan/UBSan |
| GPU komut listesi doğrulaması | — | CPU referans oynatıcı, orijinal çizimle piksel farkı ortalama 1,5–4 / 255 |

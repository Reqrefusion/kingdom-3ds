# Parite raporu: C çevirisi ↔ ActionScript kaynağı ↔ orijinal SWF (7 Kasım 2026)

Üç katman karşılaştırıldı. Hepsi yeniden üretilebilir; komutlar her satırda.

## 1. Kaynak snapshot ↔ orijinal SWF (gerçek davranış SWF'tir)

`python tools/swf_vs_source.py <swf> <kaynak dizini>` (SWF bayt kodu `tools/abc_dump.py` ile okunur)

Her metot için karşılaştırma yönleri (<, <=, >, >=), sayısal sabitler ve kullanılan adlar
karşılaştırıldı. Gerçek farklar:

| Yer | Kaynak snapshot | SWF | Çeviride |
| --- | --- | --- | --- |
| `Wall.needsWork` | `... && t < HURT_COOLDOWN` | `... && t > HURT_COOLDOWN` | SWF kuralı (kaynakta yazım hatası) |
| `PlayState.spawnTrolls` | `while(amount)` | `while (amount > 0)` | SWF kuralı (çift sayıda aynı) |
| `PlayState.saveProgress` | doğrudan dizge | `getProgress()` + `sendProgressAnalytics` | yok (analitik) |
| `MenuState` | 2 vurgu alanı | + Kickstarter düğmesi | yok (menü) |

Citizen, Troll, Bunny, Farmland, Shop, Coin, Castle ve Player metotlarında fark yok. Araç `&&`/`||`
ve `+`/`-` farklarını görmez.

## 2. Çeviri (C) ↔ kaynak

**İfade eşleşmesi** (`python tools/side_by_side.py --source <kaynak>` → `STATIC_COMPARISON.md`):
4123 / 4684 ifade (%88,0), 523 çevrilmiş fonksiyon. Eşleşmeyenler C'nin gerektirdiği yazım
(`snprintf`, `AS3_INT`, alan başlatmaları). Çekirdek oyun sınıflarının eşleşmeyen satırları tek
tek okundu: mantık sapması yok.

**Karşılaştırma yönü ve sabit paritesi** (`python tools/parity_ops.py <kaynak>`, yeni araç):
394 fonksiyon karşılaştırıldı, 20'sinde karşılaştırma sayısı, 3'ünde sabit farklı. Hepsi açıklandı:

- Bilinçli SWF düzeltmeleri: `Wall.needsWork`, `PlayState.spawnTrolls`.
- C'nin yapısı: `Castle.morph`, `PlayState.buildObjects`, `Utils.toRoman` (arabellek denetimi),
  `FlxTilemap.*` sınır denetimleri, `ColorMatrix.*` (oyunca çağrılmaz), `Weather.setVariables`
  (dairesel `timeOfDay` dalı), `FlxSound.pause`.
- Yalnız hata ayıklama: `PlayState.create` sabitleri 10/16/32/400 (`FlxInputText`, CHEATS=false),
  `FlxTilemap.drawTilemap` `<=` (görsel hata ayıklama dalı), `PlayState.nextPhase` sabitleri 2/3/4
  (kaynak dizi indisleri, C'de yapı alanları).
- GPU uyarlaması: `FlxSprite.calcFrame` (etiketli "GPU adapter").

**Fonksiyon kapsamı:** 25 kaynak fonksiyonunun C karşılığı yok. Oyun hiçbirini çağırmaz:
FlxPath (11), FlxReplay (5), Mochi (2), FlxInputText/`setWeatherFromInput`, `FlxSound.loadStream`/`gotID3`,
`FlxG.stream`/`fullscreen`, ses tepsisi.

## 3. Davranış

`python tests/run_native_oracles.py`: 167 doğrudan kontrol PASS, 0 FAIL; CSV oracle'ları 4/6
(Flixel-exact), 6/6 (one-callback). İki fark belgelenmiş çift quadtree geri çağrısıdır.
`python tests/regress.py`: OK (CPU çıktısı baseline ile piksel piksel aynı, GPU 0,19–0,45/255).
Gece/akşam sahnesi Ruffle'daki orijinalle yan yana karşılaştırıldı; ses mantığı (faz–müzik tablosu,
ortam hacimleri) SWF değil **kaynaktan** üretilen tabloyla doğrulandı.

## Sınırlar (doğrulanmadı)

- Flash ile çeviri arasında kare kare durum karşılaştırması yok; yalnızca doğrudan kontroller,
  CSV oracle'ları ve Ruffle görüntüleri.
- `Math.random` aynı değil; rastgele olaylar orijinalle aynı sırada gelmez.
- SWF'in kendisine karşı yalnızca üst düzey karşılaştırma yapıldı (yön, sabit, ad). Tam bayt kodu
  eşdeğerliği kontrol edilmedi.
- Gerçek 3DS'te denenmedi; ses duyulabilirliği Azahar/konsolda dinlenmedi.

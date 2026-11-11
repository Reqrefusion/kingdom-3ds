# Flash (AS3) oyununu 3DS'e C olarak çevirme kılavuzu

Bu kılavuz, bu klasördeki sistemi (Kingdom'ın satır satır C çevirisi) başka bir Flash oyununu
çevirmek için kullanacak kişi veya yapay zekâ içindir. Önce bunu, sonra `AGENTS.md`'yi oku.
Her komut `translit/` klasöründen çalıştırılır. Tek giriş noktası: `python tools/port.py`.

## 0. Tek paragrafta sistem

Bir ActionScript 3 oyunu (`.as` dosyaları + Flixel gibi bir motor) **dosya dosya, fonksiyon fonksiyon,
ifade ifade** C'ye çevrilir. Amaç oyunun mantığının orijinalle **aynı** olmasıdır. Çevirilen kod
`game/` ve `flixel/` içindedir ve **sonradan "iyileştirilmez"**. Flash Player'ın yaptığı işler
(bitmap, matris, filtre, ses) `flash/` içinde yeniden yazılmıştır. Konsola özgü şeyler (çizim hızlandırma,
3D, ses sürücüsü, menü) `platform/` içindedir. Doğruluk, çevirinin kaynakla ve **gerçek SWF'in bayt koduyla**
araçlarla karşılaştırılmasıyla ve küçük testlerle kanıtlanır.

## 1. Katmanlar ve kurallar

| Klasör | Ne | Değişir mi? |
| --- | --- | --- |
| `as3/` | AS3 dil davranışı (`int()`, `>>>`, `Array`, string, vtable) | Nadiren, yalnızca dil hatası bulunursa |
| `flash/` | Flash Player'ın yerleşik işleri (BitmapData, filtreler, Sound...) + GPU kaydedici | Evet, ama **sonuç Flash'la aynı olmalı** |
| `flixel/` | Motor çevirisi (`FlxSprite.as` → `FlxSprite.c`) | Yalnızca motor `.as` ile birebir |
| `game/` | Oyun çevirisi (`Player.as` → `Player.c`) | Yalnızca `.as`/SWF ile birebir |
| `platform/` | host (PC test), 3ds (citro2d, ses), common (stereo, kayıt, menü) | Serbest (parite dışı) |
| `tools/`, `tests/` | araçlar ve testler | Evet |

**Altın kurallar (zorunlu):**
1. `game/` ve `flixel/` içinde oyunun/motorun mantığı dışında bir şey olmaz. 3D, GPU, hız, görsel süs
   **yalnızca** `flash/` veya `platform/` içine girer. `python tools/check_parity_boundary.py` bunu denetler.
2. Bir `.as` dosyası = bir `.c` dosyası. `Class.method` → `Class_method(Class *self, ...)`.
   İfadelerin sırası, adları ve sabitleri aynen kalır.
3. **Gerçek davranış SWF'tir, kaynak kod değil.** Kaynak depoları SWF'ten sapabilir (bkz. 6.1). Çeviriye
   başlamadan önce `swf-diff` çalıştır.
4. Emin olmadığın veya Flash'ta doğrulayamadığın her şeye `ADAPTATION:` yorumu koy. Listeyi
   `grep -rn ADAPTATION` verir. `SWF FIX:` yorumu ise SWF'in kaynaktan farklı olduğu yerleri işaretler.
5. Tahmin etme. Kanıtı olmayan davranış ekleme. Bilmiyorsan dur ve yaz (bkz. 9).

## 2. Başlamadan önce (hazırlık)

```
python tools/port.py doctor            # araçlar ve yollar hazır mı?
```
1. Orijinal **SWF**'i ve **AS3 kaynağını** al. `port.json` içine yollarını yaz (`swf`, `source_dir`).
2. `python tools/port.py swf-diff` ile SWF'i kaynakla karşılaştır. Çıkan her fark için
   `python tools/port.py abc <Sınıf> <metot>` ile gerçek bayt kodunu oku. **SWF'te ne varsa o geçerlidir.**
3. Sınıf bağımlılık sırasını çıkar: önce dil/motor (`as3/`, `flash/`, `flixel/`), sonra oyun sınıfları,
   yapraktan köke (örnek: `Utils` → `Coin` → `Citizen` → `PlayState`).
4. Varlıkları (png, mp3) `tools/convert_assets.py` ile dönüştür (`translit_assets/`).

## 3. Bir `.as` dosyası nasıl çevrilir (örnek: Reed)

AS3 (`Reed.as`):
```actionscript
public class Reed extends FlxSprite{
    private var weather:Weather;
    private var t:Number = 0;
    public function Reed(X:int, Y:int){
        super(X,Y);
        loadGraphic(ReedImg, true, false, 32, 32);
        this.weather = (FlxG.state as PlayState).weather;
    }
    override public function update():void{
        t += weather.wind;
        frame = int(3 * (0.5 + 0.5*Math.sin(0.05*t + x)) + 0.3 * Math.sin(0.2*t));
    }
}
```
C (`game/Reed.c`) ve başlık (`game/game.h`):
```c
typedef struct Reed {
	FLXSPRITE_FIELDS            /* taban sınıfın alanları EN BAŞTA: self->x her alt sınıfta çalışır */
	Weather *weather;
	Number t;                   /* Number = double */
} Reed;
extern const As3Class Reed_class;
Reed *Reed_new(int32_t X, int32_t Y);
void Reed_update(Reed *self);

const As3Class Reed_class = {                       /* sınıf tablosu: üst sınıf + sanal metotlar */
	"Reed", &FlxSprite_class, NULL,
	{FLXSPRITE_VT, .update = (void (*)(FlxBasic *))Reed_update},   /* override edilenler burada */
};

Reed *Reed_new(int32_t X, int32_t Y)
{
	Reed *self = as3_alloc(sizeof *self);
	self->klass = &Reed_class;
	self->t = 0;                                    /* alan başlangıç değerleri super()'den ÖNCE çalışır */
	SUPER(FlxSprite_ctor((FlxSprite *)self, X, Y, NULL));
	FlxSprite_loadGraphic((FlxSprite *)self, ReedImg, true, false, 32, 32, false);  /* varsayılan argümanlar AÇIKÇA yazılır */
	self->weather = PLAYSTATE->weather;             /* (FlxG.state as PlayState) */
	return self;
}

void Reed_update(Reed *self)
{
	self->t += self->weather->wind;
	FlxSprite_set_frame((FlxSprite *)self, AS3_UINT(AS3_INT(3 * (0.5 + 0.5 * sin(0.05 * self->t + self->x)) + 0.3 * sin(0.2 * self->t))));
}
```
Dikkat: `frame = ...` bir **setter**'dır → `FlxSprite_set_frame(...)`. `int(...)` → `AS3_INT`, atamadaki `uint`
dönüşümü → `AS3_UINT`. Hiçbir ifade "sadeleştirilmedi".

## 4. AS3 → C sözlüğü

| AS3 | C | Not |
| --- | --- | --- |
| `Number` | `double` (`Number`) | tüm sayısal hesap double'dır |
| `int(x)`, `uint(x)` | `AS3_INT(x)`, `AS3_UINT(x)` | sıfıra doğru keser, 32 bit sarar; `NaN` → 0 |
| `a >> n`, `a >>> n` | `AS3_SAR(a, n)`, `AS3_SHR(a, n)` | C'nin `>>`'ü işaretli sayıda farklıdır |
| `a % b` (Number) | `as3_mod(a, b)` | C'nin `%`'i tamsayıdır, `fmod` işaret kuralları farklı olabilir |
| `Math.round(x)` | `Math_round(x)` | AS3 .5'i yukarı yuvarlar (`floor(x+0.5)`) |
| `Math.random()` | `Math_random()` | Flash'ın üreteci gizli: sıra orijinalle aynı OLAMAZ (belgele) |
| `FlxG.random()` | `FlxG_random()` | Flixel'in kendi LCG'si: orijinalle AYNI. **Çağrı sırası ve sayısı davranışı değiştirir** |
| `"a" + n + "b"` | `AS3_CAT("a", AS3_STR(n), "b")` | belleği sen serbest bırakma, yardımcılar yönetir |
| `var a:Array`, `a[i]`, `a.length` | `As3Array`, `As3Array_get(&a, i)` | sınır dışı indeks: AS3 `undefined` verir, C'de `NULL` dön (bkz. 6.5) |
| `for each (var x in g.members)` | `for (i < length) { x = As3Array_get(...); }` | AS3 `null` üyeyi atlamaz, sen de atlama; `exists/alive` bakma, kaynak bakıyorsa bak |
| `o.f()` (sanal) | `CALL0(o, f)` / `CALL(o, f, args...)` | alt sınıf override edebiliyorsa vtable üzerinden çağır |
| `super.f()` | `SUPER(Base_f(self))` | |
| `x is Foo`, `x as Foo` | `as3_is((FlxBasic *)x, &Foo_class)` ve `(Foo *)x` | `as` başarısızsa `null` verir: kontrol et |
| interface (`Workable`) | `Workable_needsWork(FlxBasic *w)` içinde `as3_is` ile dağıtım | her uygulayan sınıf için bir dal |
| `static const X = 5` | `#define Class_X 5` veya `static const` | sınıf adıyla öneklenir |
| `get/set x` | `Class_get_x(self)` / `Class_set_x(self, v)` | alan atamasına benzeyen ama setter olan satırlara dikkat |
| varsayılan argüman `f(a, b=false)` | çağıran her yerde açıkça yazılır; `NaN` varsayılanı `NAN` | `pickNewGoal(preset = NaN)` |
| callback / closure | fonksiyon göstericisi + `self` | tablo satırları için bkz. `PlayState.c` `P(...)` |
| `null` / `undefined` | `NULL` | |
| `trace(...)` / `FlxG.log` | `FlxG_log(...)` (stderr) | |

Tanımlayıcı adları (`Player`, `playstate`, `FlxG`) AS3'teki gibi kalır, yalnızca `Class_method` biçimi eklenir.

## 5. Çevrilmeyen şeyler: Flash'ın yerleşikleri

Oyun kodu `BitmapData.perlinNoise`, `DisplacementMapFilter`, `Graphics.beginGradientFill`,
`BitmapData.draw/copyPixels`, `Sound` gibi **Flash Player'ın içinde** çalışan işlemleri çağırır. Bunlar
bayt kodunda yoktur (SWF yalnızca çağrıyı içerir). Sen bunları `flash/` içinde Flash'ın **belgelenmiş**
davranışına uygun yazarsın. Kurallar:
* Oyun kodundaki çağrı argümanları SWF'tekiyle aynı olmalı (`abc_dump.py` ile doğrula).
* Yerleşiği hızlandırmak için yaklaşık yaparsan (GPU şeritleri gibi) `ADAPTATION:` yaz ve CPU yolunu
  **tam doğru** tut; hızlandırma yalnızca `Gpu_recording` açıkken devreye girsin.
* Bir yerleşiğin piksel doğruluğu bayt kodundan kanıtlanamaz: Ruffle'da orijinali çalıştırıp
  görüntü/istatistik karşılaştır (`python tools/port.py ruffle ...`).

## 6. Dikkat edilecekler (bu projede yaşanmış gerçek hatalar)

1. **Kaynak ≠ SWF.** `Wall.needsWork` kaynak depoda `t < HURT_COOLDOWN`, SWF'te `t > HURT_COOLDOWN`
   idi. Kaynağa körü körüne uyunca çiftçiler duvar inşa edemedi, krallık 200 px'te kaldı. `spawnTrolls`
   döngü koşulu da farklıydı. **Önce `swf-diff`, şüphede `abc <Sınıf> <metot>`.**
2. **Değerlendirme sırası.** C'de fonksiyon argümanlarının hesap sırası belirsizdir, AS3'te soldan sağadır.
   Aynı çağrıda birden çok `FlxG.random()` varsa geçici değişkenlerle AS3 sırasını koru; yoksa rastgele
   sayı dizisi kayar ve tüm oyun farklı gider.
3. **Grup gezintisi.** Flixel `members` dizisini gezerken `exists/alive` kontrolü yapmaz (ör. `checkWork`).
   Ölü işaretlenmiş nesneler hâlâ işlenir. Testte "öldür" yerine grubu gerçekten boşalt.
4. **Tamsayı/NaN.** `uint(-1)`, `int(NaN)`, taşma, `0/0`: hepsi AS3 kurallarıyla (`AS3_INT/UINT`) yapılmalı.
   `A0` gibi geçersiz bir faz AS3'te `undefined`, C'de çökme demektir: sınır dışı dizi erişimini `NULL` yap.
5. **Kare/çerçeve sınırı.** `FlxSprite.frame` sayfanın dışındaysa Flash `copyPixels` hiçbir şey kopyalamaz,
   önceki kare kalır (Reed kare 3). GPU yolu aynı davranmalı.
6. **Yuvarlama.** `copyPixels` hedef noktayı **aşağı** yuvarlar (floor), `Math.round` .5'i yukarı. 1 piksel
   kayma bu yüzden çıktı.
7. **Test kırılganlığı.** Rastgele sayıya bağlı testler önceki testlerin tükettiği rastgele sayıya göre
   değişir: testi kendi tohumuyla başlat.
8. **Parite dışı süsler.** 3D, su eğimi, HUD gibi şeyleri `game/`'e koyma. `check_parity_boundary.py`
   kırmızı olur.
9. **Araç tuzakları.** Bash araçları ters bölüyü (`\n`) yutar: C dizelerini yalnızca Edit/Write ile düzenle.
   Aynı emülatörü başka bir iş kullanıyor olabilir: onu kapatma (`port.py azahar` zaten reddeder).

## 7. Doğrulama merdiveni (aşağıdan yukarıya, her basamağın ne kanıtladığı)

| Basamak | Komut | Ne kanıtlar | Ne kanıtlamaz |
| --- | --- | --- | --- |
| Sınır | `port.py check --only boundary` | parite alanı temiz | — |
| Statik | `port.py check --only static` | ifadeler kaynakla aynı (%) | davranış |
| Operatör/sabit | `port.py check --only parity_ops` | eşik/zamanlayıcı/yön değişmemiş | `&&`/`+` farkı |
| SWF farkı | `port.py swf-diff` | kaynak SWF'ten sapıyor mu | yerleşik işlemler |
| Doğrudan testler | `port.py check --only oracles` | kaynaktan türetilen beklentiler | tam oyun |
| Regresyon | `port.py check --only regress` | CPU çıktısı bit bit aynı kaldı | doğruluk (yalnızca "değişmedi") |
| Ruffle | `port.py ruffle ...` | görüntü orijinalle uyuşuyor | zamanlama farkı |
| Emülatör | `port.py azahar ...` | 3DS yolu çalışıyor, hız | gerçek donanım |
| Gerçek 3DS | elle | her şey | — |

Tümü: `python tools/port.py check` (hızlı: `--quick`). Çıkış kodu 1 ise bir şey bozuk.

## 8. Yeni oyun için adım adım iş akışı

1. Bu klasörü yeni proje klasörüne kopyala; `port.json`'u düzenle; `port.py doctor`.
2. `port.py swf-diff` → farkları `tools/known_diffs.json`'a gerekçeyle yaz (veya düzelt).
3. Motor sınıflarını çevir (Flixel'se hazır). Yoksa `flixel/` benzeri bir klasör aç.
4. Oyun sınıflarını yapraktan köke çevir. Her sınıf için:
   a. `.as` dosyasını tamamen oku. b. `.c` + `game.h` girdileri. c. `port.py check --quick`.
   d. Bitti sayılması: derleniyor, `parity_ops` yeni fark yok, o sınıf için doğrudan test var.
5. İlk çalışan kareyi host'ta üret (`kingdom_host.exe --frames N --png-every M`), Ruffle'la karşılaştır.
6. `tests/baseline/` oluştur (CPU çıktısı) → `regress.py` bundan sonra bozulmayı yakalar.
7. 3DS: `port.py build-3ds`, `deploy`, `azahar`. Hız için yalnızca `platform/`/`flash/gpu.c` içinde çalış.
8. Her tur sonunda `CLAUDE.md` benzeri bir devir belgesine ne yaptığını, neyi doğrulayamadığını yaz.

## 9. Düşük seviyeli yapay zekâ için kurallar

**Yap:** küçük adımlar; her adımdan sonra `port.py check --quick`; gerçek çıktıyı oku; fark bulursan iki tarafı
(AS3 ve C) yan yana oku; emin olmadığını açıkça yaz.
**Yapma:** `game/` içinde "daha iyi" kod yazma; testi geçsin diye beklentiyi değiştirme; kanıtsız sabit uydurma;
`known_diffs.json`'a gerekçe okumadan ekleme; başka bir işin Azahar'ını kapatma; çalıştırmadığın testi "geçti" deme.
**Dur ve sor:** SWF ile kaynak çelişiyorsa ve hangisinin doğru olduğu `abc` ile okunamıyorsa; bir Flash yerleşiğinin
davranışını bilmiyorsan; bir düzeltme `game/` dışında çözülemiyorsa.

## 10. Araç rehberi

| Araç | Ne için |
| --- | --- |
| `tools/port.py` | tek giriş: `doctor`, `check`, `swf-diff`, `abc`, `build-3ds`, `deploy`, `azahar`, `ruffle` |
| `tools/abc_dump.py` | SWF bayt kodunu okur (`<swf> <Sınıf> <metot>`, `<swf> <Sınıf> --consts`) |
| `tools/swf_vs_source.py` | SWF ↔ kaynak: karşılaştırma yönü, sabit, kullanılan ad farkları |
| `tools/parity_ops.py` | C ↔ kaynak: karşılaştırma yönü ve sabit farkları |
| `tools/side_by_side.py` | ifade eşleşmesi → `STATIC_COMPARISON.md` |
| `tools/check_parity_boundary.py` | `game/`/`flixel/` temiz mi |
| `tools/known_diffs.json` | gözden geçirilmiş, kabul edilmiş farklar (gerekçeli) |
| `tests/translit_scenarios.c` | doğrudan kaynak-türevi testler (`run_native_oracles.py` çalıştırır) |
| `tests/regress.py` | CPU çıktısı baseline ile bit bit aynı mı |
| `tests/npc_probe.c` | uzun koşuda davranış gözlemi (ör. NPC'ler hareket ediyor mu) |
| `tools/azahar_autotest.ps1`, `ruffle_shot.ps1` | emülatör / Ruffle ekran görüntüsü |

Ayrıntılı geçmiş ve çözülmüş sorunlar: `CLAUDE.md`. Güncel parite durumu: `PARITY_REPORT.md`.

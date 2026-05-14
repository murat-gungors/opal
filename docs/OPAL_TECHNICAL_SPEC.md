# Opal — Teknik Şartname (v0.1)

**Proje:** Audio-Reactive Görsel Üreteci (Standalone + VST3)
**Tarih:** 14 Mayıs 2026
**Statü:** Araştırma tamamlandı, implementasyon öncesi karar belgesi
**Hedef okuyucu:** Murat (proje sahibi) + Claude Code (uygulayıcı)

---

## 0. Yönetici Özeti

Opal, Ableton Live içinde VST3 olarak çalışırken **müziğin beat'ine ve enerji konturuna kilitli, modern-minimal jeneratif görseller** üreten bir görselleştiricidir. Aynı kod tabanından bir **standalone uygulama** olarak da çıkar; standalone modda host transport yoksa sadece ses analizinden beslenir. Arayüz minimaldir: tam ekran görsel alanı + 8 knob.

Bu belge, ham kod yazılmadan önce alınması gereken **teknik kararları kayda geçirir** ve Claude Code'a "git, bunu uygula" diyebilecek netlikte bir başlangıç planı sunar. Açıkta kalan birkaç karar son bölümde listelenmiştir; Murat'ın bunları kapatması bekleniyor.

**Birinci öncelik:** Beat-tight senkron. Görsel ile ses arası gecikme, kullanıcının "tight" hissetmesini sağlayan eşik altında (~50ms toplam) kalmalı.

**Birinci sürümün stili:** Teenage Engineering OP-1 + Brian Eno 77 Million Paintings ekseninde — büyük SDF formlar, multi-stop gradient, ince film grain, ölçülü domain warp, hafif feedback trail. Shadertoy kaosu değil.

---

## 1. Kararlar Özeti

| # | Karar Alanı | Seçim | Tek Cümlelik Gerekçe |
|---|---|---|---|
| 1 | Birincil platform | macOS (Apple Silicon öncelik) | Geliştirme ortamı macOS; Ableton macOS'ta hedefleniyor. Windows v2'de. |
| 2 | Framework | **JUCE 8 (AGPLv3 lisansı altında)** | Yeni başlayan için dokümantasyon/topluluk avantajı belirleyici; iPlug2 teknik olarak biraz daha hizalı ama dokümantasyon riski yüksek. (Detay §3) |
| 3 | Lisans modeli | AGPLv3 — proje açık kaynak | Kullanıcı kişisel/OSS yapacak; JUCE 8 ücretsiz tier'ı sadece bu yolla geliyor. |
| 4 | Render API | OpenGL 4.1 Core + GLSL 410 (macOS tavanı) | macOS'ta JUCE'nin desteklediği en yüksek seviye; cross-platform aynı shader'lar Windows'ta da çalışır. |
| 5 | Metal'e geçiş | v2+ kapsamında, JUCE 8 Metal renderer roadmap'ini izle | Bugün üretim hazır değil; manuel CAMetalLayer köprüsü MVP için fazla risk. |
| 6 | Render bağlama | `juce::OpenGLContext.attachTo(editor)` pattern | Plugin scope'unda standart, host context'iyle izole. |
| 7 | Audio analiz | 512-örnek FFT (Hann window, %50 overlap) | Görsel-reactive için sweet spot; bass 93Hz çözünürlük yeterli, latency ~10ms. |
| 8 | Bant ayrıştırma | Logaritmik 3-band (bass/mid/high) + RMS + spectral flux onset | Basit, görsel için %95 etki, mel-spectrogram overkill. |
| 9 | Audio→Render thread | `std::atomic<AnalysisFrame>` snapshot + 2048-örnek `AbstractFifo` (waveform için) | Lock-free, yeni başlayan için anlaşılır, audio-thread'de allokasyon yok. |
| 10 | Beat tracking (MVP) | Onset-only (spectral flux), BPM tahmini yok | Plugin modda Ableton BPM verir; standalone'da onset yeterli, BTrack v2'ye. |
| 11 | Beat sync (plugin modda) | `AudioPlayHead::PositionInfo` (yeni API), `ppqPosition` + `bpm` | Sample-accurate, tempo otomasyonuna dayanıklı. |
| 12 | Build sistemi | CMake (`juce_add_plugin`) | 2026'da JUCE community standardı; Projucer eski. |
| 13 | Proje template | **Pamplejuce** (Sudara) fork'undan başla | Hazır: JUCE 8 submodule, Catch2, pluginval, GitHub Actions. |
| 14 | Birincil dev döngüsü | **Standalone build** (Ableton'a her seferinde yükleme YOK) | En hızlı iterasyon; DAW testi sadece milestone'larda. |
| 15 | Shader yükleme (dev) | Diskten runtime + `FileWatcher` hot-reload | Plugin'i kapatmadan shader değişikliğini canlı görme. |
| 16 | Shader yükleme (release) | `juce_add_binary_data` ile embed | Tek dosyada dağıtım. |
| 17 | Code signing (MVP) | Yok (ad-hoc local) | Kişisel kullanımda gerek yok; dağıtım gerektiğinde Pamplejuce'in hazır pipeline'ı açılır. |
| 18 | Hedef latency | ≤50ms (audio in → pixel out) | İnsan AV-senkron toleransının "tight" bandının üst sınırı. |
| 19 | VST3 parametre otomasyonu | 8 knob hepsi otomatize edilebilir | Ableton'da knob'lar canlandırılabilir; bu MVP'de var. |
| 20 | Görsel stil | SDF formlar + multi-stop gradient + hash-grain + smooth warp + 1-frame feedback | Teenage Engineering / Eno hattının modern shader karşılığı. |

---

## 2. Framework Kararı — JUCE 8 (gerekçeli)

Araştırma iki güçlü aday üretti: **JUCE 8** ve **iPlug2**. Tablo:

| Kriter | JUCE 8 | iPlug2 |
|---|---|---|
| Lisans (kişisel/OSS) | AGPLv3 → ücretsiz, kaynak açık olmalı | Liberal (WDL/zlib benzeri) — kapalı kaynak da serbest |
| Yeni başlayan dokümantasyonu | ⭐⭐⭐⭐⭐ (resmi tutorial, kitaplar, sayısız blog) | ⭐⭐⭐ (kod-temelli öğrenme, dağınık) |
| Topluluk / Stack Overflow / Discord | Çok büyük | Orta |
| macOS Metal native | ❌ (roadmap'te) | ✅ (`IGRAPHICS_METAL` flag) |
| Shader entegrasyonu kolaylığı | OpenGL üzerinden manuel ama temiz | `IShaderControl` + SkSL — tek satır |
| Standalone + VST3 macOS | ✅ tek satır | ✅ tek satır |
| Üretim örnekleri | Vital, Surge XT, Tracktion, yüzlerce | Daha az ama var |

**Neden JUCE'a meylediyorum (Murat'ın profili için):**

1. **Sen C++'a ve plugin dünyasına yenisin.** Dokümantasyon, tutorial, forum desteği ilk 3 ayda hayat kurtaracak. iPlug2'de "tıkanıp Stack Overflow'da çözüm bulamama" riski yüksek.
2. **Metal yokluğu MVP için bir engel değil.** macOS, OpenGL 4.1'i 2018'de deprecated etti ama hâlâ çalışıyor (Apple kaldırma tarihi vermedi). Apple Silicon'da OpenGL performansı zayıf ama bizim shader'larımız ağır değil — minimal fragment shader'lar 4K @ 60Hz'te de rahat çalışır.
3. **Pamplejuce template'i** (Sudara) ile saatler içinde derlenir, çalışır, CI/CD hazır bir başlangıç noktası var. iPlug2'de eşdeğer "hazır kurulum" yok.
4. **GPL endişesi:** Kullanıcı zaten açık kaynak yapacağını söyledi — AGPLv3 fiilen "bedava" oluyor. `LICENSE` dosyasına `AGPL-3.0` koymak yeterli.

**iPlug2 ne zaman tercih edilirdi:**
- Eğer Murat C++/plugin'de deneyimli olsaydı.
- Eğer ileride kapalı kaynak ticari plana kesin geçilecek olsaydı.
- Eğer ilk gün Metal performansı zorunlu olsaydı.

**Karar:** JUCE 8 + AGPLv3. iPlug2'yi v2 değerlendirme listesine al (Metal/perf optimizasyon zamanı gelirse).

---

## 3. Önerilen Mimari

### 3.1 Thread Modeli

```
┌─────────────────────────────────────────────────────────────────────┐
│ AUDIO THREAD (gerçek-zamanlı, kutsal: kilit yok, allokasyon yok)    │
│   processBlock(buffer, midi):                                        │
│     1. Audio sample'ları ring buffer'a yaz                          │
│     2. Her 256 sample'da bir FFT analizi (Hann window)              │
│     3. 3-band envelope follower → bassLevel, midLevel, highLevel    │
│     4. RMS → fullEnergy                                              │
│     5. Spectral flux → onset trigger (atomik sayaç++)               │
│     6. Host playhead → bpm, ppqPosition (varsa)                     │
│     7. AnalysisFrame snapshot'ını std::atomic'e yaz                 │
└─────────────────────────────────────────────────────────────────────┘
                            │ (atomik okuma)
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│ RENDER THREAD (OpenGL, ~60 FPS, VSync ON)                            │
│   renderOpenGL():                                                     │
│     1. AnalysisFrame snapshot'ını oku (lock-free)                    │
│     2. 8 knob değerini oku (atomik / value tree listener)            │
│     3. Uniform'ları shader'a yaz                                     │
│     4. Quad render et (fragment shader = görsel motor)               │
│     5. Optional: 1-frame feedback FBO bind/swap                      │
└─────────────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────────┐
│ MESSAGE THREAD (GUI, JUCE Timer)                                     │
│   - 8 knob'un Component repaint'i (sadece knob hareketinde)         │
│   - Shader hot-reload watcher (dev mode)                            │
│   - Status/debug overlay (FPS, latency, host transport durumu)      │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.2 Veri Akışı: `AnalysisFrame`

```cpp
struct AnalysisFrame {
    float bassLevel;        // 0..1 — envelope-followed, 20-250Hz
    float midLevel;         // 0..1 — 250-4000Hz
    float highLevel;        // 0..1 — 4-20kHz
    float rms;              // 0..1 — full-band energy
    uint32_t onsetCounter;  // monotonik artar, her onset'te ++ (render thread frame-to-frame fark alır)
    float spectralCentroid; // 0..1 — normalleştirilmiş parlaklık
    float bpm;              // host BPM (varsa), yoksa 0
    float ppqPosition;      // host playhead (varsa), yoksa -1
    bool isPlaying;
};
// Toplam ~32 byte, tek cache line sığar → std::atomic<AnalysisFrame> tek seferde yazılır.
```

> **Not:** `std::atomic<T>` `T` 16 byte'ı aşıyorsa derleyici lock kullanabilir. 32 byte için `std::atomic_is_lock_free` runtime kontrolüyle doğrula; gerekirse `juce::AbstractFifo` ile snapshot tek-üretici-tek-tüketici kuyruğuna çevir.

### 3.3 Modül Diyagramı

```
source/
├── Plugin/
│   ├── PluginProcessor.{h,cpp}      ← AudioProcessor (DSP girişi)
│   ├── PluginEditor.{h,cpp}         ← AudioProcessorEditor (UI kökü)
│   └── PluginParameters.{h,cpp}     ← AudioProcessorValueTreeState + 8 knob param tanımı
├── DSP/
│   ├── Analyzer.{h,cpp}             ← FFT + 3-band envelope + RMS
│   ├── OnsetDetector.{h,cpp}        ← Spectral flux onset
│   └── AnalysisFrame.h              ← Paylaşılan veri struct
├── GUI/
│   ├── VisualizerComponent.{h,cpp}  ← OpenGLContext sahibi, OpenGLRenderer impl
│   ├── KnobStrip.{h,cpp}            ← 8 knob layout
│   ├── ShaderProgram.{h,cpp}        ← OpenGLShaderProgram wrapper + hot-reload
│   └── Resources/Shaders/           ← .glsl dosyaları (binary embed + dev runtime)
├── Util/
│   ├── LockFreeBus.h                ← std::atomic<AnalysisFrame> + AbstractFifo
│   └── ShaderWatcher.{h,cpp}        ← FileWatcher → reload callback
└── Standalone/
    └── (JUCE Standalone wrapper otomatik üretilir, ekstra kod gerekmez)
```

---

## 4. MVP Kapsamı

### 4.1 İÇERİDE (v0.1)

- ✅ Standalone (.app, macOS) + VST3 (.vst3, macOS) tek codebase'den.
- ✅ 8 adet float knob (0..1 normalize), VST3 parametre olarak host'a expose.
- ✅ Tek tam-ekran visualizer paneli + altta 8 knob strip'i.
- ✅ Audio analiz: 512-FFT, 3-band envelope, RMS, spectral flux onset.
- ✅ Host transport okuma (plugin modda): BPM, ppqPosition, isPlaying.
- ✅ Standalone modda audio input cihazı seçimi (JUCE'nin default'u yeterli).
- ✅ Tek fragment shader ile görsel motor (aşağıda 8 knob ile tüm "stil" parametreleri kontrol edilebilir).
- ✅ Shader hot-reload (dev mode flag açıkken).
- ✅ State save/recall (set'i kapatıp açtığında knob değerleri geri gelir).
- ✅ Pamplejuce + GitHub Actions ile CI build.
- ✅ pluginval strictness 5 ile geçen test.

### 4.2 DIŞARIDA (v0.1'de YOK)

- ❌ Birden fazla görsel preset / "shader bank". MVP tek shader.
- ❌ Standalone modda BPM tahmini (BTrack). Onset-only yeterli.
- ❌ Windows build (v0.2 hedefi; kod cross-platform yazılacak ama test edilmeyecek).
- ❌ Audio Unit (AU) formatı. Sadece VST3 + Standalone. (AU 5 dakikalık iş, ama Logic test'i yapılmayacaksa eklemenin maliyeti var.)
- ❌ MIDI giriş (görseli MIDI ile tetikleme).
- ❌ Code signing / notarization (sadece yerel kullanım).
- ❌ Kullanıcı kendi shader'ını yükleyebilsin / shader editor.
- ❌ Preset kaydetme/yükleme (state recall var ama "preset bank" UI'ı yok).
- ❌ Full-screen mode toggle / external window mode.
- ❌ Görsel kayıt (video out).

### 4.3 "İlerideki Sürümler" Listesi (sadece referans için)

- v0.2: Windows + universal build, AU formatı.
- v0.3: 3-5 shader preset, preset bank UI.
- v0.4: BTrack ile standalone BPM tahmini.
- v0.5: External fullscreen window (ikinci monitör için).
- v1.0: Code signing + notarization + dağıtım (eğer yayınlanacaksa).

---

## 5. 8 Knob Önerisi

Her knob 0..1 normalize. Shader uniform isimleri parantez içinde.

| # | Knob | İşlev | Bağlandığı Audio Verisi | Shader Etkisi |
|---|---|---|---|---|
| 1 | **DRIVE** (`uDrive`) | Genel görsel yoğunluk / "vurma" gücü | `rms` ile çarpılarak | Tüm modülasyonların master multiplier'ı |
| 2 | **BASS** (`uBassMod`) | Bass'ın scale/zoom'a etkisi | `bassLevel` | SDF formun scale/displacement miktarı |
| 3 | **HUE** (`uHueShift`) | Renk paletinin kayma miktarı | `midLevel` ile modüle | Gradient'in hue rotation'ı |
| 4 | **GRAIN** (`uGrain`) | Film grain / dither yoğunluğu | `highLevel` ile boost | Hash-noise mix oranı |
| 5 | **WARP** (`uWarp`) | Domain warp şiddeti | `spectralCentroid` ile modüle | Position vector'a sin/cos warp |
| 6 | **TRAIL** (`uFeedback`) | 1-frame feedback decay | `onsetCounter` reset tetikleyici | Önceki frame'in opacity çarpanı (0.85–0.99) |
| 7 | **POP** (`uOnsetPulse`) | Onset'lerin görsel "pop"u | `onsetCounter` (frame fark) | Onset'te kısa scale/brightness pump |
| 8 | **TEMP** (`uTempCool`) | Palet sıcaklığı (cool ↔ warm) | (Statik, kullanıcı kontrolü) | Gradient color stop'ları arası interpolasyon |

**Tasarım kuralı:** Knob'lar **multiplier** olarak çalışır, **on/off switch** değil. Hepsi 0'da görsel hâlâ var (sade temel gradient + form), hepsi 1'de görsel maksimum reactive. Bu, "yanlış" ayarın imkânsız olduğu bir UX yaratır.

**VST3 otomasyon:** 8 knob da `AudioProcessorValueTreeState` üzerinden parametre. Ableton'da knob'lar otomasyon eğrisiyle canlandırılabilir; kullanıcı bir track üzerinde `DRIVE` parametresini drop'ta yukarı, breakdown'da aşağı çekebilir.

---

## 6. Görsel Tasarım Dili

### 6.1 Stil DNA'sı

- **Teenage Engineering OP-1/OP-Z** ekran grafikleri: büyük silüet, sınırlı palet, "az ama doğru".
- **Brian Eno — 77 Million Paintings**: yavaş, kesintisiz, yumuşak geçişler.
- **Risograph poster grain**: gradient + iri grain = modern + dokunsal.
- **Manuel Rossner sculpture vibe**: tek büyük 3D form, parlak yüzey, gradient kaplama.

### 6.2 Temel Shader Katmanları (alt→üst sırayla)

1. **Multi-stop gradient background** — 2-4 renk durağı arası `smoothstep` mix. Hue knob ile rotate edilir, Temp knob ile palet seçilir.
2. **SDF form (primary)** — daire / halka / yumuşak rectangle (`smin` ile). Bass knob → scale/displacement.
3. **Smooth domain warp** — düşük amplitüdlü `sin(pos.yx * freq + time)`. Warp knob → amplitüd.
4. **1-frame feedback** — önceki frame'in `0.85–0.99` çarpanıyla alt katmana toplanması. Trail knob → decay.
5. **Onset pulse overlay** — onset'te kısa süreli (50-150ms) scale/brightness "ping". Pop knob → şiddet.
6. **Hash-grain dither (üst katman)** — banding'i gizler, dokunsal his. Grain knob → mix.

### 6.3 Audio→Visual Mapping Doktrini

Milkdrop topluluğunun ve modern visualizer'ların öğrettiği kural: **2-3 parametreye sert vur, geri kalanını sabit/yavaş tut.** Çok parametreye yayılmış zayıf modülasyon, görseli "titrek" yapar; sert tek-parametre modülasyonu görseli müzikle birleşmiş hissettirir.

Opal'in fabrika tuning'i:
- Bass → **scale** (form nefes alır)
- Onset → **brightness pump + palette swap** (drop hissi)
- High → **grain miktarı** (parlak material → daha doku)
- Mid → **hue shift** (yavaş, sürekli)

Diğer parametreler kullanıcının knob ile devreye soktuğu opsiyonel modülasyonlar.

---

## 7. Latency Bütçesi

| Aşama | Tipik | Açıklama |
|---|---|---|
| Audio buffer (256 sample @ 48kHz) | 5.3ms | DAW veya CoreAudio buffer |
| FFT pencere + hop (512/256 hop) | 5-10ms | 50% overlap |
| Envelope follower attack | 1-5ms | Bass için 1ms, full için 5ms |
| Atomik snapshot (audio→render) | <1ms | Lock-free |
| Render frame | 0-16.6ms | 60Hz VSync içinde nerede olduğuna göre |
| Monitor scanout + pixel response | 5-10ms | Apple display'lerde tipik |
| **TOPLAM** | **~25-45ms** | **Hedefin altında** ✅ |

**İnsan eşiği:** Konuşma için ±200ms tolerans, müzik için "tight" hissetmek <50ms ile garantili. Opal'in tipik latency'si bu bandın altında.

**VSync kararı:** Açık tutulur (tearing korkunç görünür). 60Hz monitör → frame-time 16.6ms. macOS'ta `CVDisplayLink` ile vblank-aligned render daha temiz; JUCE'nin OpenGL renderer'ı bunu zaten doğru yapıyor.

---

## 8. Build Sistemi ve Proje Yapısı

### 8.1 Klasör

```
Opal/
├── CMakeLists.txt
├── JUCE/                          # git submodule
├── source/                        # (bkz. §3.3 modül diyagramı)
├── resources/
│   ├── fonts/
│   └── shaders/                   # .glsl dosyaları
├── tests/
│   ├── CMakeLists.txt
│   └── AnalyzerTests.cpp          # Catch2
├── cmake/
└── .github/workflows/build.yml    # CI
```

### 8.2 CMake (özet)

```cmake
cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE INTERNAL "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(Opal VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(JUCE)

juce_add_plugin(Opal
    COMPANY_NAME "Edition8"
    BUNDLE_ID com.edition8.opal
    PLUGIN_MANUFACTURER_CODE Ed8C
    PLUGIN_CODE Opal
    FORMATS Standalone VST3
    PRODUCT_NAME "Opal"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_OPENGL TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(Opal)
target_sources(Opal PRIVATE ...)  # source files
juce_add_binary_data(OpalShaders SOURCES resources/shaders/main.glsl ...)
target_link_libraries(Opal PRIVATE
    OpalShaders
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl)
```

### 8.3 İlk Komutlar

```bash
git clone --recurse-submodules https://github.com/sudara/pamplejuce.git Opal
cd Opal
# Pamplejuce'in placeholder isimlerini Opal'a değiştir (bir .sh script ile geliyor)
cmake -B Builds -G Xcode
cmake --build Builds --config Debug --target Opal_Standalone
open Builds/Opal_artefacts/Debug/Standalone/Opal.app
```

---

## 9. Geliştirme Döngüsü

**Altın kural:** Ableton'a sadece milestone'larda dokun.

1. **Günlük döngü (saniyeler):** Standalone build → çalıştır → değiştir → derle → çalıştır.
2. **Haftalık döngü:** `pluginval --strictness-level 5` ile VST3 sağlık kontrolü.
3. **Milestone (2 haftada bir):** Ableton'da VST3 yükle, multiple instance, state save/recall, otomasyon testi.

**Shader iterasyonu:** Plugin/standalone çalışırken `resources/shaders/main.glsl`'ı düzenle → kaydet → ekranda canlı yenilenir (ShaderWatcher 250ms'de bir poll eder). Compile hatasında eski program korunur, hata mesajı log paneline yazılır.

**Multiple instance + state recall:** İlk hafta görmezden gel. v0.1'in son haftası bunlar için ayrıl — Pamplejuce'in `AudioProcessorValueTreeState` örneği zaten doğru yolu gösteriyor.

---

## 10. Riskler ve Bilinmeyenler

### 10.1 Yüksek risk (proje gidişatını etkileyebilir)

1. **macOS OpenGL 4.1 deprecation:** Apple resmi olarak 2018'de "ölü ilan etti" ama hâlâ çalışıyor. 2028'e kadar muhtemelen kalır; ama Apple yıllık WWDC'lerde sürpriz yapabilir. **Mitigation:** Shader'ları minimal/portable yaz; gerekirse v2'de Metal'e veya juce_bgfx'e geçmek "tek katmanı değiştirmek" olur.
2. **Ableton + OpenGL plugin** uyumsuzluk:** Forum'larda Live'da OpenGL plugin'lerin freeze/blank-screen yaptığına dair raporlar var (özellikle Windows + Intel iGPU). **Mitigation:** Erken Ableton testi (haftada bir). Software fallback toggle (basit JUCE Graphics gradient) acil kaçış olarak hazırlan.
3. **Birden fazla plugin instance, GPU context çakışması:** Aynı sette 2 Opal → 2 OpenGL context. Test edilmeli. **Mitigation:** Her instance kendi context'inde izole; resource lifecycle `newOpenGLContextCreated`/`closing` içinde net.

### 10.2 Orta risk

4. **Sample-accurate beat sync hassasiyeti:** `ppqPosition` çoğu host'ta doğru ama uç durumlarda jitter (tempo otomasyonu sırasında) görülebilir. **Mitigation:** Geçen blokta `ppqPosition` ile sample sayısından PPQ'yu predict et; ikisi sapıyorsa öncelik audio'ya değil predicted'e ver (yumuşatma).
5. **Knob otomasyonunda artefakt:** Knob hızla değişirse görsel "zıplar". **Mitigation:** GUI'de smoothing yok, ama shader uniform set ederken `juce::SmoothedValue<float>` ile 20-50ms lerp uygula.
6. **Universal binary build:** Apple Silicon + Intel her zaman ilk denemede temiz derlenmiyor. Pamplejuce bu konuda hazır ama yine de ilk kez yapan biri için 1-2 saatlik bir dolambaç olabilir.

### 10.3 "Kolay görünür, zor çıkar"

- **VST3 state recall:** "kaydet/yükle" basit görünür ama parametre versiyonlama, eski preset'lerin yeni format'ta açılması işi kolayca karmaşıklaşır. v0.1'de sadece "anki knob değerleri kaydet, yükle" yeterli.
- **Standalone'da audio input device seçimi:** JUCE'nin default UI'ı çirkin. v0.1'de görmezden gel; v0.2'de düzelt.
- **Shader binary embed vs dev hot-reload toggle:** `#ifdef OPAL_DEV_SHADERS` ile iki yolu temiz ayır. İlk gün karıştırırsan release build'de boş ekran görürsün ve neden olduğunu anlamak yarım gün gider.
- **Apple display'de gamma/color management:** OpenGL render'da renkler "sRGB değil" gibi durabilir. JUCE'de `setComponentPaintingEnabled(false)` ve sRGB framebuffer hint'i ayarlamak gerekebilir. Renk yönetimini ilk sürümde "düz linear" kabul edip ileride düzelt.

---

## 11. Açık Sorular — Murat'ın Karar Vermesi Gerekenler

Bu sorular netleşmeden Claude Code'a "git başla" demek erken. Her birine 1-2 cümle yanıt yeterli.

1. **Lisans:** AGPLv3 ile kaynak kodu GitHub'da public yapmaya hazır mısın? Yoksa private repo + GPL ihlali riski mi tercih edersin? (Public öneririm; öğrenme deneyimini paylaşmak da değerli.)
2. **AU formatı:** v0.1'de Logic Pro için AU eklemeyelim mi? +5 dakikalık iş ama test edilmeyecek = sorumluluk yaratır. (Önerim: VST3 + Standalone yeter, AU v0.2.)
3. **Hedef Ableton sürümü:** Hangi Live sürümünde test edeceksin? (Live 11? 12?) Bu, hangi VST3 SDK davranışlarını hedeflediğimizi etkiler.
4. **Pamplejuce template'i sana uygun mu?** Sudara'nın opinionated yapısı (Catch2 testler, GitHub Actions, vs.) öğrenirken faydalı mı, fazla mı? Alternatif: sıfırdan `juce_add_plugin` ile CMake — daha az dosya ama daha az hazırlık.
5. **8 knob'un isimleri/sıralaması:** Önerdiğim isimler/sıra (DRIVE → TEMP) sana çalışıyor mu? Bir knob başka bir parametreye bağlansın istiyor musun?
6. **Görsel stil — Risk:** Birinci sürümde "tek tip" görsel (tek shader). Bu seni sınırlar mı, yoksa "ilk olarak bunu çok iyi yapalım" diye düşünüyor musun?
7. **Standalone audio input:** Standalone modda kullanıcı sistem ses çıkışını (BlackHole / Soundflower / Loopback ile) loopback yapacak mı, yoksa hep bir input cihaz mı (mikrofon, interface)? Bu, default seçim ve UX'i etkiler.
8. **Hedef tarih:** Ne zamana kadar "Ableton'da çalışıyor, beat'e oturuyor" demek istiyorsun? Bu, kapsamı daraltma kararlarına yön verir.

---

## 12. Claude Code'a Hazırlık Notları

Bu spec'i Claude Code'a verirken aşağıdaki açıklamaları **belge eklerine** koy:

### 12.1 Önce Yapılacaklar (Claude Code için sıralı checklist)

1. Pamplejuce template'ini fork'la, `Opal` ismiyle yerelde clone'la. Placeholder rename script'ini çalıştır.
2. CMake projesini Xcode generator ile aç, **Standalone target'ı derle ve çalıştır** — sadece boş JUCE penceresi görmek bile başarı.
3. `JUCE/examples/GUI/OpenGLAppDemo.h` ve `JUCE/examples/Audio/SpectrumAnalyserTutorial_03.h` örneklerini studio gibi referans tut.
4. **Önce DSP'yi yaz, sonra görseli.** Sırayla:
   - `AnalysisFrame` struct → `LockFreeBus` (atomik snapshot).
   - `Analyzer` (FFT + 3-band envelope + RMS + spectral flux). Catch2 ile unit test yaz.
   - `PluginProcessor::processBlock` içinde `Analyzer`'ı çalıştır, `AnalysisFrame`'i bus'a yaz, ekranda printf-style debug overlay göster.
5. **Sonra GUI iskelet:** `KnobStrip` (8 knob, `AudioProcessorValueTreeState`'a bağlı), boş visualizer alanı.
6. **Sonra OpenGL:** `VisualizerComponent` (kendi `OpenGLContext`'i), basit fragment shader (sadece gradient + zaman).
7. **Sonra audio→shader bağlama:** Uniform'ları her render frame'inde `AnalysisFrame`'den oku, shader'a yaz.
8. **Sonra knob→shader bağlama:** 8 knob → 8 uniform.
9. **Sonra hot-reload:** `ShaderWatcher`. (Bu olmadan iterasyon yavaş; geç eklersen pişman olursun.)
10. **Sonra görsel motor:** SDF form + warp + grain + feedback katmanlarını sırayla shader'a ekle. Her katman audio'ya tepki vermeden tek başına doğru görünmeli, sonra audio bağlanır.
11. **Sonra host transport:** `getPlayHead()`, `PositionInfo`, BPM/ppq okuma.
12. **En son:** State save/recall, pluginval, Ableton testi.

### 12.2 Claude Code'a İletilecek Şarta Bağlı Notlar

- **C++ standardı:** C++20. `std::atomic`, `std::optional`, structured bindings serbest. Coroutine kullanma (gereksiz).
- **Audio thread'de yasaklılar:** `new`/`delete`, `std::vector::push_back`, `std::mutex`, `juce::String` (içinde allokasyon var), `cout/printf`. RT-safe değilse `processBlock` içinde olmasın.
- **Yorum dili:** İngilizce. Commit mesajları İngilizce. Belgeleme (varsa) Türkçe veya İngilizce — Murat seçecek.
- **Test stratejisi:** DSP modüllerinin (Analyzer, OnsetDetector) Catch2 birim testleri olmalı. GUI ve OpenGL kodu görsel doğrulama ile (manuel) test edilir.
- **Coding style:** JUCE Coding Standards (`https://juce.com/coding-style/`). `clang-format` ile Pamplejuce'in `.clang-format` dosyası kullanılır.
- **Bağımlılıklar:** Yalnızca JUCE 8 modülleri. Üçüncü parti kütüphane eklemeden önce Murat'a sor. (Catch2 zaten Pamplejuce'te.)
- **Loglama:** `DBG()` makrosu yeterli. Release'de strip edilir.

### 12.3 Belirsiz Kalan Teknik Detaylar (Claude Code keşfedecek)

- Tam shader uniform isim/tip listesi — implementasyon sırasında crystallize olur.
- `juce::SmoothedValue<float>` parametreleri (smoothing time) — kulak/göz kararı.
- Onset eşik değeri (median multiplier) — test edilerek tune edilir.
- Universal binary'nin Apple Silicon + Intel testleri — ilk derlemede ortaya çıkar.

---

## 13. Kaynaklar (Sıralı Tematik)

### Framework
- [JUCE Get JUCE — lisans sayfası](https://juce.com/get-juce/)
- [JUCE 8 What's New](https://juce.com/releases/whats-new/)
- [JUCE Forum — JUCE 8 license discussion](https://forum.juce.com/t/juce8-license-and-open-source-projects/60987)
- [iPlug2 (alternatif değerlendirme)](https://iplug2.github.io/)
- [Pamplejuce — Sudara](https://github.com/sudara/pamplejuce)

### OpenGL + JUCE
- [JUCE OpenGL Tutorial](https://juce.com/tutorials/tutorial_open_gl_application/)
- [JUCE OpenGLContext docs](https://docs.juce.com/master/classOpenGLContext.html)
- [JUCE OpenGLShaderProgram docs](https://docs.juce.com/master/classOpenGLShaderProgram.html)
- [James Johnson — OpenGL for 2D graphics in JUCE](https://medium.com/@Im_Jimmi/using-opengl-for-2d-graphics-in-a-juce-plug-in-24aa82f634ff)
- [JUCE Forum — OpenGLContext for plugins](https://forum.juce.com/t/openglcontext-for-plugins-attach-to-plugineditor-or-child-component/46795)
- [JUCE Forum — macOS OpenGL deprecation](https://forum.juce.com/t/what-are-others-doing-with-the-opengl-depreciation-on-macos/63091)
- [Apple OpenGL Capabilities Tables](https://developer.apple.com/opengl/OpenGL-Capabilities-Tables.pdf)

### Audio Analiz + Sync
- [JUCE PositionInfo API](https://docs.juce.com/master/classAudioPlayHead_1_1PositionInfo.html)
- [JUCE Spectrum Analyser Tutorial](https://docs.juce.com/master/tutorial_spectrum_analyser.html)
- [JUCE dsp::FFT](https://docs.juce.com/master/classdsp_1_1FFT.html)
- [Timur Doumler — Locks in real-time audio safely](https://timur.audio/using-locks-in-real-time-audio-processing-safely)
- [Aubio — onset detection methods](https://aubio.org/doc/latest/specdesc_8h.html)
- [BTrack — Adam Stark beat tracker](https://github.com/adamstark/BTrack)
- [Vatakis & Spence — AV synchrony perception](https://link.springer.com/article/10.3758/PP.70.6.955)

### Build + Dev Döngüsü
- [JUCE CMake API](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
- [Melatonin — How to use CMake with JUCE](https://melatonin.dev/blog/how-to-use-cmake-with-juce/)
- [Melatonin — Big List of JUCE Tips and Tricks](https://melatonin.dev/blog/big-list-of-juce-tips-and-tricks/)
- [pluginval (Tracktion)](https://github.com/Tracktion/pluginval)
- [Sudara — awesome-juce](https://github.com/sudara/awesome-juce)

### Görsel Stil + Shader Teknik
- [Iñigo Quilez — distance functions](https://iquilezles.org/articles/distfunctions/)
- [Iñigo Quilez — smin operatörü](https://iquilezles.org/articles/smin/)
- [Iñigo Quilez — domain warping](https://iquilezles.org/articles/warp/)
- [The Book of Shaders](https://thebookofshaders.com/)
- [Codrops — Risograph grain light in three.js](https://tympanus.net/codrops/2022/03/07/creating-a-risograph-grain-light-effect-in-three-js/)
- [butterchurn — WebGL Milkdrop](https://github.com/jberg/butterchurn) (audio mapping doktrin referansı)
- [Hydra Synth](https://hydra.ojack.xyz/) (feedback mimari referansı)
- [Teenage Engineering OP-1](https://teenage.engineering/products/op-1)
- [Brian Eno — 77 Million Paintings](https://en.wikipedia.org/wiki/77_Million_Paintings)

### Referans Açık Kaynak Projeler
- [TimArt/3DAudioVisualizers (JUCE + OpenGL)](https://github.com/TimArt/3DAudioVisualizers)
- [COx2/glslEditor_AudioPlugin (JUCE plugin'de canlı shader)](https://github.com/COx2/glslEditor_AudioPlugin)
- [JanosGit/OpenGLRealtimeVisualization4JUCE](https://github.com/JanosGit/OpenGLRealtimeVisualization4JUCE)
- [mtytel/vital (JUCE + Visage SDF graphics)](https://github.com/mtytel/vital)

---

**Sonraki adım:** §11'deki 8 açık soruyu cevapla. Ardından bu belge "frozen v1.0" olarak işaretlenir ve Claude Code'a §12'deki sırayı uygulamak üzere devredilir.

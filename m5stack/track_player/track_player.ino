// track_player.ino — track.html の M5Stack CoreS3 移植（方式A: ステム再生方式）
//
// ステムWAV（2小節ループ・22.05kHz・16bit・モノラル）を stems_data.h として
// ファームウェアに埋め込み、6トラックをサンプル単位で加算ミックスして再生する。
// stems_data.h は gen_stems_header.py で生成する（SDカード不要）。
//   - 行頭タップ = ミュート抜き差し
//   - 下部スライダー = マスターLPF（本家パフォーマンスFX①）
//   - 上部タブ = 曲切り替え / ▶ = 再生・停止
//   - 4周目 = ハイハット・スネアを自動で抜くブレイク（本家の簡易再現）
//
// 環境: Arduino IDE / ボード M5CoreS3 / ライブラリ M5Unified
// アプリ領域6MBの partitions.csv を同梱（埋め込みステム約3MBのため）

#include <M5Unified.h>
#include <math.h>
#include "stems_data.h"

constexpr uint32_t SR    = 22050;
constexpr int      BLOCK = 128;
constexpr int      NTRK  = 6;
constexpr int      NSONG = 3;
constexpr int      STEPS = 32;

const char* SONG_ID[NSONG]   = { "techno", "lofi", "phonk" };
const char* SONG_NAME[NSONG] = { "TECHNO", "LO-FI", "PHONK" };

// 表示順は本家TRACKSと同じ（上からハット/スネア/キック/ベース/リード/パッド）
const char* PART[NTRK]     = { "hihat", "snare", "kick", "bass", "lead", "pad" };
const char* TRK_NAME[NTRK] = { "HAT", "SNR", "KCK", "BAS", "LED", "PAD" };
const uint8_t TRK_RGB[NTRK][3] = {
  { 0x94, 0xA3, 0xB8 }, { 0xF4, 0x72, 0xB6 }, { 0xF5, 0x9E, 0x0B },
  { 0x0D, 0x94, 0x88 }, { 0x7C, 0x3A, 0xED }, { 0x60, 0xA5, 0xFA },
};

// 表示用パターン（本家SONGSから転記）: '.'=なし 'x'=通常 'g'=ゴースト 'o'=オープン 'r'=ロール
const char* PATTERN[NSONG][NTRK] = {
  { // techno
    "..ox..o...ox..o...ox..o...ox.xox",
    "....x.......x..g....x.......x.g.",
    "x...x...x...x...x...x...x...x...",
    "..x...xx..x..xx...x...xx..x..xxx",
    "......x...............x.......x.",
    "x...............................",
  },
  { // lofi
    "x.x.x.xxx.x.x.x.x.x.x.x.x.xxx.o.",
    "....x......gx.......x.......x..g",
    "x......x..x.....x.....x...x.....",
    "x......x..x...x.x......x..x...x.",
    "..x..x....x.x.....x..x..x.x..x..",
    "x...............x...............",
  },
  { // phonk
    "x.x.x.r.x.x.xxx.x.x.x.x.x.r.x.xx",
    "........x...............x.....g.",
    "x.........x.....x.....x..x......",
    "x.........x.....x.....x..x....x.",
    "....x.......x.....x.....x.......",
    "x...............x...............",
  },
};

struct Song {
  const int16_t* stem[NTRK] = { nullptr };
  uint32_t len = 0;                    // ループ長（サンプル数）
};
Song songs[NSONG];

volatile bool     playing   = false;
volatile int      curSong   = 0;
volatile bool     muted[NTRK] = { false };
volatile uint32_t playPos   = 0;
volatile uint32_t loopCount = 0;

// ---------------------------------------------------------------- master LPF
// RBJ lowpass (Q=0.9)。本家と同じ 対数マッピング（上限はナイキストに合わせ10kHz）
volatile float cutPos = 1.0f;
float b0c, b1c, b2c, a1c, a2c;
float z1 = 0, z2 = 0;

float cutoffHz(float pos) { return 120.0f * powf(10000.0f / 120.0f, pos); }

void setFilter(float pos) {
  float w     = 2.0f * (float)M_PI * cutoffHz(pos) / SR;
  float alpha = sinf(w) / (2.0f * 0.9f);
  float cw    = cosf(w);
  float a0    = 1.0f + alpha;
  b0c = ((1.0f - cw) * 0.5f) / a0;
  b1c = (1.0f - cw) / a0;
  b2c = b0c;
  a1c = (-2.0f * cw) / a0;
  a2c = (1.0f - alpha) / a0;
}

// ---------------------------------------------------------------- audio task
void audioTask(void*) {
  // スピーカーのキュー（最大2ブロック）が参照中のバッファに書き込まないよう
  // 4枚のリングで回す（2枚だと再生中のバッファを上書きしてノイズになる）
  constexpr int NBUF = 4;
  static int16_t out[NBUF][BLOCK];
  int bufIdx = 0;
  for (;;) {
    if (!playing) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    Song& s = songs[curSong];
    if (!s.len) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    int16_t* dst = out[bufIdx];
    for (int n = 0; n < BLOCK; n++) {
      uint32_t pos = playPos;
      if (pos >= s.len) { pos = 0; playPos = 0; }
      bool brk = ((loopCount & 3) == 3);           // 4周目=ブレイク
      int32_t acc = 0;
      for (int t = 0; t < NTRK; t++) {
        if (muted[t] || !s.stem[t]) continue;
        if (brk && (t == 0 || t == 1)) continue;   // ハット・スネアを抜く
        acc += s.stem[t][pos];
      }
      float x = acc * (0.8f / 32768.0f);           // ヘッドルーム
      float y = b0c * x + z1;                      // transposed DF2
      z1 = b1c * x - a1c * y + z2;
      z2 = b2c * x - a2c * y;
      int32_t v = (int32_t)(y * 32767.0f);
      if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
      dst[n] = (int16_t)v;
      if (++playPos >= s.len) { playPos = 0; loopCount++; }
    }
    while (!M5.Speaker.playRaw(dst, BLOCK, SR, false, 1, 0)) vTaskDelay(1);
    bufIdx = (bufIdx + 1) % NBUF;
  }
}

// ---------------------------------------------------------------- UI
// レイアウト: 上部タブ(y0-26) / グリッド(y36-168) / スライダー(y190-224)
constexpr int GRID_X = 48, GRID_Y = 36, CELL_W = 8, ROW_H = 22;
uint16_t trkColor[NTRK];
int shownStep = -1;

void drawCell(int t, int i, bool head) {
  int x = GRID_X + i * CELL_W, y = GRID_Y + t * ROW_H;
  char c = PATTERN[curSong][t][i];
  uint16_t col = (c == '.') ? TFT_WHITE : trkColor[t];
  if (muted[t] && c != '.') col = M5.Display.color565(200, 203, 210);
  M5.Display.fillRect(x, y, CELL_W - 1, ROW_H - 4, col);
  if (c == 'g') M5.Display.fillRect(x + 2, y + 2, CELL_W - 5, ROW_H - 8, TFT_WHITE);  // ゴースト=薄く
  if (head) M5.Display.drawRect(x, y, CELL_W - 1, ROW_H - 4, M5.Display.color565(217, 119, 6));
  else if (i % 4 == 0) M5.Display.drawFastVLine(x, y, ROW_H - 4, M5.Display.color565(198, 203, 216));
}

void drawRow(int t) {
  int y = GRID_Y + t * ROW_H;
  M5.Display.fillRect(0, y, GRID_X - 2, ROW_H - 4, muted[t] ? M5.Display.color565(58, 63, 82) : trkColor[t]);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(muted[t] ? "MUT" : TRK_NAME[t], 6, y + (ROW_H - 4) / 2);
  for (int i = 0; i < STEPS; i++) drawCell(t, i, i == shownStep);
}

void drawTabs() {
  for (int s = 0; s < NSONG; s++) {
    int x = 8 + s * 72;
    bool on = (s == curSong);
    M5.Display.fillRoundRect(x, 3, 66, 22, 5, on ? M5.Display.color565(13, 148, 136) : TFT_WHITE);
    M5.Display.drawRoundRect(x, 3, 66, 22, 5, M5.Display.color565(13, 148, 136));
    M5.Display.setTextColor(on ? TFT_WHITE : M5.Display.color565(31, 36, 48));
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(SONG_NAME[s], x + 33, 14);
  }
  M5.Display.fillRoundRect(240, 3, 72, 22, 5, playing ? M5.Display.color565(217, 119, 6) : M5.Display.color565(13, 148, 136));
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(playing ? "STOP" : "PLAY", 276, 14);
}

void drawSlider() {
  M5.Display.fillRect(0, 186, 320, 54, TFT_WHITE);
  M5.Display.setTextColor(M5.Display.color565(92, 98, 112));
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString("FILTER", 8, 205);
  M5.Display.fillRoundRect(60, 201, 250, 8, 4, M5.Display.color565(216, 220, 228));
  int kx = 60 + (int)(cutPos * 240);
  M5.Display.fillCircle(kx + 5, 205, 10, M5.Display.color565(13, 148, 136));
  M5.Display.setTextDatum(middle_right);
  char buf[16];
  if (cutPos > 0.98f) strcpy(buf, "OPEN");
  else snprintf(buf, sizeof(buf), "%d Hz", (int)cutoffHz(cutPos));
  M5.Display.drawString(buf, 312, 226);
}

void drawAll() {
  M5.Display.fillScreen(TFT_WHITE);
  drawTabs();
  for (int t = 0; t < NTRK; t++) drawRow(t);
  drawSlider();
}

// ---------------------------------------------------------------- setup/loop
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  {
    // 出力を22.05kHzの整数倍にして再サンプリングの粗れを抑える
    M5.Speaker.end();
    auto spk = M5.Speaker.config();
    spk.sample_rate = 44100;
    M5.Speaker.config(spk);
    M5.Speaker.begin();
  }
  M5.Speaker.setVolume(190);
  for (int t = 0; t < NTRK; t++) trkColor[t] = M5.Display.color565(TRK_RGB[t][0], TRK_RGB[t][1], TRK_RGB[t][2]);

  M5.Display.setTextSize(1);

  // 埋め込みステム（stems_data.h）をテーブルに展開
  for (int s = 0; s < NSONG; s++) {
    for (int t = 0; t < NTRK; t++) {
      songs[s].stem[t] = STEMS[s][t].data;
      if (STEMS[s][t].len > songs[s].len) songs[s].len = STEMS[s][t].len;
    }
  }

  setFilter(cutPos);
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 5, nullptr, 1);
  drawAll();
}

void loop() {
  M5.update();
  auto tp = M5.Touch.getDetail();

  if (tp.wasPressed()) {
    int x = tp.x, y = tp.y;
    if (y < 30) {
      if (x >= 240) {                          // PLAY/STOP
        playing = !playing;
        if (!playing) { playPos = 0; loopCount = 0; z1 = z2 = 0; shownStep = -1; }
        drawAll();
      } else if (x >= 8 && x < 8 + NSONG * 72) {   // 曲タブ
        int s = (x - 8) / 72;
        if (s < NSONG && s != curSong) { curSong = s; playPos = 0; loopCount = 0; drawAll(); }
      }
    } else if (y >= GRID_Y && y < GRID_Y + NTRK * ROW_H && x < GRID_X) {  // 行頭=ミュート
      int t = (y - GRID_Y) / ROW_H;
      muted[t] = !muted[t];
      drawRow(t);
    }
  }
  if (tp.isPressed() && tp.y >= 186) {         // スライダー（ドラッグ対応）
    float p = (tp.x - 60) / 240.0f;
    if (p < 0) p = 0; if (p > 1) p = 1;
    cutPos = p;
    setFilter(p);
    drawSlider();
  }

  // 再生ヘッドの更新（変わった列だけ再描画）
  if (playing && songs[curSong].len) {
    int step = (int)((uint64_t)playPos * STEPS / songs[curSong].len);
    if (step != shownStep) {
      int prev = shownStep;
      shownStep = step;
      for (int t = 0; t < NTRK; t++) {
        if (prev >= 0) drawCell(t, prev, false);
        drawCell(t, step, true);
      }
    }
  }
  delay(10);
}

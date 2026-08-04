# track.html の M5Stack CoreS3 移植（方式A: ステム再生方式）

track.html のマルチトラックデモを M5Stack CoreS3 で動かすための一式です。
Tone.js のシンセ・エフェクトをマイコンで再現するのではなく、**ブラウザで各トラックを
2小節ループのWAV（ステム）に焼き固め、実機は「6トラック同時再生＋ミュート＋
マスターフィルター」に徹する**構成です。音質はブラウザ版と同一になります。

## 必要なもの

- M5Stack CoreS3（または CoreS3 SE。スピーカー・タッチパネル必須。SDカードは不要）
- Arduino IDE または arduino-cli（ボード: M5CoreS3）＋ ライブラリ `M5Unified`

## 仕組み

ステムWAVは `gen_stems_header.py` で C ヘッダ（`stems_data.h`、約3.5MBのサンプルデータ）に
変換し、**ファームウェアに埋め込み**ます。埋め込み後のバイナリは約4MBになるため、
アプリ領域6MBのカスタムパーティション（同梱の `partitions.csv`）を使います。

## 手順

### 1. ステムを用意する

書き出し済みのステム18本が `m5stack/stems/` に入っているので、曲データを変えない限り
この手順は不要。曲を変えたときは [track.html](https://takaharukanai.github.io/digital-sound-lab/track.html)
の **「M5Stack用ループ書き出しモード」** にチェックを入れ、各曲で6トラックぶんの
**STEM(.wav)** ボタンを押して書き出し、`m5stack/stems/` を差し替える
（`techno_kick.wav` 形式・2小節ループ・22.05kHz・16bit・モノラル）。

### 2. ヘッダを生成してビルド・書き込み

```bash
cd m5stack/track_player
python3 gen_stems_header.py ../stems
arduino-cli compile -b m5stack:esp32:m5stack_cores3:PartitionScheme=custom .
arduino-cli upload  -b m5stack:esp32:m5stack_cores3:PartitionScheme=custom -p <シリアルポート> .
```

Arduino IDE の場合は「ツール → Partition Scheme → Custom」を選択
（スケッチフォルダの `partitions.csv` が使われる）。

## 操作

- **上部タブ**: 曲切り替え（TECHNO / LO-FI / PHONK）
- **▶ / ■**: 再生・停止
- **行頭のトラック名タップ**: ミュートの抜き差し（＝ステム方式のインタラクション）
- **下部スライダー**: マスターローパスフィルター（本家のパフォーマンスFX①。
  DJの「フィルター落とし→開放」）
- **4周目**: ハイハットとスネアが自動で抜けるブレイク（本家の簡易再現）

## 本家との差分（仕様）

- BPM・音色・スケールは変更不可（WAVに焼き込み済みのため）
- パフォーマンスFX②（リードのディレイ量）は書き出し時の値 0.32 で固定
- ブレイク終盤のスネア4連の煽りは省略（シンセによるリアルタイム発音のため）
- WAV書き出し機能は実機側にはなし

## 拡張のヒント

- Grove端子にボリューム/距離センサーを繋ぎ、読み値を `cutPos`（0.0〜1.0）に
  入れれば「センサーで質感を演奏する」インタラクションになる
- 曲を差し替えたいときは track.html の `SONGS` を編集 → ループ書き出し →
  `gen_stems_header.py` → 再ビルド（表示パターンを変えた場合はスケッチの
  `PATTERN` も合わせて更新する）

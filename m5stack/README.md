# track.html の M5Stack CoreS3 移植（方式A: ステム再生方式）

track.html のマルチトラックデモを M5Stack CoreS3 で動かすための一式です。
Tone.js のシンセ・エフェクトをマイコンで再現するのではなく、**ブラウザで各トラックを
2小節ループのWAV（ステム）に焼き固め、実機は「6トラック同時再生＋ミュート＋
マスターフィルター」に徹する**構成です。音質はブラウザ版と同一になります。

## 必要なもの

- M5Stack CoreS3（または CoreS3 SE。スピーカー・タッチパネル・SDスロット必須）
- microSDカード
- Arduino IDE（ボード: M5CoreS3、PSRAM: OPI PSRAM 有効）＋ ライブラリ `M5Unified`

## 手順

### 1. ブラウザでステムを書き出す

1. [track.html](https://takaharukanai.github.io/digital-sound-lab/track.html) を開く
2. ページ下部の **「M5Stack用ループ書き出しモード」** にチェックを入れる
3. 曲を選び、6トラックそれぞれの **STEM(.wav)** ボタンを押す
   （`techno_kick.wav` のような名前で 2小節ループ・22.05kHz・16bit・モノラル の WAV が落ちる）
4. 3曲すべてで繰り返す（6トラック × 3曲 = 18ファイル。ブラウザが連続ダウンロードの
   許可を求めたら許可する）

### 2. SDカードに配置

SDカード直下に `stems` フォルダを作り、18ファイルをそのまま入れる:

```
/stems/techno_kick.wav
/stems/techno_snare.wav
/stems/techno_hihat.wav
/stems/techno_bass.wav
/stems/techno_lead.wav
/stems/techno_pad.wav
/stems/lofi_….wav
/stems/phonk_….wav
```

### 3. スケッチを書き込む

`track_player/track_player.ino` を Arduino IDE で開いて CoreS3 に書き込み。
起動時に全ステム（約3MB）を PSRAM に読み込みます。

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
- 曲を差し替えたいときは track.html の `SONGS` を編集 → ループ書き出し → SD更新

> **注意**: スケッチは実機未検証のたたき台です。M5Unified のバージョンによっては
> `playRaw` まわりの調整が必要になる場合があります。

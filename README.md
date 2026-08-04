# デジタル音源ラボ — 触って理解するインタラクティブ教材

MIDI・シンセサイザー・サンプラーの原理を、ブラウザだけで音を鳴らしながら学ぶ講義用教材集です。ビルド不要・依存ライブラリなしの静的HTMLで、Web Audio API / Web MIDI API を使用しています。

## 構成

| ファイル | 内容 |
|---|---|
| `index.html` | トップページ（3教材のインデックス） |
| `midi.html` | LAB 01: MIDIを触って理解する — メッセージ・CC・ベロシティ |
| `synth.html` | LAB 02: 波形生成を触って理解する — オシレーター・LFO・ADSR |
| `sampler.html` | LAB 03: サンプラーを触って理解する — 録音・スライス・モジュレーション・Web MIDI・WAV書き出し |

## ローカルで動かす

`index.html` をダブルクリックするだけでも動きますが、マイク録音・Web MIDIはローカルサーバー経由（または https）を推奨します。

```bash
python3 -m http.server 8000
# → http://localhost:8000
```

## GitHub Pages で公開する

1. GitHubで新しいリポジトリを作成（例: `digital-sound-lab`、Public）
2. このフォルダの中身をリポジトリのルートにプッシュ

   ```bash
   git init
   git add .
   git commit -m "Add interactive sound source learning materials"
   git branch -M main
   git remote add origin https://github.com/<ユーザー名>/digital-sound-lab.git
   git push -u origin main
   ```

3. リポジトリの **Settings → Pages** を開き、
   - Source: **Deploy from a branch**
   - Branch: **main** / **/(root)** → Save
4. 数分後、`https://<ユーザー名>.github.io/digital-sound-lab/` で公開されます

GitHub Pages は https 配信のため、マイク録音・Web MIDI もそのまま動作します。

## 動作環境

- Chrome / Edge 推奨（Web MIDI 対応のため）
- Firefox / Safari でも音は出ます（Web MIDI は非対応または要設定）
- マイク録音・MIDI接続はページ上のボタン押下時にブラウザが権限を確認します

## ライセンス

必要に応じて追記してください（教材配布なら MIT や CC BY を推奨）。

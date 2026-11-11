# Spectral Delay

周波数ごとにディレイタイム・フィードバック・音量・パンを描けるスペクトラルディレイ(VST3 / AU、Mac)。

- FFT 2048 / hop 512(overlap 4)、レイテンシ 2048 サンプル
- カーブ4本(64点・対数周波数軸)、Time / Feedback / Dry/Wet / Shift / Freeze
- 白黒だけのシンプルな UI

## ビルド
GitHub に push すると、GitHub Actions(`.github/workflows/build.yml`)が Mac 用(Apple Silicon + Intel)をビルドし、
テスト(DSP テスト・DAW と同じ方法での読み込みテスト・auval)を通ったものを `SpectralDelay-mac` として保存します。

## 構成
- `Source/SpectralDelayCore.h` … 音の処理(JUCE に依存しない)
- `Source/PluginProcessor.*` … パラメータ・保存・Live へのレイテンシ報告
- `Source/PluginEditor.*` … 画面
- `tests/` … テスト

JUCE 8(AGPLv3)を使用しています。



---

# README.md

# LUMINA - Advanced Spectral Mastering Tool

**LUMINA** is a high-end spectral mastering plugin designed for precision dynamic shaping and tonal balancing. By combining advanced Spectral HPSS (Harmonic Percussive Source Separation) with modern PT/ZDF-style dynamics, LUMINA offers unparalleled transparency and control over your master bus.

---

## Features

### 1. High-Resolution Spectral Engine
* **Intelligent Spectral Shaping:** Uses high-resolution FFT analysis to target specific frequency components without traditional crossover artifacts.
* **HPSS Integration:** Separates tonal and transient components within each band, allowing for independent control over "Tonal Shift" and "Transient Shift."

### 2. Multi-Band Dynamics & M/S Processing
* **3-Band Architecture:** Independent processing for Low, Mid, and High bands with adjustable crossovers.
* **True M/S Workflow:** Full Mid/Side support for surgical stereo image control.
* **PT/ZDF Dynamics:** Zero-delay feedback style envelope followers provide a smooth, analog-like response in a digital spectral domain.

### 3. Professional Workflow Tools
* **Auto-Band:** Automatically suggests optimal crossover frequencies based on the input signal's spectral distribution.
* **Auto-Level:** Intelligent loudness matching to ensure objective A/B comparisons.
* **Delta Monitoring:** Listen exclusively to the signal being removed or processed.

### 4. High-End Visualizer
* **Glow-Spectrum:** A beautiful, responsive spectral analyzer with a +3dB/octave tilt for intuitive mastering-grade monitoring.
* **Dynamic GR Meters:** High-visibility Gain Reduction meters with dynamic opacity (Mid: White, Side: Sakura-pink).

---

## Compatibility & Requirements
* **Platform:** Windows Only (VST3 / Standalone).
* **Validation:** Tested and verified exclusively on **Ableton Live**. Operation in other DAWs is not guaranteed.
* **Framework:** Built with **JUCE**.

---

## Disclaimer

**USE AT YOUR OWN RISK.**
The author shall not be held liable for any damages occurring from the use of this software. 
* **Hearing Protection:** Mastering involves significant gain changes. Please monitor at safe levels.
* **Equipment Protection:** Improper settings may produce high-energy signals. Ensure your speakers and headphones are protected by a limiter or kept at reasonable volumes.
The user assumes full responsibility for any damage to hearing or hardware.

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.  
LUMINA is distributed as **Freeware**. You are free to use, study, and share the software.

**Third-Party Licenses:** * This software uses the **JUCE** framework (GPLv3/Personal License).  
* Refer to the `LICENSE` file for full terms and conditions.

---
---

# LUMINA - 次世代スペクトラル・マスタリング・ツール

**LUMINA**は、精密なダイナミック・シェイピングとトーン・バランスの調整のために設計されたハイエンド・スペクトラル・マスタリング・プラグインです。最先端のスペクトラルHPSS（調波・打楽器音分離）技術とモダンなPT/ZDFスタイルのダイナミクスを融合させ、マスターバスにおいて比類なき透明感とコントロールを提供します。

---

## 主な特徴

### 1. 高解像度スペクトラル・エンジン
* **インテリジェント・シェイピング:** 高解像度FFT解析により、従来のクロスオーバーに伴うアーティファクトを排除し、特定の周波数成分を的確にターゲットします。
* **HPSS統合:** 各帯域内でトナール（持続音）成分とトランジェント（短音）成分を分離。独立した「Tonal Shift」および「Transient Shift」により、楽曲の骨組みを自在に操れます。

### 2. マルチバンド・ダイナミクス & M/S 処理
* **3バンド・アーキテクチャ:** 低域・中域・高域を独立して処理可能。クロスオーバー周波数は柔軟に変更できます。
* **M/Sワークフロー:** Mid/Side処理に完全対応。ステレオイメージの外科的な補正が可能です。
* **PT/ZDFダイナミクス:** ゼロディレイ・フィードバック（ZDF）スタイルのエンベロープ・フォロワーを採用。スペクトラル処理でありながら、アナログのようなしなやかな応答を実現しました。

### 3. プロフェッショナル・ワークフロー
* **Auto-Band:** 入力信号のスペクトル分布を解析し、最適なクロスオーバー周波数を自動的に提案します。
* **Auto-Level:** 知覚的なラウドネス・マッチングを行い、バイパス時との客観的な比較を支援します。
* **Delta Monitoring:** 抑制された成分（差分信号）のみをモニターし、削りすぎを防ぎます。

### 4. ハイエンド・ビジュアライザー
* **Glow-Spectrum:** +3dB/octのチルト補正を施した、視認性の高い発光型アナライザー。マスタリングに必要な「フラットな視点」を提供します。
* **Dynamic GR Meters:** リダクション量に応じて濃度が変化する高精度メーター（Mid: 白、Side: 桜色）を搭載。

---

## 互換性・動作環境
* **プラットフォーム:** Windows専用 (VST3 / Standalone)
* **検証済みホスト:** **Ableton Live** のみで動作検証を行っています。その他のDAWでの動作は保証されません。
* **フレームワーク:** **JUCE**を使用して開発されています。

---

## 免責事項

**本ソフトウェアの使用は自己責任で行ってください。** 本ソフトウェアの使用によって生じたあらゆる損害について、作者は一切の責任を負いません。
* **聴覚の保護:** マスタリング処理では大きな音量変化を伴うことがあります。常に安全な音量でモニターしてください。
* **機器の保護:** 設定によっては高エネルギーの信号が出力される可能性があります。スピーカーやヘッドフォンを保護するため、後段にリミッターを設置するか、適切な音量を維持してください。
聴覚およびハードウェアへの損傷に関する全責任はユーザーに帰属します。

---

## ライセンス

本プロジェクトは **GNU General Public License v3.0 (GPLv3)** の下でライセンスされています。  
LUMINAは **フリーウェア** として配布されます。ソフトウェアの使用、研究、共有は自由です。

**サードパーティ・ライセンス:** * 本ソフトウェアは **JUCE** フレームワーク（GPLv3/Personalライセンス）を使用しています。
* 詳細な利用規約については、同梱の `LICENSE` ファイルを参照してください。

---


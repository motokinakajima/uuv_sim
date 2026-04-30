# `main.cpp` の Python 連携メモ

このプロジェクトの現在の `main.cpp` は、Python から直接関数を呼ぶ形ではなく、**実行ファイルにコマンドライン引数を渡して起動する**形になっています。Python 側は `subprocess.run([...])` でこの実行ファイルを呼び、必要な結果を stdout や生成ファイルから受け取る想定です。

## 実行モード

### 1. 通常モード

`--batch-run` を付けない場合は、`behavior_params.cfg` を読み込み、固定の初期エージェントを使ってシミュレーションを走らせます。

このモードでは、最終的に `simulation_data.json` を出力します。中身は以下です。

- `simulation.field_type`
- `simulation.num_gaussians`
- `simulation.gaussians`
- `simulation.timesteps`

`timesteps` の各要素には、時刻ごとの各エージェントの `x`, `y`, `vx`, `vy`, `ax`, `ay` が入ります。

### 2. バッチモード

`--batch-run` を付けると、実験用の短い/長い繰り返し評価を行い、**結果を 1 行の JSON として stdout に出力**します。

このモードは Python から結果を回収しやすいです。

## Python から渡すデータ形式

現在は JSON ファイルや stdin を読む実装ではなく、**コマンドライン引数**で渡します。

### 単純な値

以下は `--key value` の形式です。

- `--agent-count 3`
- `--max-steps 7500`
- `--window 150`
- `--hold 150`
- `--num-gaussians 20`
- `--seed 1`
- `--field-seed 20260407`
- `--eps-v 0.03`
- `--eps-f 0.0005`
- `--use-speed-check true`
- `--init-pos-range 20.0`
- `--init-vel-range 1.0`

### decision tree の指定

木は `--tree-node` を複数回渡して表現します。`main.cpp` はこれをまとめて読み、木構造に復元します。

#### 内部ノード

形式:

```text
id,split,feature,threshold,left_id,right_id
```

例:

```text
0,split,speed,0.8,1,2
```

意味:

- `feature` が `threshold` より小さいなら `left_id`
- それ以外なら `right_id`

#### 葉ノード

形式:

```text
id,leaf,a0,a1,a2,a3
```

`a0` 〜 `a3` は 4 つのゲインに対するアクションです。

- `-1`, `decrease`, `dec`, `down` = 下げる
- `0`, `hold`, `stay`, `keep` = そのまま
- `1`, `increase`, `inc`, `up` = 上げる

4 つのゲインの順番は次の通りです。

1. `avoidance_gain`
2. `quark_gain`
3. `directional_derivative_gain`
4. `linear_drag_gain`

#### ルート指定

`--tree-root <id>` を指定すると、そのノードを根として使います。省略時は `0` が根です。

### 木を省略した場合

`--tree-node` が 1 つもない場合は、`main.cpp` 内蔵のデフォルト木が使われます。

## 返ってくる結果

### バッチモードの stdout JSON

バッチモードでは、実行結果が 1 つの JSON オブジェクトとして stdout に出ます。主なキーは次の通りです。

- `agent_count`
- `seed`
- `field_seed`
- `max_steps`
- `steps_executed`
- `converged`
- `converge_step`
- `best_field_final`
- `mean_field_final`
- `best_field_min_over_run`
- `improvement_from_start`
- `swarm_radius_final`
- `avg_speed_final`
- `runtime_ms`

### 非バッチモードのファイル出力

通常モードでは、結果は標準出力のメッセージに加えて、`simulation_data.json` に保存されます。

## 深さ3 GA スクリプト

このリポジトリには、深さ3固定の decision tree を GA で探索する Python スクリプトも追加してあります。

ファイル:

- `tree_ga_depth3.py`

このスクリプトは `uuv_sim --batch-run` を評価関数として使い、世代ごとに木を進化させます。

### できること

- 深さ3に達したノードを自動で葉として扱う
- 内部ノードは `feature` と `threshold` を mutation する
- 葉ノードは 4 つの出力列を個別に mutation する
- 出力列は `-1 / 0 / 1` の 3 値をとる
- 子世代の作成時に subtree crossover を使う
- 1 個体あたりの評価は 10 個の field seed × 10 個の trial の平均で行う

### 評価の考え方

GA の 1 個体は、以下の 100 回の batch 実行で評価されます。

- field seed を 10 個使う
- 各 field seed ごとに 10 trial 回す
- スコアは全 trial の平均を使う

field seed の基準値は `--field-seed` で渡し、そこから連番で 10 個の場を作ります。

### 実行例

```bash
python3 tree_ga_depth3.py \
    --population 24 \
    --generations 20 \
    --field-count 10 \
    --trials-per-field 10 \
    --workers 4 \
    --skip-build
```

ラッパーを使うなら次のように実行できます。

```bash
./run_tree_ga_depth3.sh --population 24 --generations 20 --workers 4
```

Windows なら次です。

```bat
run_tree_ga_depth3.bat --population 24 --generations 20 --workers 4
```

手で 10x10 を明示したいときの最小例はこれです。

```bash
python3 tree_ga_depth3.py \
    --population 24 \
    --generations 20 \
    --field-count 10 \
    --trials-per-field 10 \
    --workers 4
```

### 主要な出力

- `best_tree_depth3.txt`: 最良個体の `--tree-node` 互換ファイル
- `best_tree_depth3.mmd`: Mermaid 形式の可視化ファイル
- `tree_ga_log.csv`: 各世代の上位候補ログと、1 世代 1 行の集計ログ
- 標準出力: 世代ごとの best score と、最後に最良木の要約と JSON summary

### 仕組みのメモ

- 木は内部的に完全二分木として保持し、深さ3以降は自動で leaf になります。
- Python 側は `tree_ga_depth3.py` で木を生成し、そのまま C++ 側へ CLI 引数として渡します。
- 最良個体は `.txt` と `.mmd` の両方で保存されるので、VS Code の Mermaid プレビューなどでそのまま見られます。
- 評価指標は今のところ、`improvement_from_start` を主軸に、収束ボーナスや速度・実行時間の軽いペナルティを足したものです。
- ひとつの field だけに過適合しにくくするため、複数 field と複数 trial の平均を取ります。

## Validation

学習済みのツリーを固定して、同じ条件で何度も回す validation 用スクリプトもあります。

ファイル:

- `validate_tree_depth3.py`

デフォルトでは `best_tree_depth3.txt` を読み、**seed=42 固定**かつ **field_seed=42 固定**で 4500 回 batch 実行し、各 run の summary を 1 行ずつ CSV に保存します。

### 実行例

```bash
python3 validate_tree_depth3.py \
    --runs 4500 \
    --tree-file best_tree_depth3.txt \
    --output-csv tree_validation_runs.csv \
    --skip-build
```

ラッパーを使うならこちらです。

```bash
./run_validate_tree_depth3.sh
```

Windows ならこちらです。

```bat
run_validate_tree_depth3.bat
```

### CSV に入る主な列

- `run_id`, `seed`, `field_seed`
- `converged`, `converge_step`
- `best_field_final`, `mean_field_final`, `best_field_min_over_run`
- `improvement_from_start`, `swarm_radius_final`, `avg_speed_final`
- `runtime_ms`, `run_seconds`

この CSV をそのまま使って、あとで `converged` と `best_field_final` の散布図などを作れます。

## Python からの実行例

### 例 1: バッチ実行して JSON を読む

```python
import json
import subprocess

cmd = [
    "./build/uuv_sim",
    "--batch-run",
    "--max-steps", "500",
    "--agent-count", "3",
    "--tree-node", "0,split,speed,0.8,1,2",
    "--tree-node", "1,leaf,0,1,0,0",
    "--tree-node", "2,leaf,1,0,1,0",
    "--tree-root", "0",
]

result = subprocess.run(cmd, capture_output=True, text=True, check=True)
summary = json.loads(result.stdout)
print(summary["best_field_final"])
```

### 例 2: 通常実行して `simulation_data.json` を使う

```python
import subprocess

subprocess.run(["./build/uuv_sim"], check=True)
```

この場合は `simulation_data.json` を後で Python で読む形になります。

## 今の実装上の注意

- Python との接点はまだ CLI ベースです。
- 木の構造は JSON ではなく、カンマ区切りの `--tree-node` で渡します。
- `--batch-run` の出力は stdout に JSON 1 個だけ出るので、Python 側で読みやすいです。
- `--tree-node` の指定が壊れていると、内部でデフォルト木にフォールバックします。

## 補足

将来的に Python ラッパーを作るなら、今の CLI 形式はそのまま使えますが、より扱いやすくするなら次のどちらかが自然です。

- 木を JSON 文字列または JSON ファイルで渡す
- `main.cpp` をライブラリ化して、Python から直接呼べる API を用意する

現状は前者の準備として、まずは CLI で木を差し替えられる形にしてあります。
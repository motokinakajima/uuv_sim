# Agent Rule Formulas

このファイルは、現在の実装に対応する4つのルールを数式で整理したものです。

## 1. 回避ルール（近距離反発）

$$
\mathbf{d}_{ij}=\mathbf{x}_j-\mathbf{x}_i,\quad r_{ij}=\lVert \mathbf{d}_{ij}\rVert
$$

$$
\mathbf{a}_{\mathrm{avoid},i}
=
 k_{\mathrm{avoid}}
\sum_{j\neq i,\ r_{ij}<R_{\mathrm{avoid}}}
\left(
-\frac{\mathbf{d}_{ij}}{r_{ij}+0.1}
\right)
$$

## 2. 飽和クォーク引力ルール

$$
\hat{\mathbf{d}}_{ij}=\frac{\mathbf{d}_{ij}}{r_{ij}},\quad
s_{ij}=\frac{r_{ij}}{r_{ij}+R_{\mathrm{sat}}}
$$

$$
\mathbf{a}_{\mathrm{quark},i}
=
 k_{\mathrm{quark}}
\sum_{j\neq i}
\left(
F_{\max}\,s_{ij}\,\hat{\mathbf{d}}_{ij}
\right)
$$

## 3. 1ステップ記憶による方向微分（射影勾配）ルール

$$
\Delta \mathbf{x}_i(t)=\mathbf{x}_i(t)-\mathbf{x}_i(t-1),\quad
\Delta f_i(t)=f_i(t)-f_i(t-1)
$$

$$
\hat{\nabla} f_i^{\parallel}(t)
=
\frac{\Delta f_i(t)}{\lVert \Delta \mathbf{x}_i(t)\rVert^2+\varepsilon}
\,\Delta \mathbf{x}_i(t)
$$

$$
\mathbf{a}_{\mathrm{dir},i}(t)
=
-k_{\mathrm{dir}}\,\hat{\nabla} f_i^{\parallel}(t)
$$

## 4. 一次抵抗ルール

$$
\mathbf{a}_{\mathrm{drag},i}(t)
=
-k_{\mathrm{drag}}\,\mathbf{v}_i(t)
$$

## 合成加速度

$$
\mathbf{a}_i
=
\mathbf{a}_{\mathrm{avoid},i}
+
\mathbf{a}_{\mathrm{quark},i}
+
\mathbf{a}_{\mathrm{dir},i}
+
\mathbf{a}_{\mathrm{drag},i}
$$

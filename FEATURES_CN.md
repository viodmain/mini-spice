# mini-spice 功能说明文档

## 1. 概述
`mini-spice` 是一个基于 ngspice-45 架构开发的精简版电路仿真器。它实现了电路仿真的核心算法（如修改节点分析法 MNA、牛顿-拉夫逊迭代、梯形积分法），支持线性与非线性电路的直流、交流和瞬态分析。

本设计文档旨在详细说明当前版本支持的具体功能、语法规范及算法实现。

---

## 2. 支持的分析类型 (Analyses)

| 分析类型 | 命令关键字 | 功能描述 |
| :--- | :--- | :--- |
| **直流工作点分析** | `.OP` | 计算电路在直流稳态下的节点电压和支路电流。 |
| **直流扫描分析** | `.DC` | 扫描电压源或电流源的值，绘制传输特性曲线。 |
| **交流小信号分析** | `.AC` | 计算电路在不同频率下的频率响应（幅频/相频特性）。支持线性、十倍频、倍频程扫描。 |
| **瞬态分析** | `.TRAN` | 计算电路在时域内的响应。支持电容/电感的瞬态建模。 |

---

## 3. 支持的器件模型 (Devices)

### 3.1 无源器件
*   **电阻 (R)**: 线性电阻，支持标准单位（k, meg, m 等）。
*   **电容 (C)**: 线性电容，支持瞬态分析（梯形积分法）。
*   **电感 (L)**: 线性电感，支持瞬态分析。

### 3.2 独立源
*   **电压源 (V)**: 支持直流电压源 (`DC`) 和交流小信号幅值 (`AC`)。
*   **电流源 (I)**: 支持直流电流源。

### 3.3 受控源
*   **VCCS (G)**: 电压控制电流源。
*   **VCVS (E)**: 电压控制电压源。
*   **CCCS (F)**: 电流控制电流源（需通过电压源感知控制电流）。
*   **CCVS (H)**: 电流控制电压源。

### 3.4 非线性器件
*   **二极管 (D)**: PN 结二极管模型。
    *   支持模型参数：`IS` (饱和电流), `N` (发射系数), `RS` (欧姆电阻), `CJO`, `VJ`, `M` 等。
    *   支持牛顿-拉夫逊迭代求解非线性直流工作点。

---

## 4. 网表语法支持 (Netlist Syntax)

### 4.1 器件定义格式
```text
R<name> <node1> <node2> <value>
C<name> <node1> <node2> <value>
L<name> <node1> <node2> <value>
V<name> <node1> <node2> DC <dc_value> [AC <ac_magnitude>]
I<name> <node1> <node2> <value>
G<name> <out+> <out-> <ctrl+> <ctrl-> <transconductance>
E<name> <out+> <out-> <ctrl+> <ctrl-> <gain>
F<name> <out+> <out-> <vctrl_source> <gain>
H<name> <out+> <out-> <vctrl_source> <transresistance>
D<name> <anode> <cathode> <model_name>
```

### 4.2 数值单位后缀
支持以下科学计数法后缀：
*   `T` (1e12), `G` (1e9), `MEG` (1e6), `K` (1e3)
*   `m` (1e-3), `u` (1e-6), `n` (1e-9), `p` (1e-12), `f` (1e-15)

### 4.3 命令语句
*   `.OP`
*   `.DC <source> <start> <stop> <step>`
*   `.AC <type> <points> <start> <stop>` (type: `LIN`, `DEC`, `OCT`)
*   `.TRAN <tstep> <tstop>`
*   `.MODEL <name> <type> (param=value ...)`
*   `.TEMP <celsius>` (设置温度)
*   `.END` (网表结束)

---

## 5. 核心算法实现

| 算法模块 | 实现方式 | 说明 |
| :--- | :--- | :--- |
| **电路方程列写** | 修改节点分析法 (MNA) | 将节点电压和电压源支路电流作为未知数，构建矩阵方程 $Ax=b$。 |
| **矩阵求解** | 稀疏矩阵 LU 分解 | 使用全主元高斯消去法进行 LU 分解，保证数值稳定性。 |
| **非线性求解** | 牛顿-拉夫逊迭代 | 针对二极管等非线性器件，通过迭代更新电导和电流源，直到电压变化小于收敛容差。 |
| **瞬态积分** | 梯形积分法 (Trapezoidal) | 将电容/电感等效为电阻并联历史电流源，实现时域步进仿真。 |
| **稳定性增强** | GMIN 辅助 | 自动向每个节点添加 $10^{-12}$ S 的电导到地，防止浮空节点导致矩阵奇异。 |

---

## 6. 当前限制 (Limitations)

虽然核心功能已实现，但作为精简版，目前存在以下限制：
1.  **无子电路支持**: 暂不支持 `.SUBCKT` 和层次化设计。
2.  **波形源有限**: 仅支持直流 (`DC`) 源。瞬态分析中暂不支持 `SIN` (正弦), `PULSE` (脉冲) 等时变波形源。
3.  **无温度扫描**: `.TEMP` 仅设置全局温度，不支持 `.STEP TEMP`。
4.  **无噪声/傅里叶分析**: 仅包含基础的 DC, AC, TRAN 分析。
5.  **单线程运行**: 未实现并行计算优化。

---

## 8. 使用示例 (Usage Examples)

以下示例展示了如何使用 `mini-spice` 进行不同类型的电路仿真。您可以将代码保存为 `.net` 文件，然后通过命令行运行。

### 8.1 示例 1：直流工作点分析 (DC OP)
**电路描述**：一个简单的电阻分压电路，验证直流电压计算。
```spice
* 电压分压器 (Voltage Divider)
V1 in 0 DC 10
R1 in out 1k
R2 out 0 1k

.OP
.END
```
**运行命令**：
```bash
./mini-spice tests/voltage_divider.net
```
**预期结果**：
输出显示 `out` 节点电压为 `5.000000e+00` V（即 10V 经过两个 1k 电阻分压）。

---

### 8.2 示例 2：直流扫描分析 (DC Sweep)
**电路描述**：扫描输入电压源 `V1`，观察输出电压的变化，绘制传输特性曲线。
```spice
* 直流扫描测试 (DC Sweep)
V1 in 0 DC 0
R1 in out 1k
R2 out 0 1k

.DC V1 0 10 0.5
.END
```
**运行命令**：
```bash
./mini-spice tests/dc_sweep.net
```
**预期结果**：
输出一个表格，显示 `V1` 从 0V 到 10V 步进 0.5V 时，`out` 节点的电压值（应为 `V1/2`）。

---

### 8.3 示例 3：交流小信号分析 (AC Analysis)
**电路描述**：一阶 RC 低通滤波器。分析电路在不同频率下的增益衰减。
```spice
* RC 低通滤波器 (RC Low-Pass Filter)
V1 in 0 DC 5 AC 1
R1 in out 1k
C1 out 0 1u

.OP
.AC DEC 10 10 100k
.END
```
**运行命令**：
```bash
./mini-spice tests/rc_filter.net
```
**预期结果**：
1.  `.OP` 输出直流工作点。
2.  `.AC` 输出频率响应表。在低频（如 10Hz）时增益接近 1，在高频（如 100kHz）时增益显著下降（-3dB 截止频率约为 159Hz）。

---

### 8.4 示例 4：瞬态分析 (Transient Analysis)
**电路描述**：观察 RC 电路在直流电压作用下的电容充电过程。
```spice
* RC 瞬态响应 (RC Transient Response)
V1 in 0 DC 5
R1 in out 1k
C1 out 0 1u

.TRAN 10u 1m
.END
```
**运行命令**：
```bash
./mini-spice tests/rc_transient.net
```
**预期结果**：
输出时间序列数据。电压 `V(out)` 随时间按指数规律上升，最终趋近于 5V。时间常数 $\tau = RC = 1ms$。

---

### 8.5 示例 5：二极管非线性模型 (Diode Model)
**电路描述**：包含二极管的限幅电路，验证非线性器件的牛顿-拉夫逊迭代求解。
```spice
* 二极管限幅电路 (Diode Clipper)
V1 in 0 DC 5
R1 in out 1k
D1 out 0 D1N4148

.MODEL D1N4148 D(IS=2.52n N=1.752 RS=0.568)

.OP
.END
```
**预期结果**：
程序自动迭代求解二极管工作点，输出 `out` 节点电压（约为 0.7V 左右，取决于二极管模型参数）。

---

## 9. 快速开始

**编译:**
```bash
make
```

**运行示例:**
```bash
# 运行电压分压器测试
./mini-spice tests/voltage_divider.net

# 运行 RC 滤波器测试
./mini-spice tests/rc_filter.net
```

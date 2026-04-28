# mini-spice 功能介绍文档

## 1. 概述

`mini-spice` 是一个基于 ngspice-45 架构设计的精简版电路仿真器。它实现了电路仿真的核心算法（如修改节点分析法 MNA、牛顿-拉夫逊迭代、梯形积分法），支持线性与非线性电路的直流、交流、瞬态分析，以及多种高级分析功能。

本文档详细说明了 mini-spice 支持的所有功能、语法规范及算法实现。

---

## 2. 支持的分析类型 (Analyses)

| 分析类型 | 命令关键字 | 功能描述 |
| :--- | :--- | :--- |
| **直流工作点分析** | `.OP` | 计算电路在直流稳态下的节点电压和支路电流。 |
| **直流扫描分析** | `.DC` | 扫描电压源或电流源的值，绘制传输特性曲线。 |
| **交流小信号分析** | `.AC` | 计算电路在不同频率下的频率响应（幅频/相频特性）。支持线性、十倍频、倍频程扫描。 |
| **瞬态分析** | `.TRAN` | 计算电路在时域内的响应。支持电容/电感的瞬态建模。 |
| **噪声分析** | `.NOISE` | 计算电路的噪声谱密度，包括热噪声和散粒噪声。 |
| **傅里叶分析** | `.FOUR` | 对瞬态波形进行离散傅里叶变换，计算谐波分量和总谐波失真(THD)。 |
| **灵敏度分析** | `.SENS` | 计算直流工作点对各元件参数的灵敏度。 |

---

## 3. 支持的器件模型 (Devices)

### 3.1 无源器件
*   **电阻 (R)**: 线性电阻，支持标准单位（k, meg, m 等）。
*   **电容 (C)**: 线性电容，支持瞬态分析（梯形积分法）。
*   **电感 (L)**: 线性电感，支持瞬态分析。

### 3.2 独立源
*   **电压源 (V)**: 支持直流电压源 (`DC`)、交流小信号 (`AC`) 和多种时变波形。
*   **电流源 (I)**: 支持直流电流源和交流小信号。

### 3.3 受控源
*   **VCCS (G)**: 电压控制电流源。
*   **VCVS (E)**: 电压控制电压源。
*   **CCCS (F)**: 电流控制电流源（需通过电压源感知控制电流）。
*   **CCVS (H)**: 电流控制电压源。

### 3.4 非线性器件
*   **二极管 (D)**: PN 结二极管模型。
    *   支持模型参数：`IS` (饱和电流), `N` (发射系数), `RS` (欧姆电阻), `CJO`, `VJ`, `M` 等。
    *   支持牛顿-拉夫逊迭代求解非线性直流工作点。

*   **BJT (Q)**: 双极结型晶体管（NPN/PNP），基于简化 Ebers-Moll 模型。
    *   支持模型参数：`IS`, `BF`, `NF`, `BR`, `NR`, `RB`, `RE`, `RC`, `TF`, `TR` 等。
    *   支持直流工作点分析和交流小信号分析。
    *   牛顿-拉夫逊迭代求解非线性特性。

*   **MOSFET (M)**: 金属氧化物半导体场效应晶体管（NMOS/PMOS），基于 Level 1 Shichman-Hodges 模型。
    *   支持模型参数：`KP`, `VTO`, `GAMMA`, `LAMBDA`, `PHI`, `W`, `L` 等。
    *   支持截止区、线性区、饱和区三种工作区域。
    *   支持体效应（Body Effect）和沟道长度调制。
    *   牛顿-拉夫逊迭代求解非线性特性。

### 3.5 波形源
*   **SIN (正弦波)**: `SIN(Voffset Vamp Freq Td Theta Phi)`
    *   `Voffset`: 偏移电压
    *   `Vamp`: 振幅
    *   `Freq`: 频率 (Hz)
    *   `Td`: 延迟时间
    *   `Theta`: 阻尼因子
    *   `Phi`: 初始相位 (度)

*   **PULSE (脉冲波)**: `PULSE(V1 V2 Td Tr Tf Pw Per)`
    *   `V1`: 初始值
    *   `V2`: 脉冲值
    *   `Td`: 延迟时间
    *   `Tr`: 上升时间
    *   `Tf`: 下降时间
    *   `Pw`: 脉冲宽度
    *   `Per`: 周期

*   **PWL (分段线性)**: `PWL(t1 v1 t2 v2 ...)`
    *   支持最多 100 个分段点
    *   线性插值

*   **EXP (指数波)**: `EXP(V1 V2 Td1 Tau1 Td2 Tau2)`
    *   `V1`: 初始值
    *   `V2`: 峰值
    *   `Td1`: 上升延迟时间
    *   `Tau1`: 上升时间常数
    *   `Td2`: 下降延迟时间
    *   `Tau2`: 下降时间常数

### 3.6 行为源
*   **行为电流源 (B)**: `B<name> n+ n- I=<expression>`
    *   支持任意表达式，可使用 `V(node)`, `I(source)`, `TIME` 等变量
    *   示例：`B1 out 0 I=V(in)*0.001`

*   **行为电压源 (E-b)**: `B<name> n+ n- V=<expression>`
    *   支持任意表达式
    *   示例：`B2 out 0 V=V(in)*2 + V(out)^2`

### 3.7 开关模型
*   **电压控制开关 (S)**: `S<name> nc+ nc- n+ n- model`
    *   支持模型参数：`RON` (导通电阻), `ROFF` (关断电阻), `VT` (阈值电压), `VH` (迟滞电压)

*   **电流控制开关 (W)**: `W<name> nc+ nc- n+ n- model`
    *   参数同电压控制开关

### 3.8 传输线
*   **无损传输线 (T)**: `T<name> n1+ n1- n2+ n2- model`
    *   支持模型参数：`TD` (延迟时间), `Z0` (特性阻抗)
    *   基于特性方法 (Method of Characteristics) 实现

### 3.9 子电路
*   **子电路实例 (X)**: `X<name> port1 port2 ... subckt_name`
    *   支持 `.SUBCKT`/`.ENDS` 定义
    *   支持层次化设计

---

## 4. 网表语法支持 (Netlist Syntax)

### 4.1 器件定义格式
```text
R<name> <node1> <node2> <value>
C<name> <node1> <node2> <value>
L<name> <node1> <node2> <value>
V<name> <node1> <node2> DC <dc_value> [AC <ac_mag>] [SIN|PULSE|PWL|EXP ...]
I<name> <node1> <node2> <value> [AC <ac_mag>]
G<name> <out+> <out-> <ctrl+> <ctrl-> <transconductance>
E<name> <out+> <out-> <ctrl+> <ctrl-> <gain>
F<name> <out+> <out-> <vctrl_source> <gain>
H<name> <out+> <out-> <vctrl_source> <transresistance>
D<name> <anode> <cathode> <model_name>
Q<name> <collector> <base> <emitter> [substrate] <model_name>
M<name> <drain> <gate> <source> [body] <model_name> [W=<width> L=<length>]
B<name> <node1> <node2> I=<expr> | V=<expr>
S<name> <ctrl+> <ctrl-> <switch+> <switch-> <model_name>
W<name> <ctrl+> <ctrl-> <switch+> <switch-> <model_name>
T<name> <port1+> <port1-> <port2+> <port2-> <model_name>
X<name> <port1> <port2> ... <subckt_name>
```

### 4.2 数值单位后缀
支持以下科学计数法后缀：
*   `T` (1e12), `G` (1e9), `MEG` (1e6), `K` (1e3)
*   `m` (1e-3), `u` (1e-6), `n` (1e-9), `p` (1e-12), `f` (1e-15)

### 4.3 命令语句
*   `.OP` - 直流工作点分析
*   `.DC <source> <start> <stop> <step>` - 直流扫描
*   `.AC <type> <points> <start> <stop>` - 交流分析 (type: `LIN`, `DEC`, `OCT`)
*   `.TRAN <tstep> <tstop> [UIC]` - 瞬态分析
*   `.NOISE V(out) src <type> <points> <start> <stop>` - 噪声分析
*   `.FOUR <fund_freq> V(out) [harmonics]` - 傅里叶分析
*   `.SENS V(out)` - 灵敏度分析
*   `.MODEL <name> <type> (param=value ...)` - 器件模型
*   `.TEMP <celsius>` - 设置温度
*   `.PARAM name=value [name=value ...]` - 参数定义
*   `.STEP <param> <start> <stop> <step>` - 参数扫描（解析但不完整实现）
*   `.OPTIONS <option>=<value> ...` - 仿真选项
*   `.PRINT <var> [<var> ...]` - 输出指定变量
*   `.IC V(node)=<value> ...` - 初始条件
*   `.SUBCKT <name> <port1> <port2> ...` - 子电路定义
*   `.ENDS` - 子电路结束
*   `.END` - 网表结束

### 4.4 模型类型
*   `D` 或 `DIODE` - 二极管
*   `NPN` - NPN 晶体管
*   `PNP` - PNP 晶体管
*   `NMOS` - NMOS 场效应管
*   `PMOS` - PMOS 场效应管
*   `SW` 或 `SWITCH` - 电压控制开关
*   `CSW` 或 `CURRENT_SWITCH` - 电流控制开关
*   `TLINE` 或 `TRANSMISSION_LINE` - 传输线

### 4.5 仿真选项 (.OPTIONS)
*   `ABSTOL=<value>` - 绝对电流容差
*   `VNTOL=<value>` - 电压容差
*   `RELTOL=<value>` - 相对容差
*   `TRTOL=<value>` - 截断误差容差
*   `MAXITER=<value>` - 最大迭代次数
*   `GMIN=<value>` - 最小电导

### 4.6 连续行
支持使用 `+` 开头的连续行，用于长语句的换行：
```text
V1 in 0 SIN(0 1 1k
+ 0 0 0)
```

---

## 5. 核心算法实现

| 算法模块 | 实现方式 | 说明 |
| :--- | :--- | :--- |
| **电路方程列写** | 修改节点分析法 (MNA) | 将节点电压和电压源支路电流作为未知数，构建矩阵方程 Ax=b。 |
| **矩阵求解** | 稀疏矩阵 LU 分解 | 使用全主元高斯消去法进行 LU 分解，保证数值稳定性。 |
| **非线性求解** | 牛顿-拉夫逊迭代 | 针对二极管、BJT、MOSFET 等非线性器件，通过迭代更新电导和电流源，直到电压变化小于收敛容差。 |
| **瞬态积分** | 梯形积分法 (Trapezoidal) | 将电容/电感等效为电阻并联历史电流源，实现时域步进仿真。 |
| **稳定性增强** | GMIN 辅助 | 自动向每个节点添加最小电导（默认 1e-12 S）到地，防止浮空节点导致矩阵奇异。 |
| **波形评估** | 实时计算 | 在瞬态分析的每个时间步，根据当前时间评估波形源的瞬时值。 |
| **傅里叶变换** | 离散傅里叶变换 (DFT) | 对瞬态分析结果进行 DFT，计算各次谐波的幅度和相位，并计算总谐波失真 (THD)。 |
| **噪声分析** | 噪声源叠加 | 计算电阻的热噪声（4kT/R）和二极管的散粒噪声（2qI），并叠加得到输出噪声谱密度。 |
| **灵敏度分析** | 扰动法 | 对每个元件值施加微小扰动（1%），重新计算直流工作点，得到输出对元件参数的灵敏度。 |

---

## 6. 使用示例 (Usage Examples)

### 6.1 示例 1：直流工作点分析 (DC OP)
```spice
* 电压分压器
V1 in 0 DC 10
R1 in out 1k
R2 out 0 1k

.OP
.END
```

### 6.2 示例 2：直流扫描分析 (DC Sweep)
```spice
* 直流扫描
V1 in 0 DC 0
R1 in out 1k
R2 out 0 1k

.DC V1 0 10 0.5
.END
```

### 6.3 示例 3：交流小信号分析 (AC Analysis)
```spice
* RC 低通滤波器
V1 in 0 DC 5 AC 1
R1 in out 1k
C1 out 0 1u

.AC DEC 10 10 100k
.END
```

### 6.4 示例 4：瞬态分析 (Transient Analysis)
```spice
* RC 瞬态响应
V1 in 0 DC 5
R1 in out 1k
C1 out 0 1u

.TRAN 10u 1m
.END
```

### 6.5 示例 5：正弦波源瞬态分析
```spice
* 正弦波源
V1 in 0 SIN(0 1 1k 0 0 0)
R1 in out 1k
C1 out 0 100n

.TRAN 10u 5m
.END
```

### 6.6 示例 6：脉冲波源
```spice
* 脉冲波源
V1 in 0 PULSE(0 5 1m 10u 10u 1m 4m)
R1 in out 1k
C1 out 0 1u

.TRAN 10u 10m
.END
```

### 6.7 示例 7：二极管非线性模型
```spice
* 二极管限幅电路
V1 in 0 DC 5
R1 in out 1k
D1 out 0 D1N4148

.MODEL D1N4148 D(IS=2.52n N=1.752 RS=0.568)

.OP
.END
```

### 6.8 示例 8：BJT 放大器
```spice
* BJT 放大器
Vcc collector 0 DC 10
Vb base 0 DC 0.7
Rc collector 0 1k
Re emitter 0 100
Q1 collector base emitter Q2N2222

.MODEL Q2N2222 NPN(IS=1e-14 BF=100 NF=1.0)

.OP
.END
```

### 6.9 示例 9：MOSFET 放大器
```spice
* MOSFET 放大器
Vdd drain 0 DC 10
Vg gate 0 DC 2.5
Rd drain 0 1k
Rs source 0 500
M1 drain gate source 0 NMOS1 W=10u L=1u

.MODEL NMOS1 NMOS(KP=100u VTO=0.7 LAMBDA=0.01)

.OP
.END
```

### 6.10 示例 10：参数化分析
```spice
* 参数化分析
.PARAM Rval=1k Cval=100n

V1 in 0 DC 5
R1 in out {Rval}
C1 out 0 {Cval}

.OP
.TRAN 10u 1m
.END
```

### 6.11 示例 11：初始条件
```spice
* 初始条件
V1 in 0 DC 5
R1 in out 1k
C1 out 0 1u

.IC V(out)=2.5

.TRAN 10u 5m UIC
.END
```

### 6.12 示例 12：傅里叶分析
```spice
* 傅里叶分析
V1 in 0 SIN(2.5 2.5 1k)
R1 in out 1k
D1 out 0 D1N4148

.MODEL D1N4148 D(IS=2.52n N=1.752)

.TRAN 1u 5m
.FOUR 1k V(out) 9
.END
```

### 6.13 示例 13：灵敏度分析
```spice
* 灵敏度分析
V1 in 0 DC 10
R1 in out 1k
R2 out 0 2k

.SENS V(out)
.END
```

### 6.14 示例 14：噪声分析
```spice
* 噪声分析
V1 in 0 DC 5 AC 1
R1 in out 1k
R2 out 0 2k
C1 out 0 100n

.NOISE V(out) V1 DEC 10 10 100k
.END
```

### 6.15 示例 15：子电路
```spice
* 子电路定义
.SUBCKT AMPLIFIER in out gnd
Q1 out in gnd Q2N2222
R1 out vcc 1k
.ENDS

* 主电路
Vcc vcc 0 DC 10
Vin in 0 DC 0.7
X1 in out gnd AMPLIFIER
.END
```

---

## 7. 仿真选项 (.OPTIONS)

可以通过 `.OPTIONS` 命令设置仿真参数：

```spice
.OPTIONS abstol=1e-12 vntol=1e-6 reltol=0.001 maxiter=50 gmin=1e-12
```

| 选项 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `abstol` | 1e-12 | 绝对电流容差 (A) |
| `vntol` | 1e-6 | 电压容差 (V) |
| `reltol` | 1e-6 | 相对容差 |
| `trtol` | 1.0 | 截断误差容差 |
| `maxiter` | 50 | 最大迭代次数 |
| `gmin` | 1e-12 | 最小电导 (S) |

---

## 8. 当前限制 (Limitations)

虽然核心功能已实现，但作为精简版，目前存在以下限制：

1.  **子电路功能部分实现**: `.SUBCKT`/`.ENDS` 语法已支持，但子电路实例化（X 前缀）的内部节点映射需要进一步完善。
2.  **波形源在瞬态分析中的集成**: SIN/PULSE/PWL/EXP 波形源的解析已完成，但在瞬态分析中的实时更新需要进一步优化。
3.  **BJT/MOSFET 模型简化**: 使用简化模型，不包含所有二级效应（如温度依赖性、击穿效应等）。
4.  **行为源表达式有限**: 表达式解析器支持基本函数，但不支持复杂数学表达式。
5.  **无温度扫描**: `.TEMP` 仅设置全局温度，不支持 `.STEP TEMP`。
6.  **单线程运行**: 未实现并行计算优化。
7.  **输出格式**: 目前仅支持终端输出，不支持波形文件导出（如 SPICE raw 格式）。

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

# 运行 BJT 放大器测试
./mini-spice tests/bjt_amp.net

# 运行 MOSFET 放大器测试
./mini-spice tests/mos_amp.net
```

**命令行选项:**
```bash
./mini-spice [options] <netlist_file>

Options:
  -h, --help          Show this help
  -v, --version       Show version
  -o <file>           Output file
  -T <temp>           Set temperature (Celsius)
```

---

## 10. 文件结构

```
ngspice-rewrite/
├── include/
│   ├── spice_types.h    # 公共类型、常量、枚举
│   ├── circuit.h        # 电路、节点、器件、模型数据结构
│   ├── sparse.h         # 稀疏矩阵结构
│   ├── device.h         # 器件操作接口
│   ├── analysis.h       # 分析操作接口
│   ├── parser.h         # 解析器 API
│   └── output.h         # 输出 API
├── src/
│   ├── main.c           # CLI 入口点
│   ├── core/
│   │   └── circuit.c    # 电路生命周期、节点/器件管理
│   ├── parser/
│   │   └── parser.c     # 网表分词器和解析器
│   ├── devices/
│   │   ├── res.c        # 电阻
│   │   ├── cap.c        # 电容
│   │   ├── ind.c        # 电感
│   │   ├── vsrc.c       # 电压源（支持波形）
│   │   ├── isrc.c       # 电流源
│   │   ├── vccs.c       # VCCS
│   │   ├── vcvs.c       # VCVS
│   │   ├── cccs.c       # CCCS
│   │   ├── ccvs.c       # CCVS
│   │   ├── dio.c        # 二极管
│   │   ├── bjt.c        # BJT (NPN/PNP)
│   │   ├── mos.c        # MOSFET (NMOS/PMOS)
│   │   ├── behsrc.c     # 行为源
│   │   ├── switch.c     # 开关
│   │   ├── tline.c      # 传输线
│   │   └── devreg.c     # 器件注册系统
│   ├── analysis/
│   │   ├── dcop.c       # 直流工作点分析
│   │   ├── dcsweep.c    # 直流扫描分析
│   │   ├── acan.c       # 交流分析
│   │   ├── dctran.c     # 瞬态分析
│   │   ├── noise.c      # 噪声分析
│   │   ├── fourier.c    # 傅里叶分析
│   │   ├── sens.c       # 灵敏度分析
│   │   └── anareg.c     # 分析注册系统
│   ├── math/
│   │   └── sparse.c     # 稀疏矩阵 LU 求解器
│   └── output/
│       └── output.c     # 结果格式化和输出
├── tests/               # 示例网表
├── Makefile             # 构建系统
├── README.md            # 用户指南
├── DESIGN.md            # 设计文档
└── FEATURES.md          # 本文档
```

---

## 11. 参考文献

- ngspice-45 Manual & Source Code (https://ngspice.sourceforge.io/)
- "The SPICE Book" by Andrei Vladimirescu
- "Computer-Aided Analysis of Electronic Circuits" by Loebel et al.
- "Microelectronic Circuits" by Sedra/Smith

# mini-spice C++ Version - Feature Documentation

## 1. Overview

`mini-spice` is a lightweight, educational SPICE circuit simulator rewritten in modern C++17. The C++ version maintains full compatibility with the original C version while providing:

- **Type Safety**: Strongly-typed enums, classes, and templates
- **Memory Safety**: RAII, smart pointers, automatic resource management
- **Modern C++ Features**: `std::unique_ptr`, `std::shared_ptr`, `std::vector`, `std::string`, `std::variant`, `std::optional`
- **Cleaner Code**: Namespaces, encapsulation, operator overloading
- **Better Documentation**: Doxygen-style comments throughout

The simulator implements core circuit simulation algorithms including Modified Nodal Analysis (MNA), Newton-Raphson iteration for nonlinear convergence, and trapezoidal integration for transient analysis.

---

## 2. C++17 Features Used

| Feature | Usage | Benefit |
|---------|-------|---------|
| `std::unique_ptr` | Device, Node, Circuit ownership | Automatic memory management, no leaks |
| `std::shared_ptr` | Model sharing between devices | Reference counting, shared ownership |
| `std::vector` | Replaces linked lists | Contiguous memory, better cache locality |
| `std::string` | Replaces `char*` | Automatic memory, string operations |
| `std::variant` | Model parameters (type-safe union) | Type-safe alternative to `void*` |
| `std::optional` | Optional return values | Explicit "no value" semantics |
| `std::function` | Device/Analysis operations | Flexible callback system |
| `constexpr` | Constants | Compile-time evaluation |
| Namespaces | Code organization | Avoid symbol collisions |
| RAII | Resource management | Exception-safe cleanup |

---

## 3. Architecture

### 3.1 Namespace Structure

All code is organized under the `spice` namespace:

```cpp
namespace spice {
    class Circuit;
    class Node;
    class Device;
    // ...
}
```

### 3.2 Class Hierarchy

```
Circuit (top-level container)
├── Node[] (nodes, owned by unique_ptr)
├── Device[] (devices, owned by unique_ptr)
│   ├── waveform (unique_ptr<WaveformParams>)
│   ├── model (shared_ptr<Model>)
│   └── params (void*, device-specific state)
├── Model[] (models, shared_ptr for sharing)
├── Analysis[] (analyses, owned by unique_ptr)
├── SubcktDef[] (subcircuit definitions)
└── Param[] (parameters, value type)
```

### 3.3 Device Operations Interface

```cpp
struct DeviceOps {
    std::string name;
    DeviceType type;
    std::function<ErrorCode(Device*, Circuit*, SparseMatrix*)> load;
    std::function<ErrorCode(Device*, Circuit*, SparseMatrix*, Real)> acLoad;
    std::function<ErrorCode(Device*, Circuit*, SparseMatrix*)> nonlinear;
    // ...
};
```

Devices auto-register via static initialization:

```cpp
static bool registerResistor() {
    registerDevice(createResOps());
    return true;
}
static bool resistor_registered = registerResistor();
```

---

## 4. Supported Features

### 4.1 Analysis Types

| Analysis | Command | Implementation |
|----------|---------|----------------|
| DC Operating Point | `.OP` | Newton-Raphson iteration |
| DC Sweep | `.DC` | Loop over source values |
| AC Small-Signal | `.AC` | Linearized frequency response |
| Transient | `.TRAN` | Time stepping with trapezoidal integration |
| Noise | `.NOISE` | Thermal and shot noise |
| Fourier | `.FOUR` | DFT for harmonics & THD |
| Sensitivity | `.SENS` | Perturbation method |

### 4.2 Device Types

#### Linear Devices
- **Resistor (R)**: Linear resistance
- **Capacitor (C)**: Linear capacitance (transient support)
- **Inductor (L)**: Linear inductance (transient support)

#### Sources
- **Voltage Source (V)**: DC, AC, SIN, PULSE, PWL, EXP
- **Current Source (I)**: DC, AC

#### Dependent Sources
- **VCCS (G)**: Voltage-controlled current source
- **VCVS (E)**: Voltage-controlled voltage source
- **CCCS (F)**: Current-controlled current source
- **CCVS (H)**: Current-controlled voltage source

#### Nonlinear Devices
- **Diode (D)**: PN junction, Shockley equation
- **BJT (Q)**: NPN/PNP, Ebers-Moll model
- **MOSFET (M)**: NMOS/PMOS, Level 1 Shichman-Hodges

#### Advanced Devices
- **Behavioral Source (B)**: Expression-based sources
- **Switches (S/W)**: Voltage/current controlled
- **Transmission Line (T)**: Lossless, Method of Characteristics
- **Subcircuit (X)**: Hierarchical design

### 4.3 Waveform Sources

| Waveform | Syntax | Parameters |
|----------|--------|------------|
| SIN | `SIN(Voffset Vamp Freq Td Theta Phi)` | 6 parameters |
| PULSE | `PULSE(V1 V2 Td Tr Tf Pw Per)` | 7 parameters |
| PWL | `PWL(t1 v1 t2 v2 ...)` | Variable points |
| EXP | `EXP(V1 V2 Td1 Tau1 Td2 Tau2)` | 6 parameters |
| AC | `AC magnitude [phase]` | 1-2 parameters |

---

## 5. C++ Implementation Details

### 5.1 Memory Management

**Before (C):**
```c
Circuit *ckt = (Circuit *)malloc(sizeof(Circuit));
Node *node = (Node *)malloc(sizeof(Node));
// Manual free in circuit_free()
```

**After (C++):**
```cpp
auto ckt = std::make_unique<Circuit>("title");
auto node = std::make_unique<Node>("n1", 0);
// Automatic cleanup via RAII
```

### 5.2 Data Structures

**Before (C linked list):**
```c
typedef struct Node {
    char *name;
    struct Node *next;
} Node;
```

**After (C++ vector):**
```cpp
class Circuit {
    std::vector<std::unique_ptr<Node>> nodes;
};
```

### 5.3 Type Safety

**Before (C enum):**
```c
typedef enum {
    DEV_RESISTOR = 0,
    DEV_CAPACITOR,
    // ...
} device_type_t;
```

**After (C++ scoped enum):**
```cpp
enum class DeviceType : int {
    RESISTOR = 0,
    CAPACITOR,
    // ...
};
```

### 5.4 Variant for Model Parameters

**Before (C void pointer):**
```c
typedef struct Model {
    void *params;  // Cast to diode_model_t, bjt_model_t, etc.
} Model;
```

**After (C++ variant):**
```cpp
class Model {
    std::variant<
        std::monostate,
        DiodeModelParams,
        BjtModelParams,
        MosModelParams,
        SwitchModelParams,
        TransmissionLineParams
    > params;
};
```

---

## 6. Building and Running

### 6.1 Requirements

- **Compiler**: GCC 7+ or Clang 5+ with C++17 support
- **CMake**: 3.14+ (recommended)
- **Make**: GNU Make
- **Math Library**: libm (linked automatically)

### 6.2 Building with CMake (Recommended)

```bash
# Configure
mkdir build && cd build
cmake ..

# Build
cmake --build .

# Run tests
ctest

# Install (optional)
sudo cmake --install .
```

**CMake options:**

```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Custom install prefix
cmake -DCMAKE_INSTALL_PREFIX=/opt/mini-spice ..

# Specify compiler
CC=gcc CXX=g++ cmake ..
```

### 6.3 Building with Make (Legacy)

The project previously used a manual Makefile. This has been replaced by CMake.
If you still need to use Make, run:

```bash
cd build
make
```

### 6.4 Running

```bash
# Run a netlist
./mini-spice tests/voltage_divider.net

# Specify output file
./mini-spice -o results.raw tests/voltage_divider.net

# Set temperature
./mini-spice -T 50 tests/voltage_divider.net

# Show help
./mini-spice --help

# Show version
./mini-spice --version
```

---

## 7. Netlist Syntax

### 7.1 Device Definitions

```spice
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

### 7.2 Value Suffixes

| Suffix | Multiplier |
|--------|-----------|
| T | 1e12 |
| G | 1e9 |
| MEG | 1e6 |
| K | 1e3 |
| m | 1e-3 |
| u | 1e-6 |
| n | 1e-9 |
| p | 1e-12 |
| f | 1e-15 |

### 7.3 Dot Commands

```spice
.OP                                    # DC operating point
.DC <source> <start> <stop> <step>     # DC sweep
.AC <type> <points> <start> <stop>     # AC analysis
.TRAN <tstep> <tstop> [UIC]            # Transient analysis
.NOISE V(out) src <type> <pts> <start> <stop>  # Noise analysis
.FOUR <fund_freq> V(out) [harmonics]   # Fourier analysis
.SENS V(out)                           # Sensitivity analysis
.MODEL <name> <type> (param=value ...) # Device model
.TEMP <celsius>                        # Temperature
.PARAM name=value                      # Parameter definition
.OPTIONS <opt>=<value> ...             # Simulation options
.PRINT <var> [...]                     # Output specification
.IC V(node)=<value> ...                # Initial conditions
.SUBCKT <name> <port1> <port2> ...     # Subcircuit definition
.ENDS                                  # Subcircuit end
.END                                   # Netlist end
```

---

## 8. Examples

### 8.1 Voltage Divider

```spice
* Voltage Divider
V1 in 0 DC 10
R1 in out 1k
R2 out 0 1k

.OP
.END
```

### 8.2 RC Filter with AC Analysis

```spice
* RC Low-pass Filter
V1 in 0 DC 5 AC 1
R1 in out 1k
C1 out 0 1u

.AC DEC 10 10 100k
.END
```

### 8.3 RC Transient Response

```spice
* RC Transient
V1 in 0 PULSE(0 5 1m 10u 10u 1m 4m)
R1 in out 1k
C1 out 0 1u

.TRAN 10u 10m
.END
```

### 8.4 Diode Clipper

```spice
* Diode Clipper
V1 in 0 DC 5
R1 in out 1k
D1 out 0 D1N4148

.MODEL D1N4148 D(IS=2.52n N=1.752 RS=0.568)

.OP
.END
```

### 8.5 BJT Amplifier

```spice
* BJT Amplifier
Vcc collector 0 DC 10
Vb base 0 DC 0.7
Rc collector 0 1k
Re emitter 0 100
Q1 collector base emitter Q2N2222

.MODEL Q2N2222 NPN(IS=1e-14 BF=100 NF=1.0)

.OP
.END
```

### 8.6 MOSFET Amplifier

```spice
* MOSFET Amplifier
Vdd drain 0 DC 10
Vg gate 0 DC 2.5
Rd drain 0 1k
Rs source 0 500
M1 drain gate source 0 NMOS1 W=10u L=1u

.MODEL NMOS1 NMOS(KP=100u VTO=0.7 LAMBDA=0.01)

.OP
.END
```

---

## 9. File Structure

```
ngspice-rewrite/
├── include/
│   ├── spice_types.hpp    # Common types, constants, enums
│   ├── circuit.hpp        # Circuit, Node, Device, Model classes
│   ├── sparse.hpp         # SparseMatrix class
│   ├── device.hpp         # DeviceOps interface
│   ├── analysis.hpp       # AnalysisOps interface
│   ├── parser.hpp         # Parser interface
│   └── output.hpp         # Output interface
├── src/
│   ├── main.cpp           # CLI entry point
│   ├── core/
│   │   └── circuit.cpp    # Circuit implementation
│   ├── parser/
│   │   └── parser.cpp     # Netlist parser
│   ├── devices/
│   │   ├── res.cpp        # Resistor
│   │   ├── cap.cpp        # Capacitor
│   │   ├── ind.cpp        # Inductor
│   │   ├── vsrc.cpp       # Voltage source
│   │   ├── isrc.cpp       # Current source
│   │   ├── vccs.cpp       # VCCS
│   │   ├── vcvs.cpp       # VCVS
│   │   ├── cccs.cpp       # CCCS
│   │   ├── ccvs.cpp       # CCVS
│   │   ├── dio.cpp        # Diode
│   │   ├── bjt.cpp        # BJT (NPN/PNP)
│   │   ├── mos.cpp        # MOSFET (NMOS/PMOS)
│   │   ├── behsrc.cpp     # Behavioral source
│   │   ├── switch.cpp     # Switches
│   │   ├── tline.cpp      # Transmission line
│   │   └── devreg.cpp     # Device registry
│   ├── analysis/
│   │   ├── dcop.cpp       # DC operating point
│   │   ├── dcsweep.cpp    # DC sweep
│   │   ├── acan.cpp       # AC analysis
│   │   ├── dctran.cpp     # Transient analysis
│   │   ├── noise.cpp      # Noise analysis
│   │   ├── fourier.cpp    # Fourier analysis
│   │   ├── sens.cpp       # Sensitivity analysis
│   │   └── anareg.cpp     # Analysis registry
│   ├── math/
│   │   └── sparse.cpp     # Sparse matrix solver
│   └── output/
│       └── output.cpp     # Output formatting
├── tests/                 # Example netlists
├── CMakeLists.txt          # CMake build system
├── README.md              # User guide
├── DESIGN.md              # Design document
├── FEATURES_CN_V0.02.md   # Original C version docs
└── FEATURES_CPP.md        # This document
```

---

## 10. Limitations

1. **Parser**: Currently a skeleton implementation; full parsing needs to be completed
2. **Subcircuits**: Partial implementation
3. **AC Analysis**: Real-valued only (no complex numbers)
4. **BJT/MOSFET**: Simplified models without all second-order effects
5. **Behavioral Sources**: Limited expression parser
6. **Single-threaded**: No parallelization
7. **Output**: Terminal only; no waveform file export

---

## 11. Future Work

- Complete the parser implementation
- Add complex number support for AC analysis
- Implement advanced MOSFET models (BSIM)
- Add temperature sweep (`.STEP TEMP`)
- Implement SPICE raw file export
- Add GUI/plotting support
- Multi-core parallelization

---

## 12. References

- ngspice-45 Manual & Source Code (https://ngspice.sourceforge.io/)
- "The SPICE Book" by Andrei Vladimirescu
- "Computer-Aided Analysis of Electronic Circuits" by Loebel et al.
- "Microelectronic Circuits" by Sedra/Smith
- C++17 Standard (ISO/IEC 14882:2017)

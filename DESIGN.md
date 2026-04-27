# mini-spice Design Document

## 1. Overview
`mini-spice` is a lightweight, educational SPICE circuit simulator inspired by the ngspice-45 architecture. It implements core circuit simulation algorithms including Modified Nodal Analysis (MNA), Newton-Raphson iteration for nonlinear convergence, and trapezoidal integration for transient analysis. The simulator supports linear and nonlinear devices, multiple analysis types, and a simplified SPICE netlist parser.

## 2. System Architecture
The simulator follows a modular pipeline architecture:
```
Netlist File
     │
     ▼
[Parser] ──► [Circuit Builder] ──► [Circuit Data Structure]
     │
     ▼
[Analysis Dispatcher] ──► [Solver Engine]
     │                           │
     ▼                           ▼
[Output Formatter] ◄── [Device Loaders] ◄── [Sparse Matrix]
```

**Key Layers:**
- **Frontend**: Netlist parsing, command processing, output formatting
- **Core**: Circuit representation, node/equation mapping
- **Backend**: Matrix assembly, device models, analysis algorithms
- **Math**: Sparse linear algebra, numerical integration

## 3. Core Modules

### 3.1 Netlist Parser (`src/parser/parser.c`)
- **Type**: Single-pass, token-based parser
- **Features**:
  - Reads SPICE-compatible netlists
  - Supports device prefixes: `R`, `C`, `L`, `V`, `I`, `G`, `E`, `F`, `H`, `D`
  - Parses dot-commands: `.OP`, `.DC`, `.AC`, `.TRAN`, `.MODEL`, `.TEMP`
  - Handles value suffixes: `k`, `meg`, `m`, `u`, `n`, `p`, `f`
  - Supports `DC`, `AC` qualifiers for sources
- **Limitations**: No subcircuit (`.SUBCKT`) expansion, no continuation lines (`+`), no parametric sweeps

### 3.2 Circuit Data Structures (`include/circuit.h`, `src/core/circuit.c`)
- **`Circuit`**: Top-level container holding nodes, devices, models, analyses, tolerances, and temperature
- **`Node`**: Linked list of circuit nodes with name, internal number, and equation index (`eqnum`)
- **`Device`**: Instance data including type, node connections (`n1`-`n5`), values, and model pointers
- **`Model`**: Parameter sets for device types (e.g., diode `IS`, `N`, `RS`)
- **`Analysis`**: Linked list of requested analyses with sweep parameters
- **Equation Mapping**: 
  - Node voltages map to equation indices `0` to `N-1`
  - Voltage source branch currents are allocated separately and resolved after parsing

### 3.3 Sparse Matrix Solver (`include/sparse.h`, `src/math/sparse.c`)
- **Structure**: Row/column linked lists for non-zero elements
- **RHS Handling**: Separate `rhs[]` array (not embedded in matrix)
- **Factorization**: LU decomposition with **full pivoting** (searches entire unassigned submatrix for largest element)
- **Solve**: Forward/backward substitution with row/column permutation tracking
- **Stability**: GMIN (1e-12 S) automatically added to ground for floating nodes

### 3.4 Device Models (`src/devices/*.c`)
Each device implements a standardized interface (`DeviceOps`):
```c
typedef struct DeviceOps {
    const char *name;
    device_type_t type;
    int (*setup)(Device*, Circuit*);
    int (*load)(Device*, Circuit*, SparseMatrix*);
    int (*ac_load)(Device*, Circuit*, SparseMatrix*, double omega);
    int (*update)(Device*, Circuit*);
    int (*nonlinear)(Device*, Circuit*, SparseMatrix*);
} DeviceOps;
```
- **Linear Devices** (R, C, L, V, I, G, E, F, H): Implement `load()` and `ac_load()`
- **Nonlinear Devices** (D): Implement `nonlinear()` for Newton-Raphson iteration
- **Registration**: Centralized in `devreg.c` via function pointer table

### 3.5 Analysis Engines (`src/analysis/*.c`)
| Analysis | File | Algorithm |
|----------|------|-----------|
| DC OP | `dcop.c` | Newton-Raphson iteration until voltage change < `vntol` |
| DC Sweep | `dcsweep.c` | Loop over source values, run DC OP at each step |
| AC | `acan.c` | Linearize around DC OP, solve complex system per frequency |
| Transient | `dctran.c` | Time stepping, capacitor history update, Newton-Raphson per step |

## 4. Key Algorithms

### 4.1 Modified Nodal Analysis (MNA)
Circuit equations are formulated as `A·x = b` where:
- `x` contains node voltages and voltage source branch currents
- `A` is the MNA matrix assembled from device stamps
- `b` is the RHS vector containing independent source values

**Device Stamps** (examples):
- **Resistor**: `[[g, -g], [-g, g]]`
- **Voltage Source**: Adds row/column for branch current, RHS = V
- **VCCS**: Off-diagonal transconductance terms

### 4.2 Newton-Raphson Iteration
For nonlinear circuits (diodes):
1. Evaluate device currents and dynamic conductances at current voltage estimate
2. Linearize: `I ≈ g·V + I_hist`
3. Assemble matrix and solve
4. Update voltages and check convergence: `ΔV < vntol`
5. Repeat until convergence or max iterations

### 4.3 Trapezoidal Integration (Transient)
Capacitors/inductors are discretized:
- **Capacitor**: `i = C·dv/dt ≈ (C/Δt)·(v(t) - v(t-Δt))`
- Equivalent circuit: Conductance `G_eq = C/Δt` in parallel with history current source `I_hist = G_eq·v(t-Δt)`
- History values stored in `device->params` and updated after each converged step

## 5. Data Flow
```
1. Parse netlist → Create Circuit struct
2. Resolve branch current indices → Allocate equation numbers
3. Initialize circuit → Allocate voltage/current arrays
4. Run analyses:
   a. Setup matrix size
   b. For each analysis step:
      i. Clear matrix & RHS
      ii. Load all devices (linear + nonlinear)
      iii. Add GMIN for stability
      iv. Factor & solve sparse system
      v. Check convergence
      vi. Update state (transient history)
   c. Output results
5. Cleanup & free memory
```

## 6. File Structure
```
ngspice-rewrite/
├── include/
│   ├── spice_types.h    # Common types, constants, enums
│   ├── circuit.h        # Circuit, Node, Device, Model structs
│   ├── sparse.h         # SparseMatrix, element structs
│   ├── device.h         # DeviceOps interface
│   ├── analysis.h       # AnalysisOps interface
│   ├── parser.h         # Parser API
│   └── output.h         # Output API
├── src/
│   ├── main.c           # CLI entry point, argument parsing
│   ├── core/
│   │   └── circuit.c    # Circuit lifecycle, node/device management
│   ├── parser/
│   │   └── parser.c     # Netlist tokenizer & parser
│   ├── devices/
│   │   ├── res.c ... dio.c  # Device implementations
│   │   └── devreg.c     # Device registration system
│   ├── analysis/
│   │   ├── dcop.c ... dctran.c  # Analysis implementations
│   │   └── anareg.c     # Analysis registration system
│   ├── math/
│   │   └── sparse.c     # Sparse LU solver
│   └── output/
│       └── output.c     # Results formatting & raw export
├── tests/               # Example netlists
├── Makefile             # Build system
├── README.md            # User guide
└── DESIGN.md            # This document
```

## 7. Limitations & Future Work
### Current Limitations
- Only DC independent sources (no AC, pulse, sinusoidal waveforms)
- No subcircuit (`.SUBCKT`) or hierarchical design
- No temperature-dependent analysis
- Limited convergence diagnostics
- Single-threaded execution
- No noise, Fourier, or pole-zero analysis

### Potential Extensions
- **Waveform Sources**: Add `SIN`, `PULSE`, `EXP`, `PWL` generators
- **Subcircuits**: Implement `.SUBCKT` parsing and node renaming
- **Advanced Solvers**: Replace full pivoting with Markowitz sparsity optimization
- **Complex Numbers**: Proper AC analysis with magnitude/phase tracking
- **BJT/MOSFET Models**: Add semiconductor device models
- **GUI/Plotting**: Integrate with gnuplot or build a simple waveform viewer
- **Parallelization**: Multi-core support for large circuits

## 8. References
- ngspice-45 Manual & Source Code (https://ngspice.sourceforge.io/)
- "The SPICE Book" by Andrei Vladimirescu
- "Computer-Aided Analysis of Electronic Circuits" by Loebel et al.

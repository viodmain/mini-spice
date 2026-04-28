# mini-spice Design Document

## 1. Overview
`mini-spice` is a lightweight, educational SPICE circuit simulator inspired by the ngspice-45 architecture. It implements core circuit simulation algorithms including Modified Nodal Analysis (MNA), Newton-Raphson iteration for nonlinear convergence, and trapezoidal integration for transient analysis. The simulator supports linear and nonlinear devices, multiple analysis types, waveform sources, semiconductor devices (BJT/MOSFET), behavioral sources, and advanced analyses (Noise, Fourier, Sensitivity).

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
- **Core**: Circuit representation, node/equation mapping, parameter/subcircuit management
- **Backend**: Matrix assembly, device models, analysis algorithms
- **Math**: Sparse linear algebra, numerical integration, DFT

## 3. Core Modules

### 3.1 Netlist Parser (`src/parser/parser.c`)
- **Type**: Single-pass, token-based parser with continuation line support
- **Features**:
  - Reads SPICE-compatible netlists
  - Supports device prefixes: `R`, `C`, `L`, `V`, `I`, `G`, `E`, `F`, `H`, `D`, `Q`, `M`, `B`, `S`, `W`, `T`, `X`
  - Parses dot-commands: `.OP`, `.DC`, `.AC`, `.TRAN`, `.MODEL`, `.TEMP`, `.PARAM`, `.STEP`, `.OPTIONS`, `.PRINT`, `.IC`, `.NOISE`, `.FOUR`, `.SENS`, `.SUBCKT`/`.ENDS`
  - Handles value suffixes: `T`, `G`, `MEG`, `K`, `m`, `u`, `n`, `p`, `f`
  - Supports `DC`, `AC` qualifiers and waveform keywords: `SIN`, `PULSE`, `PWL`, `EXP`
  - Supports continuation lines (`+`) for long statements
  - Parameter substitution for `{param}` syntax

### 3.2 Circuit Data Structures (`include/circuit.h`, `src/core/circuit.c`)
- **`Circuit`**: Top-level container holding nodes, devices, models, analyses, parameters, subcircuits, tolerances, and temperature
- **`Node`**: Linked list of circuit nodes with name, internal number, equation index (`eqnum`), and initial condition support (`init_voltage`, `has_init`)
- **`Device`**: Instance data including type, node connections (`n1`-`n5`), values, model pointers, waveform parameters, behavioral expressions, and subcircuit mapping
- **`Model`**: Parameter sets for device types (Diode, BJT, MOSFET, Switch, Transmission Line)
- **`Analysis`**: Linked list of requested analyses with sweep/transform parameters
- **`Param`**: Key-value store for `.PARAM` definitions
- **`SubcktDef`**: Subcircuit definition with port names and internal device/node lists
- **`waveform_params_t`**: Unified structure for SIN, PULSE, PWL, EXP, and AC waveforms
- **Equation Mapping**:
  - Node voltages map to equation indices `0` to `N-1`
  - Voltage source branch currents are allocated separately and resolved after parsing

### 3.3 Sparse Matrix Solver (`include/sparse.h`, `src/math/sparse.c`)
- **Structure**: Row/column linked lists for non-zero elements
- **RHS Handling**: Separate `rhs[]` array (not embedded in matrix)
- **Factorization**: LU decomposition with **full pivoting** (searches entire unassigned submatrix for largest element)
- **Solve**: Forward/backward substitution with row/column permutation tracking
- **Stability**: GMIN (default 1e-12 S) automatically added to ground for floating nodes

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
- **Nonlinear Devices** (D, Q, M): Implement `nonlinear()` for Newton-Raphson iteration
- **Behavioral Sources** (B): Evaluate expressions at load time, support `V()`, `I()`, `TIME`
- **Switches** (S, W): Piecewise-linear conductance based on control voltage/current
- **Transmission Lines** (T): Method of Characteristics with history buffer
- **Registration**: Centralized in `devreg.c` via function pointer table

### 3.5 Analysis Engines (`src/analysis/*.c`)
| Analysis | File | Algorithm |
|----------|------|-----------|
| DC OP | `dcop.c` | Newton-Raphson iteration until voltage change < `vntol` |
| DC Sweep | `dcsweep.c` | Loop over source values, run DC OP at each step |
| AC | `acan.c` | Linearize around DC OP, solve complex system per frequency |
| Transient | `dctran.c` | Time stepping, capacitor/inductor history update, Newton-Raphson per step |
| Noise | `noise.c` | Superimpose thermal (4kT/R) and shot (2qI) noise sources |
| Fourier | `fourier.c` | Run transient for one period, compute DFT for harmonics & THD |
| Sensitivity | `sens.c` | Perturb each component by 1%, re-solve DC OP, compute dV/dR |

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
- **BJT/MOSFET**: 3/4-terminal Jacobian matrices with gm, gds, gpi, gmb

### 4.2 Newton-Raphson Iteration
For nonlinear circuits (diodes, BJTs, MOSFETs):
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

### 4.4 Waveform Evaluation
Time-varying sources are evaluated at each transient time step:
- **SIN**: `Voffset + Amp·exp(-Theta·τ)·sin(2πf·τ + φ)`
- **PULSE**: Piecewise linear ramp up, flat top, ramp down, repeat per period
- **PWL**: Linear interpolation between user-defined points
- **EXP**: Dual exponential rise/fall with separate time constants

### 4.5 Discrete Fourier Transform (Fourier Analysis)
- Collects one period of transient data (default 1000 points)
- Computes `a_h = (2/N)·Σ v[n]·cos(2πh·n/N)` and `b_h = (2/N)·Σ v[n]·sin(2πh·n/N)`
- Magnitude: `√(a_h² + b_h²)`, Phase: `atan2(-b_h, a_h)`
- THD: `√(Σ harmonics²) / fundamental × 100%`

### 4.6 Noise Analysis
- **Thermal Noise**: `i_n² = 4kT/R` for resistors
- **Shot Noise**: `i_n² = 2qI_D` for diodes/BJTs
- Superposition assumes uncorrelated sources; output noise density computed per frequency point

### 4.7 Sensitivity Analysis
- **Perturbation Method**: `S = (V_out(R+ΔR) - V_out(R)) / ΔR`
- Relative sensitivity: `S_rel = S × R / V_out`
- Computes for all resistors and voltage sources

## 5. Data Flow
```
1. Parse netlist → Create Circuit struct
2. Resolve branch current indices → Allocate equation numbers
3. Initialize circuit → Allocate voltage/current arrays, apply .IC
4. Run analyses:
   a. Setup matrix size
   b. For each analysis step:
      i. Clear matrix & RHS
      ii. Load all devices (linear + nonlinear)
      iii. Add GMIN for stability
      iv. Factor & solve sparse system
      v. Check convergence
      vi. Update state (transient history, waveform evaluation)
   c. Output results
5. Cleanup & free memory
```

## 6. File Structure
```
ngspice-rewrite/
├── include/
│   ├── spice_types.h    # Common types, constants, enums, sim_options_t
│   ├── circuit.h        # Circuit, Node, Device, Model, Param, SubcktDef, waveform_params_t
│   ├── sparse.h         # SparseMatrix, element structs
│   ├── device.h         # DeviceOps interface
│   ├── analysis.h       # AnalysisOps interface
│   ├── parser.h         # Parser API
│   └── output.h         # Output API
├── src/
│   ├── main.c           # CLI entry point, argument parsing
│   ├── core/
│   │   └── circuit.c    # Circuit lifecycle, node/device/param/subckt management, waveform_eval()
│   ├── parser/
│   │   └── parser.c     # Netlist tokenizer & parser (extended)
│   ├── devices/
│   │   ├── res.c ... dio.c  # Device implementations
│   │   ├── bjt.c        # BJT (NPN/PNP) Ebers-Moll model
│   │   ├── mos.c        # MOSFET (NMOS/PMOS) Level 1 model
│   │   ├── behsrc.c     # Behavioral sources (B)
│   │   ├── switch.c     # Voltage/Current controlled switches (S/W)
│   │   ├── tline.c      # Lossless transmission line (T)
│   │   └── devreg.c     # Device registration system
│   ├── analysis/
│   │   ├── dcop.c ... dctran.c  # Analysis implementations
│   │   ├── noise.c      # Noise analysis
│   │   ├── fourier.c    # Fourier analysis
│   │   ├── sens.c       # Sensitivity analysis
│   │   └── anareg.c     # Analysis registration system
│   ├── math/
│   │   └── sparse.c     # Sparse LU solver
│   └── output/
│       └── output.c     # Results formatting & raw export
├── tests/               # Example netlists (voltage_divider, bjt_amp, mos_amp, sin_source, etc.)
├── Makefile             # Build system
├── README.md            # User guide
├── DESIGN.md            # This document
└── FEATURES.md          # Feature documentation
```

## 7. Limitations & Future Work
### Current Limitations
- Subcircuit instantiation (X-prefix) node mapping needs refinement
- Waveform sources parse correctly but transient integration could be optimized for stiff waveforms
- BJT/MOSFET models are simplified (no temperature dependence, breakdown, or advanced second-order effects)
- Behavioral source expression parser is limited to basic functions and node references
- No temperature sweep (`.STEP TEMP`)
- Single-threaded execution
- Output limited to terminal; no SPICE raw/waveform file export

### Potential Extensions
- **Advanced Solvers**: Replace full pivoting with Markowitz sparsity optimization
- **Complex Numbers**: Proper AC analysis with magnitude/phase tracking in matrix
- **Advanced Models**: BSIM MOSFET, Gummel-Poon BJT, thermal noise in transient
- **GUI/Plotting**: Integrate with gnuplot or build a simple waveform viewer
- **Parallelization**: Multi-core support for large circuits and parametric sweeps
- **Export Formats**: SPICE raw, CSV, or HDF5 output

## 8. References
- ngspice-45 Manual & Source Code (https://ngspice.sourceforge.io/)
- "The SPICE Book" by Andrei Vladimirescu
- "Computer-Aided Analysis of Electronic Circuits" by Loebel et al.
- "Microelectronic Circuits" by Sedra/Smith

# mini-spice

A minimal SPICE circuit simulator based on ngspice-45 architecture.

## Features

### Supported Devices
- **Resistors** (R) - Linear resistors
- **Capacitors** (C) - Linear capacitors
- **Inductors** (L) - Linear inductors
- **Voltage Sources** (V) - Independent DC voltage sources
- **Current Sources** (I) - Independent DC current sources
- **VCCS** (G) - Voltage-Controlled Current Source
- **VCVS** (E) - Voltage-Controlled Voltage Source
- **CCCS** (F) - Current-Controlled Current Source
- **CCVS** (H) - Current-Controlled Voltage Source
- **Diodes** (D) - PN junction diodes with model parameters

### Supported Analyses
- **.OP** - DC Operating Point Analysis
- **.DC** - DC Transfer Characteristic (voltage/current sweep)
- **.AC** - AC Small-Signal Analysis (linear, decade, octave sweeps)
- **.TRAN** - Transient Analysis (time-domain simulation)

### Key Algorithms
- **Sparse Matrix Solver** - LU decomposition with full pivoting
- **Newton-Raphson Iteration** - For nonlinear DC convergence
- **Trapezoidal Integration** - For transient analysis of capacitors/inductors
- **Modified Nodal Analysis (MNA)** - Circuit equation formulation

## Building

```bash
cd ngspice-rewrite
make
```

This produces the `mini-spice` executable.

## Usage

```bash
./mini-spice <netlist_file>
```

### Options
- `-h, --help` - Show help
- `-v, --version` - Show version
- `-o <file>` - Output file (raw format)
- `-T <temp>` - Set temperature in Celsius (default: 27°C)

## Netlist Format

### Basic Syntax
```
<title line>
<device lines>
<.analysis statements>
.end
```

### Device Lines
```
R<name> <node1> <node2> <resistance>
C<name> <node1> <node2> <capacitance>
L<name> <node1> <node2> <inductance>
V<name> <node1> <node2> DC <voltage> [AC <magnitude>]
I<name> <node1> <node2> <current>
G<name> <out+> <out-> <ctrl+> <ctrl-> <transconductance>
E<name> <out+> <out-> <ctrl+> <ctrl-> <voltage_gain>
F<name> <out+> <out-> <Vctrl> <current_gain>
H<name> <out+> <out-> <Vctrl> <transresistance>
D<name> <anode> <cathode> <model_name>
```

### Model Lines
```
.model <name> D(IS=<saturation_current> N=<emission_coeff> RS=<ohmic_resistance> ...)
```

### Analysis Statements
```
.op                              # DC operating point
.dc <source> <start> <stop> <step>  # DC sweep
.ac <type> <points> <start> <stop>  # AC analysis (type: lin/dec/oct)
.tran <tstep> <tstop>            # Transient analysis
.temp <temperature_celsius>      # Set temperature
```

### Value Suffixes
- `k` or `K` - kilo (1e3)
- `meg` or `MEG` - mega (1e6)
- `m` or `M` - milli (1e-3)
- `u` or `U` - micro (1e-6)
- `n` or `N` - nano (1e-9)
- `p` or `P` - pico (1e-12)
- `f` or `F` - femto (1e-15)

## Examples

### Voltage Divider
```
Voltage Divider
V1 in 0 DC 10
R1 in out 1k
R2 out 0 1k

.op
.end
```

### RC Low-Pass Filter
```
RC Low-Pass Filter
V1 in 0 DC 5
R1 in out 1k
C1 out 0 1u

.op
.ac dec 10 10 100k
.tran 10u 1m
.end
```

### DC Sweep
```
DC Sweep Test
V1 in 0 DC 0
R1 in out 1k
R2 out 0 1k

.dc V1 0 10 0.5
.end
```

## Architecture

```
ngspice-rewrite/
├── include/
│   ├── spice_types.h    # Common type definitions
│   ├── circuit.h        # Circuit data structures
│   ├── sparse.h         # Sparse matrix interface
│   ├── device.h         # Device model interface
│   ├── analysis.h       # Analysis interface
│   ├── parser.h         # Netlist parser interface
│   └── output.h         # Output interface
├── src/
│   ├── main.c           # Main entry point
│   ├── core/
│   │   └── circuit.c    # Circuit management
│   ├── parser/
│   │   └── parser.c     # SPICE netlist parser
│   ├── devices/
│   │   ├── res.c        # Resistor model
│   │   ├── cap.c        # Capacitor model
│   │   ├── ind.c        # Inductor model
│   │   ├── vsrc.c       # Voltage source model
│   │   ├── isrc.c       # Current source model
│   │   ├── vccs.c       # VCCS model
│   │   ├── vcvs.c       # VCVS model
│   │   ├── cccs.c       # CCCS model
│   │   ├── ccvs.c       # CCVS model
│   │   ├── dio.c        # Diode model
│   │   └── devreg.c     # Device registration
│   ├── analysis/
│   │   ├── dcop.c       # DC operating point
│   │   ├── dcsweep.c    # DC sweep
│   │   ├── acan.c       # AC analysis
│   │   ├── dctran.c     # Transient analysis
│   │   └── anareg.c     # Analysis registration
│   ├── math/
│   │   └── sparse.c     # Sparse matrix solver
│   └── output/
│       └── output.c     # Output handling
├── tests/               # Test netlists
└── Makefile
```

## Limitations

- Only DC voltage sources (no AC, pulse, sinusoidal, etc.)
- No subcircuit support (.SUBCKT)
- No temperature analysis
- Limited convergence diagnostics
- No noise analysis
- No Fourier analysis
- No pole-zero analysis
- Single-threaded execution

## Testing

```bash
make test
```

Or run individual tests:
```bash
./mini-spice tests/voltage_divider.net
./mini-spice tests/rc_filter.net
./mini-spice tests/dc_sweep.net
```

## License

Based on ngspice-45 architecture. See ngspice license for details.

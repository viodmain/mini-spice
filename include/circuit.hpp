/**
 * @file circuit.hpp
 * @brief Circuit data structures (C++17 version)
 *
 * This header defines the core circuit data structures using modern C++ features:
 * - std::string for names instead of char*
 * - std::vector instead of linked lists
 * - std::unique_ptr for owned resources
 * - RAII for automatic memory management
 *
 * The Circuit class serves as the top-level container holding all circuit
 * information including nodes, devices, models, analyses, and simulation state.
 */
#ifndef CIRCUIT_HPP
#define CIRCUIT_HPP

#include "spice_types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <unordered_map>

namespace spice {

//=============================================================================
// Forward Declarations
//=============================================================================

class Node;
class Device;
class Model;
class Analysis;
class SubcktDef;
class WaveformParams;

//=============================================================================
// Node Class
//=============================================================================

/**
 * @brief Represents a circuit node
 *
 * Each node has a unique name, an internal number, and an equation number
 * used in the MNA matrix. Ground node (0 or gnd) has eqnum = -1.
 *
 * Supports initial conditions via .IC command.
 */
class Node {
public:
    std::string name;         /**< Node name (e.g., "in", "out", "1") */
    Index number;             /**< Internal node number (unique ID) */
    Index eqnum;              /**< Equation number in MNA matrix (-1 for ground) */
    bool is_ground;           /**< True if this is the ground node */
    bool is_vsource;          /**< True if connected to voltage source branch */
    Real init_voltage;        /**< Initial condition voltage (for .IC) */
    bool has_init;            /**< True if initial condition is specified */

    /**
     * @brief Construct a new Node
     * @param name_ Node name
     * @param number_ Internal node number
     */
    Node(std::string name_, Index number_);

    /**
     * @brief Check if this node is the ground node
     * @return True if ground
     */
    bool isGround() const { return is_ground; }

    /**
     * @brief Get equation number (returns -1 for ground)
     * @return Equation number or -1
     */
    Index getEqNum() const { return eqnum; }
};

//=============================================================================
// Waveform Parameters
//=============================================================================

/**
 * @brief Parameters for time-varying voltage/current source waveforms
 *
 * Supports SIN, PULSE, PWL, EXP, and AC waveforms.
 * Uses std::vector for PWL points instead of raw arrays.
 */
class WaveformParams {
public:
    WaveformType type;

    // SIN parameters
    Real sin_voffset = 0.0;   /**< Offset voltage */
    Real sin_vamp = 0.0;      /**< Amplitude */
    Real sin_freq = 0.0;      /**< Frequency (Hz) */
    Real sin_td = 0.0;        /**< Delay time */
    Real sin_theta = 0.0;     /**< Damping factor */
    Real sin_phi = 0.0;       /**< Phase (degrees) */

    // PULSE parameters
    Real pulse_v1 = 0.0;      /**< Initial value */
    Real pulse_v2 = 0.0;      /**< Pulsed value */
    Real pulse_td = 0.0;      /**< Delay time */
    Real pulse_tr = 0.0;      /**< Rise time */
    Real pulse_tf = 0.0;      /**< Fall time */
    Real pulse_pw = 0.0;      /**< Pulse width */
    Real pulse_per = 0.0;     /**< Period */

    // PWL parameters (using vector instead of raw arrays)
    std::vector<Real> pwl_time;    /**< Time values */
    std::vector<Real> pwl_value;   /**< Value at each time */

    // EXP parameters
    Real exp_v1 = 0.0;      /**< Initial value */
    Real exp_v2 = 0.0;      /**< Peak value */
    Real exp_td1 = 0.0;     /**< Rise delay time */
    Real exp_tau1 = 0.0;    /**< Rise time constant */
    Real exp_td2 = 0.0;     /**< Fall delay time */
    Real exp_tau2 = 0.0;    /**< Fall time constant */

    // AC parameters
    Real ac_mag = 0.0;      /**< AC magnitude */
    Real ac_phase = 0.0;    /**< AC phase (degrees) */

    /**
     * @brief Construct waveform with given type
     * @param type_ Waveform type
     */
    explicit WaveformParams(WaveformType type_ = WaveformType::NONE);

    /**
     * @brief Evaluate waveform at time t
     * @param t Simulation time
     * @return Instantaneous value
     */
    Real evaluate(Real t) const;
};

//=============================================================================
// Device Class
//=============================================================================

/**
 * @brief Represents a circuit device instance
 *
 * Stores device type, node connections, values, model reference,
 * waveform parameters, and behavioral expressions.
 *
 * Node connections (n1, n2, n3, n4, n5) store equation numbers after parsing.
 * A value of -1 indicates ground or unused connection.
 */
class Device {
public:
    std::string name;               /**< Device instance name (R1, C2, Q1, etc.) */
    DeviceType type;                /**< Device type enum */
    Index n1, n2;                   /**< Primary node connections */
    Index n3, n4, n5;               /**< Additional nodes (dependent sources, etc.) */
    Real value;                     /**< Primary value (R, C, L, gain, etc.) */
    Real value2;                    /**< Secondary value (e.g., MOSFET L) */

    std::shared_ptr<Model> model;   /**< Model pointer (for D, Q, M, S, W, T) */
    std::unique_ptr<WaveformParams> waveform; /**< Time-varying waveform */

    std::string expr;               /**< Behavioral expression string (for B sources) */
    void* params = nullptr;         /**< Device-specific instance state (owned) */

    // Subcircuit specific
    SubcktDef* subckt_def = nullptr;        /**< Subcircuit definition (non-owning) */
    std::vector<std::unique_ptr<Device>> sub_devices; /**< Devices inside subcircuit */
    std::vector<Index> port_map;            /**< Port-to-node mapping */

    /**
     * @brief Construct a new Device
     * @param name_ Device name
     * @param type_ Device type
     * @param n1_ First node equation number
     * @param n2_ Second node equation number
     * @param value_ Primary value
     */
    Device(std::string name_, DeviceType type_, Index n1_, Index n2_, Real value_);

    /**
     * @brief Get node voltage (handles ground case)
     * @param ckt Reference to circuit
     * @param nodeEq Node equation number
     * @return Voltage at node (0.0 for ground)
     */
    static Real getNodeVoltage(const class Circuit& ckt, Index nodeEq);

    /**
     * @brief Get voltage across two nodes
     * @param ckt Reference to circuit
     * @return Voltage difference (V(n1) - V(n2))
     */
    Real getVoltageAcross(const class Circuit& ckt) const;
};

//=============================================================================
// Model Parameter Structures
//=============================================================================

/**
 * @brief Diode model parameters
 *
 * Implements the standard SPICE diode model with saturation current,
 * emission coefficient, series resistance, and junction capacitance.
 */
struct DiodeModelParams {
    Real is = 1.0e-14;    /**< Saturation current (A) */
    Real n = 1.0;         /**< Emission coefficient */
    Real rs = 0.0;        /**< Ohmic resistance (Ω) */
    Real cjo = 0.0;       /**< Zero-bias junction capacitance (F) */
    Real vj = 0.7;        /**< Junction potential (V) */
    Real m = 0.5;         /**< Grading coefficient */
    Real tt = 0.0;        /**< Transit time (s) */
    Real eg = 1.11;       /**< Activation energy (eV) */
    Real xti = 3.0;       /**< Temperature exponent */
    Real kf = 0.0;        /**< Flicker noise coefficient */
    Real af = 1.0;        /**< Flicker noise exponent */
    Real fc = 0.5;        /**< Forward bias coefficient */
};

/**
 * @brief BJT model parameters (simplified Ebers-Moll)
 *
 * Supports both NPN and PNP transistors with forward/reverse beta,
 * parasitic resistances, junction capacitances, and transit times.
 */
struct BjtModelParams {
    int polarity = 1;       /**< 1 = NPN, -1 = PNP */

    // Transport parameters
    Real is = 1.0e-14;    /**< Saturation current (A) */
    Real bf = 100.0;      /**< Ideal maximum forward beta */
    Real nf = 1.0;        /**< Forward emission coefficient */
    Real br = 1.0;        /**< Ideal maximum reverse beta */
    Real nr = 1.0;        /**< Reverse emission coefficient */

    // Parasitic resistances
    Real rb = 0.0;        /**< Ohmic base resistance (Ω) */
    Real re = 0.0;        /**< Ohmic emitter resistance (Ω) */
    Real rc = 0.0;        /**< Ohmic collector resistance (Ω) */

    // Junction capacitances
    Real cje = 0.0;       /**< Base-emitter zero-bias capacitance (F) */
    Real vje = 0.75;      /**< Base-emitter built-in potential (V) */
    Real me = 0.33;       /**< Base-emitter grading coefficient */
    Real cjcs = 0.0;      /**< Base-collector substrate capacitance (F) */
    Real vjc = 0.75;      /**< Base-collector built-in potential (V) */
    Real mc = 0.33;       /**< Base-collector grading coefficient */

    // Transit times
    Real tf = 0.0;        /**< Ideal forward transit time (s) */
    Real tr = 0.0;        /**< Ideal reverse transit time (s) */

    // Breakdown
    Real bvc = 0.0;       /**< Base-collector breakdown voltage (V) */
    Real bve = 0.0;       /**< Base-emitter breakdown voltage (V) */
    Real ibvc = 0.0;      /**< Current at BVC breakdown (A) */
    Real ibve = 0.0;      /**< Current at BVE breakdown (A) */

    // Temperature
    Real eg = 1.11;       /**< Activation energy (eV) */
    Real xti = 3.0;       /**< Temperature exponent for IS */
};

/**
 * @brief MOSFET model parameters (Level 1 - Shichman-Hodges)
 *
 * Supports NMOS and PMOS with threshold voltage, body effect,
 * channel length modulation, and geometric parameters.
 */
struct MosModelParams {
    int polarity = 1;       /**< 1 = NMOS, -1 = PMOS */

    // Process parameters
    Real kp = 1.0e-4;     /**< Transconductance parameter (A/V²) */
    Real vto = 0.7;       /**< Threshold voltage (V) */
    Real gamma = 0.0;     /**< Body effect parameter (√V) */
    Real lambda = 0.0;    /**< Channel length modulation (1/V) */
    Real phi = 0.6;       /**< Surface potential (V) */

    // Geometric parameters (can be overridden per instance)
    Real w = 1.0e-5;      /**< Channel width (m) */
    Real l = 1.0e-6;      /**< Channel length (m) */

    // Junction capacitances
    Real cj = 0.0;        /**< Bottom junction capacitance (F/m²) */
    Real mj = 0.5;        /**< Bottom junction grading coefficient */
    Real cjsw = 0.0;      /**< Side junction capacitance (F/m) */
    Real mjs = 0.5;       /**< Side junction grading coefficient */
    Real cjo = 0.0;       /**< Zero-bias depletion capacitance (F) */

    // Resistances
    Real rd = 0.0;        /**< Drain ohmic resistance (Ω) */
    Real rs = 0.0;        /**< Source ohmic resistance (Ω) */
    Real rb = 0.0;        /**< Gate ohmic resistance (Ω) */

    // Mobility and velocity
    Real u0 = 0.0;        /**< Surface mobility */
    Real vmax = 0.0;      /**< Maximum lateral velocity */

    // Temperature
    Real eg = 1.11;       /**< Activation energy (eV) */
    Real xti = 3.0;       /**< Temperature exponent */
};

/**
 * @brief Switch model parameters
 *
 * Supports both voltage-controlled (S) and current-controlled (W) switches
 * with on/off resistance and hysteresis.
 */
struct SwitchModelParams {
    Real ron = 1.0;       /**< On resistance (Ω) */
    Real roff = 1.0e6;    /**< Off resistance (Ω) */
    Real vt = 0.5;        /**< Threshold voltage (V) */
    Real vh = 0.0;        /**< Hysteresis voltage (V) */
};

/**
 * @brief Transmission line parameters
 *
 * Implements lossless transmission line using Method of Characteristics.
 */
struct TransmissionLineParams {
    Real td = 1.0e-9;     /**< Delay time (s) */
    Real z0 = 50.0;       /**< Characteristic impedance (Ω) */
    Real f = 0.0;         /**< Frequency (for lossy, unused) */
    Real n = 1.0;         /**< Number of segments (unused) */
};

//=============================================================================
// Model Class
//=============================================================================

/**
 * @brief Represents a device model (shared parameter set)
 *
 * Models are shared between device instances of the same type.
 * For example, all 2N2222 BJTs share the same model parameters.
 *
 * Uses std::variant to store different model parameter types.
 */
class Model {
public:
    std::string name;                     /**< Model name */
    DeviceType type;                      /**< Device type this model is for */

    /**
     * @brief Model parameters stored as variant
     *
     * Holds the appropriate parameter struct based on device type:
     * - Diode: DiodeModelParams
     * - BJT: BjtModelParams
     * - MOSFET: MosModelParams
     * - Switch: SwitchModelParams
     * - Transmission line: TransmissionLineParams
     */
    std::variant<
        std::monostate,
        DiodeModelParams,
        BjtModelParams,
        MosModelParams,
        SwitchModelParams,
        TransmissionLineParams
    > params;

    /**
     * @brief Construct a new Model
     * @param name_ Model name
     * @param type_ Device type
     */
    Model(std::string name_, DeviceType type_);

    /**
     * @brief Get typed parameters (diode)
     * @return Pointer to diode parameters, or nullptr if wrong type
     */
    DiodeModelParams* getDiodeParams();

    /**
     * @brief Get typed parameters (BJT)
     * @return Pointer to BJT parameters, or nullptr if wrong type
     */
    BjtModelParams* getBjtParams();

    /**
     * @brief Get typed parameters (MOSFET)
     * @return Pointer to MOSFET parameters, or nullptr if wrong type
     */
    MosModelParams* getMosParams();

    /**
     * @brief Get typed parameters (switch)
     * @return Pointer to switch parameters, or nullptr if wrong type
     */
    SwitchModelParams* getSwitchParams();

    /**
     * @brief Get typed parameters (transmission line)
     * @return Pointer to transmission line parameters, or nullptr if wrong type
     */
    TransmissionLineParams* getTlineParams();

    /**
     * @brief Set parameter by name
     * @param param Parameter name (e.g., "is", "bf", "vto")
     * @param value Parameter value
     * @return ErrorCode
     */
    ErrorCode setParam(const std::string& param, Real value);
};

//=============================================================================
// Subcircuit Definition
//=============================================================================

/**
 * @brief Represents a subcircuit definition (.SUBCKT/.ENDS)
 *
 * Contains port names and internal device/node lists.
 * Devices and nodes are owned by this class via unique_ptr.
 */
class SubcktDef {
public:
    std::string name;                                     /**< Subcircuit name */
    std::vector<std::string> port_names;                  /**< Port names */
    std::vector<std::unique_ptr<Device>> devices;         /**< Internal devices */
    std::vector<std::unique_ptr<Node>> nodes;             /**< Internal nodes */

    /**
     * @brief Construct a new SubcktDef
     * @param name_ Subcircuit name
     */
    explicit SubcktDef(std::string name_);

    /**
     * @brief Get number of ports
     * @return Port count
     */
    size_t getNumPorts() const { return port_names.size(); }
};

//=============================================================================
// Sweep Type
//=============================================================================

/**
 * @brief Sweep type for DC/AC/Noise analysis
 */
enum class SweepType : int {
    LINEAR,     /**< Linear sweep */
    OCTAVE,     /**< Octave sweep */
    DECADE      /**< Decade sweep */
};

//=============================================================================
// Analysis Parameters
//=============================================================================

/**
 * @brief Parameters for each analysis type
 *
 * Uses std::optional for parameters that are only valid for specific analyses.
 */
class AnalysisParams {
public:
    AnalysisType type;

    // DC sweep parameters
    std::string src_name;             /**< Source to sweep */
    Real start = 0.0, stop = 0.0;     /**< Sweep range */
    Real step = 0.0;                  /**< Sweep step */
    SweepType sweep_type = SweepType::LINEAR;

    // AC analysis parameters
    Real ac_start = 0.0, ac_stop = 0.0;
    Real ac_points = 0.0;
    SweepType ac_sweep_type = SweepType::DECADE;

    // Transient parameters
    Real tstart = 0.0, tstop = 0.0;
    Real tstep = 0.0;
    Real tmax = 0.0;
    IntegrationMethod integration = IntegrationMethod::TRAPEZOIDAL;
    bool use_uic = false;             /**< Use initial conditions (skip DC OP) */

    // Noise analysis parameters
    std::string noise_output;         /**< Output node */
    std::string noise_src;            /**< Input source */
    Real noise_start = 0.0, noise_stop = 0.0;
    Real noise_points = 0.0;
    SweepType noise_sweep_type = SweepType::DECADE;

    // Fourier analysis parameters
    Real four_freq = 0.0;             /**< Fundamental frequency */
    int four_harmonics = 9;           /**< Number of harmonics */

    // Sensitivity analysis
    std::string sens_output;          /**< Output variable */

    // Pole-zero analysis
    std::string pz_input;             /**< Input source */
    std::string pz_output;            /**< Output node */

    /**
     * @brief Construct analysis parameters with given type
     * @param type_ Analysis type
     */
    explicit AnalysisParams(AnalysisType type_);
};

//=============================================================================
// Analysis Class
//=============================================================================

/**
 * @brief Represents an analysis request
 *
 * Wraps AnalysisParams and provides lifecycle management.
 */
class Analysis {
public:
    AnalysisParams params;    /**< Analysis parameters */

    /**
     * @brief Construct a new Analysis
     * @param type_ Analysis type
     */
    explicit Analysis(AnalysisType type_);
};

//=============================================================================
// Parameter (.PARAM)
//=============================================================================

/**
 * @brief Represents a .PARAM definition
 *
 * Key-value pair for parameterized netlists.
 */
struct Param {
    std::string name;     /**< Parameter name */
    Real value;           /**< Parameter value */
};

//=============================================================================
// Circuit Class
//=============================================================================

/**
 * @brief Top-level circuit container
 *
 * Holds all circuit information: nodes, devices, models, analyses, parameters,
 * subcircuit definitions, simulation state, and results.
 *
 * Uses RAII: all owned resources are automatically freed when Circuit is destroyed.
 */
class Circuit {
public:
    std::string title;                          /**< Circuit title */

    // Owned resources (using unique_ptr for automatic cleanup)
    std::vector<std::unique_ptr<Node>> nodes;           /**< All nodes */
    std::vector<std::unique_ptr<Device>> devices;       /**< All devices */
    std::vector<std::shared_ptr<Model>> models;         /**< All models (shared) */
    std::vector<std::unique_ptr<Analysis>> analyses;    /**< All analyses */
    std::vector<Param> params;                          /**< .PARAM definitions */
    std::vector<std::unique_ptr<SubcktDef>> subckts;    /**< Subcircuit definitions */

    // Node and equation counts
    Index num_nodes = 0;              /**< Number of nodes */
    Index num_eqns = 0;               /**< Number of equations */

    // Results storage
    std::vector<Real> voltage;        /**< Node voltages (indexed by eqnum) */
    std::vector<Real> current;        /**< Voltage source currents */

    // Simulation state
    Real time = 0.0;                  /**< Current simulation time */
    Real temp = TNOM_K;               /**< Temperature (Kelvin) */
    Real temp_celsius = TNOM_C;       /**< Temperature (Celsius) */

    // Tolerances (for quick access)
    Real abstol = DEFAULT_ABSTOL;
    Real reltol = DEFAULT_RELTOL;
    Real vntol = DEFAULT_VNTOL;
    Real trtol = DEFAULT_TRTOL;
    int maxiter = DEFAULT_MAXITER;
    int trmaxiter = DEFAULT_MAXITER;
    Real gmin = DEFAULT_GMIN;

    // Simulation options
    SimulationOptions options;

    // Voltage source branch current management
    int num_vsources = 0;             /**< Number of voltage sources */
    int next_vsrc_branch = 0;         /**< Next branch current index */

    // Output specification
    bool print_all = true;            /**< Print all nodes if no .PRINT */
    std::vector<std::string> print_nodes;   /**< Nodes to print */

    /**
     * @brief Construct a new Circuit
     * @param title_ Circuit title
     */
    explicit Circuit(std::string title_ = "Untitled");

    /**
     * @brief Destructor - resources are automatically freed via RAII
     */
    ~Circuit() = default;

    // Prevent copying (circuit owns resources)
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;

    // Allow moving
    Circuit(Circuit&&) = default;
    Circuit& operator=(Circuit&&) = default;

    // --- Node management ---

    /**
     * @brief Find a node by name
     * @param name Node name
     * @return Pointer to node, or nullptr if not found
     */
    Node* findNode(const std::string& name);

    /**
     * @brief Get existing node or create new one
     * @param name Node name
     * @return Pointer to node (newly created or existing)
     */
    Node* getOrCreateNode(const std::string& name);

    /**
     * @brief Get or allocate equation number for node
     * @param node Pointer to node
     * @return Equation number (-1 for ground)
     */
    Index getEqNum(Node* node);

    // --- Device management ---

    /**
     * @brief Add a 2-terminal device
     * @param name Device name
     * @param type Device type
     * @param n1 First node equation number
     * @param n2 Second node equation number
     * @param value Device value
     * @return Pointer to created device
     */
    Device* addDevice(const std::string& name, DeviceType type,
                      Index n1, Index n2, Real value);

    /**
     * @brief Add a 4-terminal device
     * @param name Device name
     * @param type Device type
     * @param n1 First node
     * @param n2 Second node
     * @param n3 Third node
     * @param n4 Fourth node
     * @param value Device value
     * @return Pointer to created device
     */
    Device* addDevice4(const std::string& name, DeviceType type,
                       Index n1, Index n2, Index n3, Index n4, Real value);

    /**
     * @brief Add a 5-terminal device
     * @param name Device name
     * @param type Device type
     * @param n1 First node
     * @param n2 Second node
     * @param n3 Third node
     * @param n4 Fourth node
     * @param n5 Fifth node
     * @param value Device value
     * @return Pointer to created device
     */
    Device* addDevice5(const std::string& name, DeviceType type,
                       Index n1, Index n2, Index n3, Index n4, Index n5, Real value);

    // --- Model management ---

    /**
     * @brief Find a model by name
     * @param name Model name
     * @return Shared pointer to model, or empty if not found
     */
    std::shared_ptr<Model> findModel(const std::string& name);

    /**
     * @brief Add a new model or return existing one
     * @param name Model name
     * @param type Device type
     * @return Shared pointer to model
     */
    std::shared_ptr<Model> addModel(const std::string& name, DeviceType type);

    // --- Parameter management ---

    /**
     * @brief Find a parameter by name
     * @param name Parameter name
     * @return Pointer to parameter, or nullptr if not found
     */
    Param* findParam(const std::string& name);

    /**
     * @brief Set a parameter value
     * @param name Parameter name
     * @param value Parameter value
     * @return ErrorCode
     */
    ErrorCode setParam(const std::string& name, Real value);

    /**
     * @brief Evaluate a parameter expression
     * @param expr Expression string (e.g., "{Rval}" or "1k")
     * @return Evaluated value
     */
    Real evalParam(const std::string& expr);

    // --- Subcircuit management ---

    /**
     * @brief Find a subcircuit definition by name
     * @param name Subcircuit name
     * @return Pointer to definition, or nullptr if not found
     */
    SubcktDef* findSubckt(const std::string& name);

    /**
     * @brief Add a new subcircuit definition
     * @param name Subcircuit name
     * @return Pointer to created definition
     */
    SubcktDef* addSubckt(const std::string& name);

    // --- Circuit lifecycle ---

    /**
     * @brief Initialize circuit (allocate result arrays, apply IC)
     * @return ErrorCode
     */
    ErrorCode init();

    /**
     * @brief Allocate a voltage source branch current index
     * @return Branch index
     */
    int allocVsrcBranch();

    /**
     * @brief Resolve branch current indices to equation numbers
     * @return ErrorCode
     */
    ErrorCode resolveBranches();
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Parse a number with SPICE suffix (k, meg, m, u, n, p, f, etc.)
 * @param str String to parse
 * @return Parsed value
 */
Real parseNumber(const std::string& str);

/**
 * @brief Convert string to lowercase
 * @param str String to convert (modified in place)
 */
void toLower(std::string& str);

/**
 * @brief Convert string to lowercase (returns new string)
 * @param str Input string
 * @return Lowercase copy
 */
std::string toLowerCopy(const std::string& str);

/**
 * @brief Trim whitespace from both ends of string
 * @param str String to trim
 * @return Trimmed string
 */
std::string trim(const std::string& str);

} // namespace spice

#endif // CIRCUIT_HPP

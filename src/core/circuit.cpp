/**
 * @file circuit.cpp
 * @brief Circuit data structures implementation (C++17 version)
 *
 * Implements the Circuit, Node, Device, Model, and WaveformParams classes
 * defined in circuit.hpp.
 */
#include "circuit.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cmath>

namespace spice {

//=============================================================================
// Node Implementation
//=============================================================================

Node::Node(std::string name_, Index number_)
    : name(std::move(name_)),
      number(number_),
      eqnum(-1),
      is_ground(false),
      is_vsource(false),
      init_voltage(0.0),
      has_init(false)
{
    // Check if this is ground
    is_ground = (name == "0" || toLowerCopy(name) == "gnd");
}

//=============================================================================
// WaveformParams Implementation
//=============================================================================

WaveformParams::WaveformParams(WaveformType type_)
    : type(type_) {}

Real WaveformParams::evaluate(Real t) const {
    switch (type) {
    case WaveformType::SIN: {
        // SIN(Voffset Vamp Freq Td Theta Phi)
        if (t < sin_td)
            return sin_voffset;

        Real tau = t - sin_td;
        Real phase = sin_phi * M_PI / 180.0;
        Real omega = 2.0 * M_PI * sin_freq;
        Real damping = sin_theta;

        Real envelope = 1.0;
        if (damping > 0)
            envelope = std::exp(-damping * tau);

        return sin_voffset + envelope * sin_vamp * std::sin(omega * tau + phase);
    }

    case WaveformType::PULSE: {
        // PULSE(V1 V2 Td Tr Tf Pw Per)
        if (t < pulse_td)
            return pulse_v1;

        // Calculate position within period
        Real t_period = pulse_per > 0 ? std::fmod(t - pulse_td, pulse_per) : 0;

        // Rising edge
        if (t_period < pulse_tr)
            return pulse_v1 + (pulse_v2 - pulse_v1) * t_period / pulse_tr;

        // Flat top
        if (t_period < pulse_tr + pulse_pw)
            return pulse_v2;

        // Falling edge
        if (t_period < pulse_tr + pulse_pw + pulse_tf)
            return pulse_v2 - (pulse_v2 - pulse_v1) * (t_period - pulse_tr - pulse_pw) / pulse_tf;

        return pulse_v1;
    }

    case WaveformType::PWL: {
        // PWL(t1 v1 t2 v2 ...)
        if (pwl_time.size() < 2)
            return 0.0;

        // Before first point
        if (t <= pwl_time.front())
            return pwl_value.front();

        // After last point
        if (t >= pwl_time.back())
            return pwl_value.back();

        // Interpolate
        for (size_t i = 0; i < pwl_time.size() - 1; i++) {
            if (t >= pwl_time[i] && t <= pwl_time[i + 1]) {
                Real frac = (t - pwl_time[i]) / (pwl_time[i + 1] - pwl_time[i]);
                return pwl_value[i] + frac * (pwl_value[i + 1] - pwl_value[i]);
            }
        }

        return pwl_value.back();
    }

    case WaveformType::EXP: {
        // EXP(V1 V2 Td1 Tau1 Td2 Tau2)
        if (t < exp_td1)
            return exp_v1;

        if (t < exp_td2) {
            // Rising exponential
            Real tau = t - exp_td1;
            if (exp_tau1 > 0)
                return exp_v1 + (exp_v2 - exp_v1) * (1.0 - std::exp(-tau / exp_tau1));
            return exp_v2;
        }

        // Falling exponential
        Real tau = t - exp_td2;
        if (exp_tau2 > 0)
            return exp_v2 + (exp_v1 - exp_v2) * (1.0 - std::exp(-tau / exp_tau2));
        return exp_v1;
    }

    default:
        return 0.0;
    }
}

//=============================================================================
// Device Implementation
//=============================================================================

Device::Device(std::string name_, DeviceType type_, Index n1_, Index n2_, Real value_)
    : name(std::move(name_)),
      type(type_),
      n1(n1_),
      n2(n2_),
      n3(-1),
      n4(-1),
      n5(-1),
      value(value_),
      value2(0.0) {}

Real Device::getNodeVoltage(const Circuit& ckt, Index nodeEq) {
    if (nodeEq < 0)
        return 0.0;  // Ground
    if (nodeEq >= static_cast<Index>(ckt.voltage.size()))
        return 0.0;  // Out of bounds
    return ckt.voltage[nodeEq];
}

Real Device::getVoltageAcross(const Circuit& ckt) const {
    Real v1 = getNodeVoltage(ckt, n1);
    Real v2 = getNodeVoltage(ckt, n2);
    return v1 - v2;
}

//=============================================================================
// Model Implementation
//=============================================================================

Model::Model(std::string name_, DeviceType type_)
    : name(std::move(name_)), type(type_) {}

DiodeModelParams* Model::getDiodeParams() {
    if (type == DeviceType::DIODE) {
        if (std::holds_alternative<std::monostate>(params)) {
            params = DiodeModelParams{};
        }
        return &std::get<DiodeModelParams>(params);
    }
    return nullptr;
}

BjtModelParams* Model::getBjtParams() {
    if (type == DeviceType::NPN || type == DeviceType::PNP) {
        if (std::holds_alternative<std::monostate>(params)) {
            BjtModelParams p;
            p.polarity = (type == DeviceType::NPN) ? 1 : -1;
            params = p;
        }
        return &std::get<BjtModelParams>(params);
    }
    return nullptr;
}

MosModelParams* Model::getMosParams() {
    if (type == DeviceType::NMOS || type == DeviceType::PMOS) {
        if (std::holds_alternative<std::monostate>(params)) {
            MosModelParams p;
            p.polarity = (type == DeviceType::NMOS) ? 1 : -1;
            params = p;
        }
        return &std::get<MosModelParams>(params);
    }
    return nullptr;
}

SwitchModelParams* Model::getSwitchParams() {
    if (type == DeviceType::SWITCH_VOLTAGE || type == DeviceType::SWITCH_CURRENT) {
        if (std::holds_alternative<std::monostate>(params)) {
            params = SwitchModelParams{};
        }
        return &std::get<SwitchModelParams>(params);
    }
    return nullptr;
}

TransmissionLineParams* Model::getTlineParams() {
    if (type == DeviceType::TRANSMISSION_LINE) {
        if (std::holds_alternative<std::monostate>(params)) {
            params = TransmissionLineParams{};
        }
        return &std::get<TransmissionLineParams>(params);
    }
    return nullptr;
}

ErrorCode Model::setParam(const std::string& param, Real value) {
    std::string lower_param = toLowerCopy(param);

    switch (type) {
    case DeviceType::DIODE: {
        DiodeModelParams* p = getDiodeParams();
        if (!p) return ErrorCode::NOT_FOUND;
        if (lower_param == "is") p->is = value;
        else if (lower_param == "n") p->n = value;
        else if (lower_param == "rs") p->rs = value;
        else if (lower_param == "cjo") p->cjo = value;
        else if (lower_param == "vj") p->vj = value;
        else if (lower_param == "m") p->m = value;
        else if (lower_param == "tt") p->tt = value;
        else if (lower_param == "eg") p->eg = value;
        else if (lower_param == "xti") p->xti = value;
        else if (lower_param == "kf") p->kf = value;
        else if (lower_param == "af") p->af = value;
        else if (lower_param == "fc") p->fc = value;
        else return ErrorCode::NOT_FOUND;
        break;
    }
    case DeviceType::NPN:
    case DeviceType::PNP: {
        BjtModelParams* p = getBjtParams();
        if (!p) return ErrorCode::NOT_FOUND;
        if (lower_param == "is") p->is = value;
        else if (lower_param == "bf") p->bf = value;
        else if (lower_param == "nf") p->nf = value;
        else if (lower_param == "br") p->br = value;
        else if (lower_param == "nr") p->nr = value;
        else if (lower_param == "rb") p->rb = value;
        else if (lower_param == "re") p->re = value;
        else if (lower_param == "rc") p->rc = value;
        else if (lower_param == "tf") p->tf = value;
        else if (lower_param == "tr") p->tr = value;
        else return ErrorCode::NOT_FOUND;
        break;
    }
    case DeviceType::NMOS:
    case DeviceType::PMOS: {
        MosModelParams* p = getMosParams();
        if (!p) return ErrorCode::NOT_FOUND;
        if (lower_param == "kp") p->kp = value;
        else if (lower_param == "vto") p->vto = value;
        else if (lower_param == "gamma") p->gamma = value;
        else if (lower_param == "lambda") p->lambda = value;
        else if (lower_param == "phi") p->phi = value;
        else if (lower_param == "w") p->w = value;
        else if (lower_param == "l") p->l = value;
        else return ErrorCode::NOT_FOUND;
        break;
    }
    case DeviceType::SWITCH_VOLTAGE:
    case DeviceType::SWITCH_CURRENT: {
        SwitchModelParams* p = getSwitchParams();
        if (!p) return ErrorCode::NOT_FOUND;
        if (lower_param == "ron") p->ron = value;
        else if (lower_param == "roff") p->roff = value;
        else if (lower_param == "vt") p->vt = value;
        else if (lower_param == "vh") p->vh = value;
        else return ErrorCode::NOT_FOUND;
        break;
    }
    case DeviceType::TRANSMISSION_LINE: {
        TransmissionLineParams* p = getTlineParams();
        if (!p) return ErrorCode::NOT_FOUND;
        if (lower_param == "td") p->td = value;
        else if (lower_param == "z0") p->z0 = value;
        else return ErrorCode::NOT_FOUND;
        break;
    }
    default:
        return ErrorCode::NOT_FOUND;
    }

    return ErrorCode::OK;
}

//=============================================================================
// SubcktDef Implementation
//=============================================================================

SubcktDef::SubcktDef(std::string name_)
    : name(std::move(name_)) {}

//=============================================================================
// AnalysisParams Implementation
//=============================================================================

AnalysisParams::AnalysisParams(AnalysisType type_)
    : type(type_) {}

//=============================================================================
// Analysis Implementation
//=============================================================================

Analysis::Analysis(AnalysisType type_)
    : params(type_) {}

//=============================================================================
// Circuit Implementation
//=============================================================================

Circuit::Circuit(std::string title_)
    : title(std::move(title_))
{
    // Create ground node (node 0)
    auto ground = std::make_unique<Node>("0", num_nodes++);
    ground->is_ground = true;
    nodes.push_back(std::move(ground));
}

Node* Circuit::findNode(const std::string& name) {
    for (auto& node : nodes) {
        if (node->name == name)
            return node.get();
    }
    return nullptr;
}

Node* Circuit::getOrCreateNode(const std::string& name) {
    // First, try to find existing node
    Node* existing = findNode(name);
    if (existing)
        return existing;

    // Create new node
    auto node = std::make_unique<Node>(name, num_nodes++);
    Node* ptr = node.get();
    nodes.push_back(std::move(node));
    return ptr;
}

Index Circuit::getEqNum(Node* node) {
    if (!node || node->is_ground)
        return -1;  // Ground has no equation

    if (node->eqnum >= 0)
        return node->eqnum;

    // Allocate new equation number
    node->eqnum = num_eqns++;
    return node->eqnum;
}

Device* Circuit::addDevice(const std::string& name, DeviceType type,
                           Index n1, Index n2, Real value)
{
    auto dev = std::make_unique<Device>(name, type, n1, n2, value);
    Device* ptr = dev.get();
    devices.push_back(std::move(dev));
    return ptr;
}

Device* Circuit::addDevice4(const std::string& name, DeviceType type,
                            Index n1, Index n2, Index n3, Index n4, Real value)
{
    Device* dev = addDevice(name, type, n1, n2, value);
    if (dev) {
        dev->n3 = n3;
        dev->n4 = n4;
    }
    return dev;
}

Device* Circuit::addDevice5(const std::string& name, DeviceType type,
                            Index n1, Index n2, Index n3, Index n4, Index n5, Real value)
{
    Device* dev = addDevice4(name, type, n1, n2, n3, n4, value);
    if (dev) {
        dev->n5 = n5;
    }
    return dev;
}

std::shared_ptr<Model> Circuit::findModel(const std::string& name) {
    for (auto& model : models) {
        if (model->name == name)
            return model;
    }
    return nullptr;
}

std::shared_ptr<Model> Circuit::addModel(const std::string& name, DeviceType type) {
    // Check if model already exists
    auto existing = findModel(name);
    if (existing)
        return existing;

    // Create new model
    auto model = std::make_shared<Model>(name, type);
    models.push_back(model);
    return model;
}

Param* Circuit::findParam(const std::string& name) {
    for (auto& p : params) {
        if (p.name == name)
            return &p;
    }
    return nullptr;
}

ErrorCode Circuit::setParam(const std::string& name, Real value) {
    Param* p = findParam(name);
    if (p == nullptr) {
        params.push_back(Param{name, value});
    } else {
        p->value = value;
    }
    return ErrorCode::OK;
}

Real Circuit::evalParam(const std::string& expr) {
    // For simple parameter references like {param_name}
    if (expr.size() >= 2 && expr.front() == '{' && expr.back() == '}') {
        std::string name = expr.substr(1, expr.size() - 2);
        Param* p = findParam(name);
        if (p)
            return p->value;
    }

    // Try to parse as number with suffix
    try {
        return parseNumber(expr);
    } catch (...) {
        return 0.0;
    }
}

SubcktDef* Circuit::findSubckt(const std::string& name) {
    for (auto& s : subckts) {
        if (toLowerCopy(s->name) == toLowerCopy(name))
            return s.get();
    }
    return nullptr;
}

SubcktDef* Circuit::addSubckt(const std::string& name) {
    SubcktDef* existing = findSubckt(name);
    if (existing)
        return existing;

    auto s = std::make_unique<SubcktDef>(name);
    SubcktDef* ptr = s.get();
    subckts.push_back(std::move(s));
    return ptr;
}

ErrorCode Circuit::init() {
    // Allocate voltage and current arrays
    Index total_eqns = num_eqns + num_vsources;
    voltage.resize(total_eqns + 1, 0.0);
    current.resize(total_eqns + 1, 0.0);

    // Apply initial conditions from nodes
    for (auto& node : nodes) {
        if (node->has_init && node->eqnum >= 0 &&
            node->eqnum < static_cast<Index>(voltage.size())) {
            voltage[node->eqnum] = node->init_voltage;
        }
    }

    return ErrorCode::OK;
}

int Circuit::allocVsrcBranch() {
    num_vsources++;
    return next_vsrc_branch++;
}

ErrorCode Circuit::resolveBranches() {
    // Branch current equation numbers start after node equations
    Index branch_offset = num_eqns;

    for (auto& dev : devices) {
        if (dev->type == DeviceType::VSRC ||
            dev->type == DeviceType::VCVS ||
            dev->type == DeviceType::CCVS) {
            // Convert branch index to equation number
            if (dev->n3 >= 0 && dev->n3 < next_vsrc_branch) {
                dev->n3 = branch_offset + dev->n3;
            }
            if (dev->n5 >= 0 && dev->n5 < next_vsrc_branch) {
                dev->n5 = branch_offset + dev->n5;
            }
        }
    }

    return ErrorCode::OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

Real parseNumber(const std::string& str) {
    try {
        char* end;
        Real val = std::strtod(str.c_str(), &end);

        // Handle suffixes
        std::string suffix(end);
        toLower(suffix);

        if (suffix == "meg" || suffix == "mege") val *= 1e6;
        else if (suffix == "k") val *= 1e3;
        else if (suffix == "m") val *= 1e-3;
        else if (suffix == "u") val *= 1e-6;
        else if (suffix == "n") val *= 1e-9;
        else if (suffix == "p") val *= 1e-12;
        else if (suffix == "f") val *= 1e-15;
        else if (suffix == "t") val *= 1e12;
        else if (suffix == "g") val *= 1e9;

        return val;
    } catch (...) {
        return 0.0;
    }
}

void toLower(std::string& str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
}

std::string toLowerCopy(const std::string& str) {
    std::string result = str;
    toLower(result);
    return result;
}

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace spice

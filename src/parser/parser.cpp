/**
 * @file parser.cpp
 * @brief SPICE netlist parser implementation (C++17 version)
 */
#include "parser.hpp"
#include "device.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace spice {

//=============================================================================
// Parser Implementation
//=============================================================================

std::unique_ptr<Circuit> Parser::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        setError("Cannot open file: " + filename);
        return nullptr;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return parseString(ss.str());
}

std::unique_ptr<Circuit> Parser::parseString(const std::string& input) {
    std::vector<std::string> lines;
    std::istringstream stream(input);
    std::string line;
    while (std::getline(stream, line))
        lines.push_back(line);

    std::vector<std::string> joined = processContinuations(lines);

    circuit = nullptr;
    bool title_parsed = false;

    for (const auto& raw_line : joined) {
        std::string trimmed = trim(raw_line);
        if (trimmed.empty() || trimmed[0] == '*')
            continue;

        if (!title_parsed) {
            circuit = std::make_unique<Circuit>(trimmed);
            title_parsed = true;
            continue;
        }

        if (trimmed[0] == '.') {
            if (parseDotCommand(trimmed) != ErrorCode::OK)
                return nullptr;
        } else {
            if (parseDeviceLine(trimmed) != ErrorCode::OK)
                return nullptr;
        }

        if (toLowerCopy(trimmed).substr(0, 4) == ".end")
            break;
    }

    if (!circuit) {
        setError("Empty netlist or no title");
        return nullptr;
    }

    // Resolve branch equation numbers
    circuit->resolveBranches();

    return std::move(circuit);
}

std::vector<std::string> Parser::processContinuations(const std::vector<std::string>& lines) {
    std::vector<std::string> result;
    for (const auto& l : lines) {
        std::string t = trim(l);
        if (t.empty() || t[0] == '*') continue;
        if (t[0] == '+' && !result.empty()) {
            result.back() += " " + t.substr(1);
        } else {
            result.push_back(t);
        }
    }
    return result;
}

std::vector<std::string> Parser::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : line) {
        if (c == '(' || c == ')') {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
            tokens.push_back(std::string(1, c));
        } else if (std::isspace(c)) {
            if (!current.empty()) { tokens.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

Index Parser::getOrCreateNodeEq(const std::string& name) {
    Node* n = circuit->getOrCreateNode(name);
    return circuit->getEqNum(n);
}

ErrorCode Parser::parseDeviceLine(const std::string& line) {
    if (line.empty()) return ErrorCode::OK;
    char prefix = std::toupper(line[0]);
    auto tokens = tokenize(line);
    if (tokens.empty()) return ErrorCode::OK;

    switch (prefix) {
        case 'R': return parseResistor(tokens);
        case 'C': return parseCapacitor(tokens);
        case 'L': return parseInductor(tokens);
        case 'V': return parseVoltageSource(tokens);
        case 'I': return parseCurrentSource(tokens);
        case 'G': return parseVCCS(tokens);
        case 'E': return parseVCVS(tokens);
        case 'F': return parseCCCS(tokens);
        case 'H': return parseCCVS(tokens);
        case 'D': return parseDiode(tokens);
        case 'Q': return parseBJT(tokens);
        case 'M': return parseMOSFET(tokens);
        case 'B': return parseBehavioralSrc(tokens);
        case 'S': case 'W': return parseSwitch(tokens);
        case 'T': return parseTransmissionLine(tokens);
        case 'X': return parseSubcktInstance(tokens);
        default: return ErrorCode::OK;
    }
}

ErrorCode Parser::parseDotCommand(const std::string& line) {
    auto tokens = tokenize(line);
    if (tokens.empty()) return ErrorCode::OK;
    std::string cmd = toLowerCopy(tokens[0]);

    if (cmd == ".op") return parseDCOp();
    if (cmd == ".dc") return parseDCSweep(tokens);
    if (cmd == ".ac") return parseAC(tokens);
    if (cmd == ".tran") return parseTransient(tokens);
    if (cmd == ".model") return parseModel(tokens);
    if (cmd == ".temp") return parseTemp(tokens);
    if (cmd == ".param") return parseParam(tokens);
    if (cmd == ".step") return parseStep(tokens);
    if (cmd == ".options") return parseOptions(tokens);
    if (cmd == ".print") return parsePrint(tokens);
    if (cmd == ".ic") return parseIC(tokens);
    if (cmd == ".noise") return parseNoise(tokens);
    if (cmd == ".four") return parseFourier(tokens);
    if (cmd == ".sens") return parseSensitivity(tokens);
    if (cmd == ".subckt") return parseSubckt(tokens);
    if (cmd == ".ends") return parseEnds();
    if (cmd == ".end") return parseEnd();
    return ErrorCode::OK;
}

// --- Devices ---

ErrorCode Parser::parseResistor(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("R: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    circuit->addDevice(t[0], DeviceType::RESISTOR, n1, n2, parseNumber(t[3]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseCapacitor(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("C: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    circuit->addDevice(t[0], DeviceType::CAPACITOR, n1, n2, parseNumber(t[3]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseInductor(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("L: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    circuit->addDevice(t[0], DeviceType::INDUCTOR, n1, n2, parseNumber(t[3]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseVoltageSource(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("V: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    int ib = circuit->allocVsrcBranch();
    Real v = 0.0;
    int val_idx = 3;
    if (t.size() >= 5 && toLowerCopy(t[3]) == "dc") {
        v = parseNumber(t[4]); val_idx = 5;
    } else {
        v = parseNumber(t[3]); val_idx = 4;
    }
    auto dev = circuit->addDevice4(t[0], DeviceType::VSRC, n1, n2, ib, -1, v);
    if (val_idx < t.size()) {
        dev->waveform = parseWaveform(t, val_idx);
    }
    return ErrorCode::OK;
}

ErrorCode Parser::parseCurrentSource(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("I: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    Real i = parseNumber(t[3]);
    auto dev = circuit->addDevice(t[0], DeviceType::ISRC, n1, n2, i);
    if (t.size() > 4 && toLowerCopy(t[4]) == "ac") {
        dev->waveform = std::make_unique<WaveformParams>(WaveformType::AC);
        dev->waveform->ac_mag = parseNumber(t[5]);
    }
    return ErrorCode::OK;
}

ErrorCode Parser::parseVCCS(const std::vector<std::string>& t) {
    if (t.size() < 6) { setError("G: needs 6 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    Index n3 = getOrCreateNodeEq(t[3]), n4 = getOrCreateNodeEq(t[4]);
    circuit->addDevice4(t[0], DeviceType::VCCS, n1, n2, n3, n4, parseNumber(t[5]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseVCVS(const std::vector<std::string>& t) {
    if (t.size() < 6) { setError("E: needs 6 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    Index n3 = getOrCreateNodeEq(t[3]), n4 = getOrCreateNodeEq(t[4]);
    int ib = circuit->allocVsrcBranch();
    circuit->addDevice5(t[0], DeviceType::VCVS, n1, n2, n3, n4, ib, parseNumber(t[5]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseCCCS(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError("F: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    Device* ctrl = nullptr;
    for (auto& d : circuit->devices) if (d->name == t[3]) ctrl = d.get();
    if (!ctrl) { setError("F: ctrl source not found"); return ErrorCode::NOT_FOUND; }
    circuit->addDevice4(t[0], DeviceType::CCCS, n1, n2, ctrl->n3, -1, parseNumber(t[4]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseCCVS(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError("H: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    Device* ctrl = nullptr;
    for (auto& d : circuit->devices) if (d->name == t[3]) ctrl = d.get();
    if (!ctrl) { setError("H: ctrl source not found"); return ErrorCode::NOT_FOUND; }
    int ib = circuit->allocVsrcBranch();
    circuit->addDevice5(t[0], DeviceType::CCVS, n1, n2, ib, ctrl->n3, -1, parseNumber(t[4]));
    return ErrorCode::OK;
}

ErrorCode Parser::parseDiode(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("D: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    auto model = circuit->addModel(t[3], DeviceType::DIODE);
    if (!model->getDiodeParams()) model->params = DiodeModelParams{};
    auto dev = circuit->addDevice(t[0], DeviceType::DIODE, n1, n2, 0.0);
    dev->model = model;
    return ErrorCode::OK;
}

ErrorCode Parser::parseBJT(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError("Q: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index c = getOrCreateNodeEq(t[1]), b = getOrCreateNodeEq(t[2]), e = getOrCreateNodeEq(t[3]);
    Index sub = -1;
    if (t.size() > 5) sub = getOrCreateNodeEq(t[5]);
    auto model = circuit->findModel(t[4]);
    DeviceType type = DeviceType::NPN;
    if (model) type = model->type;
    else {
        // Guess from .MODEL later, default NPN
        model = circuit->addModel(t[4], DeviceType::NPN);
    }
    auto dev = circuit->addDevice5(t[0], type, c, b, e, sub, -1, 0.0);
    dev->model = model;
    return ErrorCode::OK;
}

ErrorCode Parser::parseMOSFET(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError("M: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index d = getOrCreateNodeEq(t[1]), g = getOrCreateNodeEq(t[2]), s = getOrCreateNodeEq(t[3]);
    Index body = -1;
    if (t.size() > 5) body = getOrCreateNodeEq(t[5]);
    auto model = circuit->findModel(t[4]);
    DeviceType type = DeviceType::NMOS;
    if (model) type = model->type;
    else model = circuit->addModel(t[4], DeviceType::NMOS);
    Real w = 0.0, l = 0.0;
    for (size_t i = 5; i < t.size(); i++) {
        if (t[i].substr(0, 2) == "W=") w = parseNumber(t[i].substr(2));
        if (t[i].substr(0, 2) == "L=") l = parseNumber(t[i].substr(2));
    }
    auto dev = circuit->addDevice5(t[0], type, d, g, s, body, -1, w);
    dev->model = model;
    dev->value2 = l;
    return ErrorCode::OK;
}

ErrorCode Parser::parseBehavioralSrc(const std::vector<std::string>& t) {
    if (t.size() < 4) { setError("B: needs 4 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1 = getOrCreateNodeEq(t[1]), n2 = getOrCreateNodeEq(t[2]);
    bool is_v = false;
    std::string expr;
    for (size_t i = 3; i < t.size(); i++) {
        if (t[i].substr(0, 2) == "V=") { is_v = true; expr = t[i].substr(2); }
        else if (t[i].substr(0, 2) == "I=") { expr = t[i].substr(2); }
    }
    auto dev = circuit->addDevice(t[0], is_v ? DeviceType::BEHAVIORAL_VSRC : DeviceType::BEHAVIORAL_SRC, n1, n2, 0.0);
    dev->expr = expr;
    return ErrorCode::OK;
}

ErrorCode Parser::parseSwitch(const std::vector<std::string>& t) {
    if (t.size() < 6) { setError("S/W: needs 6 tokens"); return ErrorCode::SYNTAX_ERROR; }
    bool is_v = (t[0][0] == 'S' || t[0][0] == 's');
    Index nc1 = getOrCreateNodeEq(t[1]), nc2 = getOrCreateNodeEq(t[2]);
    Index n1 = getOrCreateNodeEq(t[3]), n2 = getOrCreateNodeEq(t[4]);
    auto model = circuit->addModel(t[5], is_v ? DeviceType::SWITCH_VOLTAGE : DeviceType::SWITCH_CURRENT);
    if (!model->getSwitchParams()) model->params = SwitchModelParams{};
    auto dev = circuit->addDevice4(t[0], is_v ? DeviceType::SWITCH_VOLTAGE : DeviceType::SWITCH_CURRENT, nc1, nc2, n1, n2, 0.0);
    dev->model = model;
    return ErrorCode::OK;
}

ErrorCode Parser::parseTransmissionLine(const std::vector<std::string>& t) {
    if (t.size() < 6) { setError("T: needs 6 tokens"); return ErrorCode::SYNTAX_ERROR; }
    Index n1p = getOrCreateNodeEq(t[1]), n1m = getOrCreateNodeEq(t[2]);
    Index n2p = getOrCreateNodeEq(t[3]), n2m = getOrCreateNodeEq(t[4]);
    auto model = circuit->addModel(t[5], DeviceType::TRANSMISSION_LINE);
    if (!model->getTlineParams()) model->params = TransmissionLineParams{};
    auto dev = circuit->addDevice4(t[0], DeviceType::TRANSMISSION_LINE, n1p, n1m, n2p, n2m, 0.0);
    dev->model = model;
    return ErrorCode::OK;
}

ErrorCode Parser::parseSubcktInstance(const std::vector<std::string>& t) {
    if (t.size() < 3) { setError("X: needs 3 tokens"); return ErrorCode::SYNTAX_ERROR; }
    auto dev = circuit->addDevice(t[0], DeviceType::SUBCKT, -1, -1, 0.0);
    dev->subckt_def = circuit->findSubckt(t.back());
    if (!dev->subckt_def) { setError("X: subckt not found"); return ErrorCode::NOT_FOUND; }
    for (size_t i = 1; i < t.size() - 1; i++) {
        Node* n = circuit->getOrCreateNode(t[i]);
        dev->port_map.push_back(n->is_ground ? -1 : circuit->getEqNum(n));
    }
    return ErrorCode::OK;
}

// --- Dot Commands ---

ErrorCode Parser::parseModel(const std::vector<std::string>& t) {
    if (t.size() < 3) { setError(".MODEL: needs 3 tokens"); return ErrorCode::SYNTAX_ERROR; }
    std::string type_str = toLowerCopy(t[2]);
    DeviceType type;
    if (type_str == "d" || type_str == "diode") type = DeviceType::DIODE;
    else if (type_str == "npn") type = DeviceType::NPN;
    else if (type_str == "pnp") type = DeviceType::PNP;
    else if (type_str == "nmos") type = DeviceType::NMOS;
    else if (type_str == "pmos") type = DeviceType::PMOS;
    else if (type_str == "sw" || type_str == "switch") type = DeviceType::SWITCH_VOLTAGE;
    else if (type_str == "csw") type = DeviceType::SWITCH_CURRENT;
    else if (type_str == "tline") type = DeviceType::TRANSMISSION_LINE;
    else return ErrorCode::OK; // skip unknown

    auto model = circuit->addModel(t[1], type);
    // Parse params (param=value)
    for (size_t i = 3; i < t.size(); i++) {
        auto eq = t[i].find('=');
        if (eq != std::string::npos) {
            std::string pname = toLowerCopy(t[i].substr(0, eq));
            Real pval = parseNumber(t[i].substr(eq + 1));
            model->setParam(pname, pval);
        }
    }
    return ErrorCode::OK;
}

ErrorCode Parser::parseDCOp() {
    circuit->analyses.push_back(std::make_unique<Analysis>(AnalysisType::DC_OP));
    return ErrorCode::OK;
}

ErrorCode Parser::parseDCSweep(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError(".DC: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    auto ana = std::make_unique<Analysis>(AnalysisType::DC_SWEEP);
    ana->params.src_name = t[1];
    ana->params.start = parseNumber(t[2]);
    ana->params.stop = parseNumber(t[3]);
    ana->params.step = parseNumber(t[4]);
    circuit->analyses.push_back(std::move(ana));
    return ErrorCode::OK;
}

ErrorCode Parser::parseAC(const std::vector<std::string>& t) {
    if (t.size() < 5) { setError(".AC: needs 5 tokens"); return ErrorCode::SYNTAX_ERROR; }
    auto ana = std::make_unique<Analysis>(AnalysisType::AC);
    std::string type = toLowerCopy(t[1]);
    if (type == "dec") ana->params.ac_sweep_type = SweepType::DECADE;
    else if (type == "oct") ana->params.ac_sweep_type = SweepType::OCTAVE;
    else ana->params.ac_sweep_type = SweepType::LINEAR;
    ana->params.ac_points = parseNumber(t[2]);
    ana->params.ac_start = parseNumber(t[3]);
    ana->params.ac_stop = parseNumber(t[4]);
    circuit->analyses.push_back(std::move(ana));
    return ErrorCode::OK;
}

ErrorCode Parser::parseTransient(const std::vector<std::string>& t) {
    if (t.size() < 3) { setError(".TRAN: needs 3 tokens"); return ErrorCode::SYNTAX_ERROR; }
    auto ana = std::make_unique<Analysis>(AnalysisType::TRANSIENT);
    ana->params.tstep = parseNumber(t[1]);
    ana->params.tstop = parseNumber(t[2]);
    if (t.size() > 3 && toLowerCopy(t[3]) == "uic") ana->params.use_uic = true;
    circuit->analyses.push_back(std::move(ana));
    return ErrorCode::OK;
}

ErrorCode Parser::parseNoise(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseFourier(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseSensitivity(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseTemp(const std::vector<std::string>& t) {
    if (t.size() < 2) return ErrorCode::OK;
    circuit->temp_celsius = parseNumber(t[1]);
    circuit->temp = circuit->temp_celsius + 273.15;
    return ErrorCode::OK;
}
ErrorCode Parser::parseParam(const std::vector<std::string>& t) {
    for (size_t i = 1; i < t.size(); i++) {
        auto eq = t[i].find('=');
        if (eq != std::string::npos)
            circuit->setParam(t[i].substr(0, eq), parseNumber(t[i].substr(eq+1)));
    }
    return ErrorCode::OK;
}
ErrorCode Parser::parseStep(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseOptions(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parsePrint(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseIC(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseSubckt(const std::vector<std::string>& t) { return ErrorCode::OK; }
ErrorCode Parser::parseEnds() { return ErrorCode::OK; }
ErrorCode Parser::parseEnd() { return ErrorCode::OK; }

// --- Waveform ---

std::unique_ptr<WaveformParams> Parser::parseWaveform(const std::vector<std::string>& t, size_t si) {
    if (si >= t.size()) return nullptr;
    std::string kw = toLowerCopy(t[si]);
    if (kw == "sin" && si + 6 < t.size()) {
        auto w = std::make_unique<WaveformParams>(WaveformType::SIN);
        w->sin_voffset = parseNumber(t[si+1]); w->sin_vamp = parseNumber(t[si+2]);
        w->sin_freq = parseNumber(t[si+3]); w->sin_td = parseNumber(t[si+4]);
        w->sin_theta = parseNumber(t[si+5]); w->sin_phi = parseNumber(t[si+6]);
        return w;
    }
    if (kw == "pulse" && si + 7 < t.size()) {
        auto w = std::make_unique<WaveformParams>(WaveformType::PULSE);
        w->pulse_v1 = parseNumber(t[si+1]); w->pulse_v2 = parseNumber(t[si+2]);
        w->pulse_td = parseNumber(t[si+3]); w->pulse_tr = parseNumber(t[si+4]);
        w->pulse_tf = parseNumber(t[si+5]); w->pulse_pw = parseNumber(t[si+6]);
        w->pulse_per = parseNumber(t[si+7]);
        return w;
    }
    if (kw == "pwl") {
        auto w = std::make_unique<WaveformParams>(WaveformType::PWL);
        for (size_t j = si + 1; j + 1 < t.size() && w->pwl_time.size() < MAX_PWL_POINTS; j += 2) {
            w->pwl_time.push_back(parseNumber(t[j]));
            w->pwl_value.push_back(parseNumber(t[j+1]));
        }
        return w;
    }
    if (kw == "exp" && si + 6 < t.size()) {
        auto w = std::make_unique<WaveformParams>(WaveformType::EXP);
        w->exp_v1 = parseNumber(t[si+1]); w->exp_v2 = parseNumber(t[si+2]);
        w->exp_td1 = parseNumber(t[si+3]); w->exp_tau1 = parseNumber(t[si+4]);
        w->exp_td2 = parseNumber(t[si+5]); w->exp_tau2 = parseNumber(t[si+6]);
        return w;
    }
    if (kw == "ac" && si + 1 < t.size()) {
        auto w = std::make_unique<WaveformParams>(WaveformType::AC);
        w->ac_mag = parseNumber(t[si+1]);
        if (si + 2 < t.size()) w->ac_phase = parseNumber(t[si+2]);
        return w;
    }
    return nullptr;
}

//=============================================================================
// Convenience
//=============================================================================
static std::string last_parser_error;
std::unique_ptr<Circuit> parseFile(const std::string& f) {
    Parser p; auto r = p.parseFile(f); last_parser_error = p.getError(); return r;
}
std::unique_ptr<Circuit> parseString(const std::string& i) {
    Parser p; auto r = p.parseString(i); last_parser_error = p.getError(); return r;
}
const std::string& getParserError() { return last_parser_error; }

} // namespace spice

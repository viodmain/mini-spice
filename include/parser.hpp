/**
 * @file parser.hpp
 * @brief Netlist parser interface (C++17 version)
 *
 * This header defines the Parser class for reading SPICE netlists and
 * creating Circuit objects.
 *
 * Features:
 * - SPICE-compatible netlist format
 * - Device prefixes: R, C, L, V, I, G, E, F, H, D, Q, M, B, S, W, T, X
 * - Dot-commands: .OP, .DC, .AC, .TRAN, .MODEL, .TEMP, .PARAM, .STEP,
 *   .OPTIONS, .PRINT, .IC, .NOISE, .FOUR, .SENS, .SUBCKT/.ENDS
 * - Value suffixes: T, G, MEG, K, m, u, n, p, f
 * - Waveform keywords: SIN, PULSE, PWL, EXP
 * - Continuation lines (+)
 * - Parameter substitution ({param})
 */
#ifndef PARSER_HPP
#define PARSER_HPP

#include "spice_types.hpp"
#include "circuit.hpp"
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <vector>

namespace spice {

//=============================================================================
// Parser Class
//=============================================================================

/**
 * @brief SPICE netlist parser
 *
 * Reads a SPICE netlist file or string and creates a fully-populated
 * Circuit object with all devices, models, analyses, and parameters.
 *
 * Usage:
 * @code
 * Parser parser;
 * std::unique_ptr<Circuit> ckt = parser.parseFile("circuit.net");
 * if (!ckt) {
 *     std::cerr << parser.getError() << std::endl;
 * }
 * @endcode
 */
class Parser {
public:
    /**
     * @brief Construct a new Parser
     */
    Parser() = default;

    /**
     * @brief Parse a netlist file
     * @param filename Path to netlist file
     * @return Unique pointer to Circuit, or nullptr on error
     */
    std::unique_ptr<Circuit> parseFile(const std::string& filename);

    /**
     * @brief Parse a netlist from string
     * @param input Netlist content
     * @return Unique pointer to Circuit, or nullptr on error
     */
    std::unique_ptr<Circuit> parseString(const std::string& input);

    /**
     * @brief Get last error message
     * @return Error string (empty if no error)
     */
    const std::string& getError() const { return last_error; }

private:
    std::string last_error;                         /**< Last error message */
    std::unique_ptr<Circuit> circuit;               /**< Circuit being built */
    std::vector<std::string> continuation_buffer;   /**< Continuation line buffer */

    /**
     * @brief Set error message
     * @param msg Error message
     */
    void setError(const std::string& msg) {
        last_error = msg;
    }

    /**
     * @brief Process continuation lines
     *
     * Joins lines starting with '+' to the previous line.
     *
     * @param lines Input lines
     * @return Joined lines
     */
    std::vector<std::string> processContinuations(const std::vector<std::string>& lines);

    /**
     * @brief Tokenize a line into whitespace-separated tokens
     * @param line Input line
     * @return Vector of tokens
     */
    std::vector<std::string> tokenize(const std::string& line);

    /**
     * @brief Parse the title line (first line of netlist)
     * @param line First line
     * @return ErrorCode
     */
    ErrorCode parseTitle(const std::string& line);

    /**
     * @brief Parse a device line
     * @param line Device definition line
     * @return ErrorCode
     */
    ErrorCode parseDeviceLine(const std::string& line);

    /**
     * @brief Parse a dot-command
     * @param line Command line (starting with '.')
     * @return ErrorCode
     */
    ErrorCode parseDotCommand(const std::string& line);

    // --- Device parsing helpers ---

    ErrorCode parseResistor(const std::vector<std::string>& tokens);
    ErrorCode parseCapacitor(const std::vector<std::string>& tokens);
    ErrorCode parseInductor(const std::vector<std::string>& tokens);
    ErrorCode parseVoltageSource(const std::vector<std::string>& tokens);
    ErrorCode parseCurrentSource(const std::vector<std::string>& tokens);
    ErrorCode parseVCCS(const std::vector<std::string>& tokens);
    ErrorCode parseVCVS(const std::vector<std::string>& tokens);
    ErrorCode parseCCCS(const std::vector<std::string>& tokens);
    ErrorCode parseCCVS(const std::vector<std::string>& tokens);
    ErrorCode parseDiode(const std::vector<std::string>& tokens);
    ErrorCode parseBJT(const std::vector<std::string>& tokens);
    ErrorCode parseMOSFET(const std::vector<std::string>& tokens);
    ErrorCode parseBehavioralSrc(const std::vector<std::string>& tokens);
    ErrorCode parseSwitch(const std::vector<std::string>& tokens);
    ErrorCode parseTransmissionLine(const std::vector<std::string>& tokens);
    ErrorCode parseSubcktInstance(const std::vector<std::string>& tokens);

    // --- Dot-command parsing helpers ---

    ErrorCode parseModel(const std::vector<std::string>& tokens);
    ErrorCode parseDCOp();
    ErrorCode parseDCSweep(const std::vector<std::string>& tokens);
    ErrorCode parseAC(const std::vector<std::string>& tokens);
    ErrorCode parseTransient(const std::vector<std::string>& tokens);
    ErrorCode parseNoise(const std::vector<std::string>& tokens);
    ErrorCode parseFourier(const std::vector<std::string>& tokens);
    ErrorCode parseSensitivity(const std::vector<std::string>& tokens);
    ErrorCode parseTemp(const std::vector<std::string>& tokens);
    ErrorCode parseParam(const std::vector<std::string>& tokens);
    ErrorCode parseStep(const std::vector<std::string>& tokens);
    ErrorCode parseOptions(const std::vector<std::string>& tokens);
    ErrorCode parsePrint(const std::vector<std::string>& tokens);
    ErrorCode parseIC(const std::vector<std::string>& tokens);
    ErrorCode parseSubckt(const std::vector<std::string>& tokens);
    ErrorCode parseEnds();
    ErrorCode parseEnd();

    /**
     * @brief Get or create node and return its equation number
     * @param name Node name
     * @return Equation number (-1 for ground)
     */
    Index getOrCreateNodeEq(const std::string& name);

    /**
     * @brief Parse waveform from tokens
     * @param tokens Token list
     * @param start_idx Starting index for waveform keyword
     * @return Unique pointer to waveform, or nullptr
     */
    std::unique_ptr<WaveformParams> parseWaveform(
        const std::vector<std::string>& tokens, size_t start_idx);
};

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Parse a netlist file (convenience function)
 * @param filename Path to netlist file
 * @return Unique pointer to Circuit, or nullptr on error
 */
std::unique_ptr<Circuit> parseFile(const std::string& filename);

/**
 * @brief Parse a netlist from string (convenience function)
 * @param input Netlist content
 * @return Unique pointer to Circuit, or nullptr on error
 */
std::unique_ptr<Circuit> parseString(const std::string& input);

/**
 * @brief Get last parser error (convenience function)
 * @return Error string
 */
const std::string& getParserError();

} // namespace spice

#endif // PARSER_HPP

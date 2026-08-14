#include "include/utils.h"
#include "include/sha.h"
#include <chrono>
#include <algorithm>
#include <charconv>
#include <cmath>

namespace utils {

std::string sha256(const std::string& input) {
    return SHA256::hash(input);
}

std::string calculateMerkleRoot(const std::vector<std::string>& transactions) {
    if (transactions.empty()) {
        return sha256(""); // Empty merkle root
    }
    
    if (transactions.size() == 1) {
        return transactions[0];
    }
    
    std::vector<std::string> currentLevel = transactions;
    
    while (currentLevel.size() > 1) {
        std::vector<std::string> nextLevel;
        
        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            std::string combined;
            if (i + 1 < currentLevel.size()) {
                combined = currentLevel[i] + currentLevel[i + 1];
            } else {
                // If odd number of elements, duplicate the last one
                combined = currentLevel[i] + currentLevel[i];
            }
            nextLevel.push_back(sha256(combined));
        }
        
        currentLevel = nextLevel;
    }
    
    return currentLevel[0];
}

long long getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

std::string bytesToHex(const std::vector<unsigned char>& data) {
    std::stringstream ss;
    for (unsigned char byte : data) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

bool checkProofOfWork(const std::string& hash, int difficulty) {
    // A difficulty from a chain file or a peer is untrusted: a negative value
    // used to convert to a huge size_t in std::string(count, char) and throw,
    // and a huge positive one allocated gigabytes on every validation attempt.
    if (difficulty < 0) return false;

    const size_t required = static_cast<size_t>(difficulty);
    if (required > hash.size()) return false; // unsatisfiable, not an error

    return std::all_of(hash.begin(), hash.begin() + required,
                       [](char c) { return c == '0'; });
}

int percentComplete(long long done, long long total) {
    if (total <= 0 || done <= 0) return 0;
    if (done >= total) return 100;
    return static_cast<int>(done * 100 / total);
}

namespace {

// Surrounding whitespace is the user pressing space, or a CRLF line ending
// surviving std::getline; neither makes the input invalid.
std::string_view trim(std::string_view text) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(whitespace);
    return text.substr(first, last - first + 1);
}

// Parses the WHOLE trimmed string, so trailing junk ("1x") is rejected rather
// than silently ignored the way std::stoi and operator>> ignore it.
template <typename T>
std::optional<T> parseWhole(std::string_view text) {
    text = trim(text);
    if (text.empty()) return std::nullopt;

    T value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);

    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return value;
}

} // namespace

std::optional<int> parseInt(std::string_view text) {
    return parseWhole<int>(text);
}

std::optional<double> parseDouble(std::string_view text) {
    const auto value = parseWhole<double>(text);
    // from_chars accepts "nan" and "inf"; a monetary amount must be neither.
    if (!value || !std::isfinite(*value)) return std::nullopt;
    return value;
}

} // namespace utils

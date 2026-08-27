#include "include/utils.h"
#include "include/sha.h"
#include <chrono>
#include <algorithm>
#include <vector>
#include <charconv>
#include <cmath>

namespace utils {

std::string sha256(const std::string& input) {
    return SHA256::hash(input);
}

namespace {

// Domain separation, as BIP-340 and RFC 6962 do: a leaf and an internal node
// go through the same hash function, so without a distinct prefix byte a
// transaction hash and an interior node are the same shape.
constexpr uint8_t MERKLE_LEAF_PREFIX = 0x00;
constexpr uint8_t MERKLE_NODE_PREFIX = 0x01;

SHA256::Digest merkleLeaf(const std::string& transaction_hash) {
    std::vector<uint8_t> input;
    input.reserve(1 + transaction_hash.size());
    input.push_back(MERKLE_LEAF_PREFIX);
    input.insert(input.end(), transaction_hash.begin(), transaction_hash.end());

    return SHA256::hashRaw(input.data(), input.size());
}

SHA256::Digest merkleNode(const SHA256::Digest& left, const SHA256::Digest& right) {
    // Raw digests, not their hex renderings: hashing the text would double
    // the input to every call up the tree.
    std::vector<uint8_t> input;
    input.reserve(1 + left.size() + right.size());
    input.push_back(MERKLE_NODE_PREFIX);
    input.insert(input.end(), left.begin(), left.end());
    input.insert(input.end(), right.begin(), right.end());

    return SHA256::hashRaw(input.data(), input.size());
}

} // namespace

std::string calculateMerkleRoot(const std::vector<std::string>& transactions) {
    if (transactions.empty()) {
        return sha256(""); // Empty merkle root
    }

    std::vector<SHA256::Digest> level;
    level.reserve(transactions.size());
    for (const auto& transaction : transactions) {
        level.push_back(merkleLeaf(transaction));
    }

    while (level.size() > 1) {
        std::vector<SHA256::Digest> next;
        next.reserve((level.size() + 1) / 2);

        for (size_t i = 0; i < level.size(); i += 2) {
            if (i + 1 < level.size()) {
                next.push_back(merkleNode(level[i], level[i + 1]));
            } else {
                // Promoted unchanged. Duplicating it made [a, b, c] and
                // [a, b, c, c] produce the same root.
                next.push_back(level[i]);
            }
        }

        level = std::move(next);
    }

    return bytesToHex(std::vector<unsigned char>(level[0].begin(), level[0].end()));
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

std::optional<std::int64_t> parseInt64(std::string_view text) {
    return parseWhole<std::int64_t>(text);
}

std::optional<double> parseDouble(std::string_view text) {
    const auto value = parseWhole<double>(text);
    // from_chars accepts "nan" and "inf"; a monetary amount must be neither.
    if (!value || !std::isfinite(*value)) return std::nullopt;
    return value;
}

} // namespace utils

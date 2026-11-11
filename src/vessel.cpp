#include "kitezh/vessel.hpp"

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    // пробел / таб / перевод строки в позывном
    for (size_t i = 0; i < vessel.call_sign.size(); i++) {
        char c = vessel.call_sign[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            return "bad call sign";
        }
    }

    if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }

    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int total = next.length_m;
    for (size_t i = 0; i < inside.size(); i++) {
        total += inside[i].length_m;
        total += kGapM;  // зазор только между соседями
    }
    return total <= kChamberLengthM;
}

}  // namespace kitezh


    // Helper functionality to match bone names with heuristics
    // (Copied from AnimationLoader to ensure consistent bone matching)

    // Try a number of heuristics to match an animation channel name to a skeleton bone name.
    // Returns true and sets outBoneId/outMatchedName on success.
    static bool tryFindBoneId(const std::shared_ptr<Skeleton>& skeleton, const std::string& originalName, int &outBoneId, std::string &outMatchedName) {
        // Helper lambdas
        auto stripAfterPipe = [](const std::string &s)->std::string {
            size_t p = s.find('|');
            return (p == std::string::npos) ? s : s.substr(0, p);
        };

        auto stripTrailingDigitsDot = [](const std::string &s)->std::string {
            size_t dot = s.rfind('.');
            if (dot == std::string::npos) return s;
            bool allDigits = true;
            for (size_t i = dot + 1; i < s.size(); ++i) {
                if (!std::isdigit(static_cast<unsigned char>(s[i]))) { allDigits = false; break; }
            }
            return allDigits ? s.substr(0, dot) : s;
        };

        auto startsWith = [](const std::string &s, const std::string &pref)->bool { return s.size() >= pref.size() && s.compare(0, pref.size(), pref) == 0; };

        auto stripAssimpFbx = [](const std::string &s)->std::string {
            std::string token = "_$AssimpFbx$_";
            size_t p = s.find(token);
            if (p != std::string::npos) return s.substr(0, p);
            return s;
        };

        std::vector<std::string> candidates;
        std::string base = stripAfterPipe(originalName);
        candidates.push_back(base);

        std::string cleaned = stripAssimpFbx(base);
        if (cleaned != base) candidates.push_back(cleaned);

        std::string stripped = stripTrailingDigitsDot(cleaned);
        if (stripped != cleaned) candidates.push_back(stripped);

        // Common prefixes used by exporters / rigs
        const std::vector<std::string> prefixes = {"Zombie_Ctrl_", "Zombie_Ctrl", "Zombie_", "Zombie", "Ctrl_", "ctrl_", "Bip01_", ""};
        for (const auto &pref : prefixes) {
            if (!pref.empty() && startsWith(base, pref)) {
                std::string t = base.substr(pref.size());
                candidates.push_back(t);
                // also try stripping trailing numeric suffix from this variant
                std::string t2 = stripTrailingDigitsDot(t);
                if (t2 != t) candidates.push_back(t2);
            }
        }

        // Try removing occurrences of "Ctrl" inside the name (e.g. "Zombie_Ctrl_LeftLeg" -> "Zombie_LeftLeg")
        if (base.find("Ctrl_") != std::string::npos) {
            std::string t = base;
            size_t pos = 0;
            while ((pos = t.find("Ctrl_", pos)) != std::string::npos) {
                t.erase(pos, 5);
            }
            candidates.push_back(t);
            candidates.push_back(stripTrailingDigitsDot(t));
        }

        // Ensure uniqueness while preserving order
        std::vector<std::string> uniq;
        for (const auto &c : candidates) {
            if (c.empty()) continue;
            if (std::find(uniq.begin(), uniq.end(), c) == uniq.end()) uniq.push_back(c);
        }

        // Try each candidate against the skeleton bone map
        for (const auto &cand : uniq) {
            auto it = skeleton->boneMap.find(cand);
            if (it != skeleton->boneMap.end()) {
                outBoneId = it->second;
                outMatchedName = cand;
                return true;
            }
        }

        return false;
    }

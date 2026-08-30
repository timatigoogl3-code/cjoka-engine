#pragma once
// Blackboard — Глобальное хранилище состояния мира, флагов, счётчиков и квестов
#include <string>
#include <unordered_map>
#include <sstream>
#include <iostream>

class Blackboard {
public:
    static Blackboard& Get() {
        static Blackboard instance;
        return instance;
    }

    // Bool
    void setBool(const std::string& key, bool value) { m_bools[key] = value; }
    bool getBool(const std::string& key, bool defaultVal = false) const {
        auto it = m_bools.find(key);
        return it != m_bools.end() ? it->second : defaultVal;
    }

    // Int
    void setInt(const std::string& key, int value) { m_ints[key] = value; }
    int getInt(const std::string& key, int defaultVal = 0) const {
        auto it = m_ints.find(key);
        return it != m_ints.end() ? it->second : defaultVal;
    }

    // Float
    void setFloat(const std::string& key, float value) { m_floats[key] = value; }
    float getFloat(const std::string& key, float defaultVal = 0.0f) const {
        auto it = m_floats.find(key);
        return it != m_floats.end() ? it->second : defaultVal;
    }

    // String
    void setString(const std::string& key, const std::string& value) { m_strings[key] = value; }
    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = m_strings.find(key);
        return it != m_strings.end() ? it->second : defaultVal;
    }

    bool hasKey(const std::string& key) const {
        return m_bools.count(key) || m_ints.count(key) || m_floats.count(key) || m_strings.count(key);
    }

    void clear() {
        m_bools.clear();
        m_ints.clear();
        m_floats.clear();
        m_strings.clear();
    }

    // Serialize to JSON formatted string
    std::string serialize() const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"bools\": {";
        bool first = true;
        for (const auto& [k, v] : m_bools) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << k << "\": " << (v ? "true" : "false");
        }
        ss << "},\n";

        ss << "  \"ints\": {";
        first = true;
        for (const auto& [k, v] : m_ints) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << k << "\": " << v;
        }
        ss << "},\n";

        ss << "  \"floats\": {";
        first = true;
        for (const auto& [k, v] : m_floats) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << k << "\": " << v;
        }
        ss << "},\n";

        ss << "  \"strings\": {";
        first = true;
        for (const auto& [k, v] : m_strings) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << k << "\": \"" << v << "\"";
        }
        ss << "}\n";
        ss << "}";
        return ss.str();
    }

    // Deserialize from JSON formatted string
    bool deserialize(const std::string& json) {
        clear();
        auto extractSection = [](const std::string& str, const std::string& secName) -> std::string {
            size_t k = str.find("\"" + secName + "\"");
            if (k == std::string::npos) return "";
            size_t ob = str.find('{', k);
            if (ob == std::string::npos) return "";
            size_t cb = str.find('}', ob);
            if (cb == std::string::npos) return "";
            return str.substr(ob + 1, cb - ob - 1);
        };

        // Bools
        std::string bSec = extractSection(json, "bools");
        if (!bSec.empty()) {
            size_t pos = 0;
            while (true) {
                size_t q1 = bSec.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = bSec.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string k = bSec.substr(q1 + 1, q2 - q1 - 1);
                size_t colon = bSec.find(':', q2);
                if (colon == std::string::npos) break;
                size_t end = bSec.find_first_of(",}", colon);
                std::string valStr = bSec.substr(colon + 1, (end == std::string::npos ? bSec.size() : end) - (colon + 1));
                bool val = (valStr.find("true") != std::string::npos || valStr.find("1") != std::string::npos);
                m_bools[k] = val;
                if (end == std::string::npos) break;
                pos = end + 1;
            }
        }

        // Ints
        std::string iSec = extractSection(json, "ints");
        if (!iSec.empty()) {
            size_t pos = 0;
            while (true) {
                size_t q1 = iSec.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = iSec.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string k = iSec.substr(q1 + 1, q2 - q1 - 1);
                size_t colon = iSec.find(':', q2);
                if (colon == std::string::npos) break;
                size_t end = iSec.find_first_of(",}", colon);
                std::string valStr = iSec.substr(colon + 1, (end == std::string::npos ? iSec.size() : end) - (colon + 1));
                try { m_ints[k] = std::stoi(valStr); } catch(...) {}
                if (end == std::string::npos) break;
                pos = end + 1;
            }
        }

        // Floats
        std::string fSec = extractSection(json, "floats");
        if (!fSec.empty()) {
            size_t pos = 0;
            while (true) {
                size_t q1 = fSec.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = fSec.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string k = fSec.substr(q1 + 1, q2 - q1 - 1);
                size_t colon = fSec.find(':', q2);
                if (colon == std::string::npos) break;
                size_t end = fSec.find_first_of(",}", colon);
                std::string valStr = fSec.substr(colon + 1, (end == std::string::npos ? fSec.size() : end) - (colon + 1));
                try { m_floats[k] = std::stof(valStr); } catch(...) {}
                if (end == std::string::npos) break;
                pos = end + 1;
            }
        }

        // Strings
        std::string sSec = extractSection(json, "strings");
        if (!sSec.empty()) {
            size_t pos = 0;
            while (true) {
                size_t q1 = sSec.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = sSec.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string k = sSec.substr(q1 + 1, q2 - q1 - 1);
                size_t colon = sSec.find(':', q2);
                if (colon == std::string::npos) break;
                size_t valQ1 = sSec.find('"', colon);
                if (valQ1 == std::string::npos) break;
                size_t valQ2 = sSec.find('"', valQ1 + 1);
                if (valQ2 == std::string::npos) break;
                std::string val = sSec.substr(valQ1 + 1, valQ2 - valQ1 - 1);
                m_strings[k] = val;
                pos = valQ2 + 1;
            }
        }

        return true;
    }

    const std::unordered_map<std::string, bool>& bools() const { return m_bools; }
    const std::unordered_map<std::string, int>& ints() const { return m_ints; }
    const std::unordered_map<std::string, float>& floats() const { return m_floats; }
    const std::unordered_map<std::string, std::string>& strings() const { return m_strings; }

private:
    Blackboard() = default;

    std::unordered_map<std::string, bool> m_bools;
    std::unordered_map<std::string, int> m_ints;
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, std::string> m_strings;
};

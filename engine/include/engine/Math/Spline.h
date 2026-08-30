#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Math {

enum class SplineType {
    CatmullRom,
    Bezier,
    Linear
};

struct SplinePoint {
    glm::vec3 position{0.0f};
    glm::vec3 tangent{0.0f, 0.0f, 1.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    float distance = 0.0f; // Arc-length distance along the spline
    glm::vec3 scale{1.0f}; // Local curve scale
    float roll = 0.0f;    // Banking / roll angle in degrees
};

class Spline3D {
public:
    Spline3D(SplineType type = SplineType::CatmullRom, bool closed = false)
        : m_type(type), m_closed(closed) {}

    void setClosed(bool closed) { m_closed = closed; recalculateSpline(); }
    bool isClosed() const { return m_closed; }

    void setType(SplineType type) { m_type = type; recalculateSpline(); }
    SplineType type() const { return m_type; }

    void addControlPoint(const glm::vec3& p) {
        m_controlPoints.push_back(p);
        recalculateSpline();
    }

    void setControlPoints(const std::vector<glm::vec3>& points) {
        m_controlPoints = points;
        recalculateSpline();
    }

    const std::vector<glm::vec3>& controlPoints() const { return m_controlPoints; }
    std::vector<glm::vec3>& controlPoints() { return m_controlPoints; }

    void clear() {
        m_controlPoints.clear();
        m_samples.clear();
        m_totalLength = 0.0f;
    }

    size_t size() const { return m_controlPoints.size(); }
    float totalLength() const { return m_totalLength; }

    // Evaluates position along normalized progress t in [0.0, 1.0]
    glm::vec3 evaluatePosition(float t) const {
        if (m_samples.empty()) return m_controlPoints.empty() ? glm::vec3(0.0f) : m_controlPoints[0];
        float dist = std::clamp(t, 0.0f, 1.0f) * m_totalLength;
        return evaluateAtDistance(dist).position;
    }

    // Evaluates tangent along normalized progress t in [0.0, 1.0]
    glm::vec3 evaluateTangent(float t) const {
        if (m_samples.empty()) return glm::vec3(0.0f, 0.0f, 1.0f);
        float dist = std::clamp(t, 0.0f, 1.0f) * m_totalLength;
        return evaluateAtDistance(dist).tangent;
    }

    // Samples exact point along arc-length distance in [0, totalLength]
    SplinePoint evaluateAtDistance(float distance) const {
        if (m_samples.empty()) return SplinePoint{};
        if (m_samples.size() == 1) return m_samples[0];

        if (m_closed && m_totalLength > 0.001f) {
            distance = std::fmod(distance, m_totalLength);
            if (distance < 0.0f) distance += m_totalLength;
        } else {
            distance = std::clamp(distance, 0.0f, m_totalLength);
        }

        // Binary search in sorted samples
        auto it = std::upper_bound(m_samples.begin(), m_samples.end(), distance,
            [](float dist, const SplinePoint& pt) {
                return dist < pt.distance;
            });

        if (it == m_samples.begin()) return m_samples.front();
        if (it == m_samples.end()) return m_samples.back();

        const SplinePoint& p1 = *(it - 1);
        const SplinePoint& p2 = *it;

        float segmentLen = p2.distance - p1.distance;
        float factor = (segmentLen > 0.0001f) ? (distance - p1.distance) / segmentLen : 0.0f;

        SplinePoint result;
        result.position = glm::mix(p1.position, p2.position, factor);
        result.tangent = glm::normalize(glm::mix(p1.tangent, p2.tangent, factor));
        result.normal = glm::normalize(glm::mix(p1.normal, p2.normal, factor));
        result.distance = distance;
        result.scale = glm::mix(p1.scale, p2.scale, factor);
        result.roll = glm::mix(p1.roll, p2.roll, factor);
        return result;
    }

    // Finds the closest point on the spline to an arbitrary 3D position
    // Returns {closestDistanceAlongSpline, closestPoint}
    std::pair<float, SplinePoint> findClosestPoint(const glm::vec3& queryPos, float initialGuessDist = -1.0f) const {
        if (m_samples.empty()) return {0.0f, SplinePoint{}};

        float bestDistSq = 1e12f;
        float bestArcDist = 0.0f;

        // Localized search if initial guess is provided
        if (initialGuessDist >= 0.0f && m_samples.size() > 20) {
            float window = 40.0f; // search within +/- 40 meters
            float minD = initialGuessDist - window;
            float maxD = initialGuessDist + window;
            for (size_t i = 0; i < m_samples.size(); ++i) {
                float d = m_samples[i].distance;
                if ((d >= minD && d <= maxD) || (m_closed && (d < window || d > m_totalLength - window))) {
                    float distSq = glm::distance2(queryPos, m_samples[i].position);
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        bestArcDist = d;
                    }
                }
            }
        } else {
            // Full search
            for (const auto& sample : m_samples) {
                float distSq = glm::distance2(queryPos, sample.position);
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    bestArcDist = sample.distance;
                }
            }
        }

        // Refine with local step search
        float step = 0.5f;
        for (float offset = -2.0f; offset <= 2.0f; offset += step) {
            float testDist = bestArcDist + offset;
            SplinePoint sp = evaluateAtDistance(testDist);
            float distSq = glm::distance2(queryPos, sp.position);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestArcDist = testDist;
            }
        }

        return {bestArcDist, evaluateAtDistance(bestArcDist)};
    }

    const std::vector<SplinePoint>& samples() const { return m_samples; }

    void recalculateSpline() {
        m_samples.clear();
        m_totalLength = 0.0f;
        if (m_controlPoints.size() < 2) return;

        size_t numPoints = m_controlPoints.size();
        int numSegments = m_closed ? static_cast<int>(numPoints) : static_cast<int>(numPoints) - 1;
        const int subdivisionsPerSegment = 24;

        glm::vec3 prevPos = m_controlPoints[0];
        SplinePoint firstSample;
        firstSample.position = prevPos;
        firstSample.distance = 0.0f;
        m_samples.push_back(firstSample);

        for (int seg = 0; seg < numSegments; ++seg) {
            glm::vec3 p0 = getControlPoint(seg - 1);
            glm::vec3 p1 = getControlPoint(seg);
            glm::vec3 p2 = getControlPoint(seg + 1);
            glm::vec3 p3 = getControlPoint(seg + 2);

            for (int sub = 1; sub <= subdivisionsPerSegment; ++sub) {
                float u = float(sub) / float(subdivisionsPerSegment);
                glm::vec3 pos;
                if (m_type == SplineType::CatmullRom) {
                    pos = catmullRom(p0, p1, p2, p3, u);
                } else if (m_type == SplineType::Bezier) {
                    pos = glm::mix(glm::mix(p1, p2, u), glm::mix(p2, p3, u), u);
                } else {
                    pos = glm::mix(p1, p2, u);
                }

                float stepDist = glm::distance(prevPos, pos);
                m_totalLength += stepDist;

                SplinePoint sp;
                sp.position = pos;
                sp.distance = m_totalLength;
                m_samples.push_back(sp);
                prevPos = pos;
            }
        }

        // Calculate tangents & normals
        for (size_t i = 0; i < m_samples.size(); ++i) {
            glm::vec3 prevP = (i == 0) ? (m_closed ? m_samples[m_samples.size() - 2].position : m_samples[0].position) : m_samples[i - 1].position;
            glm::vec3 nextP = (i + 1 == m_samples.size()) ? (m_closed ? m_samples[1].position : m_samples.back().position) : m_samples[i + 1].position;
            glm::vec3 tang = nextP - prevP;
            if (glm::length(tang) > 0.0001f) {
                m_samples[i].tangent = glm::normalize(tang);
            } else {
                m_samples[i].tangent = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            // Approximate normal (up vector projected onto perpendicular plane)
            glm::vec3 up(0.0f, 1.0f, 0.0f);
            glm::vec3 right = glm::cross(m_samples[i].tangent, up);
            if (glm::length(right) > 0.001f) {
                right = glm::normalize(right);
                m_samples[i].normal = glm::normalize(glm::cross(right, m_samples[i].tangent));
            } else {
                m_samples[i].normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

private:
    glm::vec3 getControlPoint(int idx) const {
        int n = static_cast<int>(m_controlPoints.size());
        if (n == 0) return glm::vec3(0.0f);
        if (m_closed) {
            idx = (idx % n + n) % n;
            return m_controlPoints[static_cast<size_t>(idx)];
        }
        idx = std::clamp(idx, 0, n - 1);
        return m_controlPoints[static_cast<size_t>(idx)];
    }

    static glm::vec3 catmullRom(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3, float t) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) +
                       (-p0 + p2) * t +
                       (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                       (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }

    SplineType m_type = SplineType::CatmullRom;
    bool m_closed = false;
    std::vector<glm::vec3> m_controlPoints;
    std::vector<SplinePoint> m_samples;
    float m_totalLength = 0.0f;
};

} // namespace Math

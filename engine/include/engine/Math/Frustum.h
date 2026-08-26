#pragma once
#include <glm/glm.hpp>
#include <array>

namespace Math {

struct Plane {
    glm::vec3 normal = { 0.f, 1.f, 0.f };
    float distance = 0.f;

    Plane() = default;
    Plane(const glm::vec3& p1, const glm::vec3& norm)
        : normal(glm::normalize(norm)), distance(glm::dot(normal, p1)) {}

    float getSignedDistanceToPlane(const glm::vec3& point) const {
        return glm::dot(normal, point) - distance;
    }
};

struct Frustum {
    Plane topFace;
    Plane bottomFace;
    Plane rightFace;
    Plane leftFace;
    Plane farFace;
    Plane nearFace;

    // AABB intersection check
    bool isOnFrustum(const glm::vec3& minExtents, const glm::vec3& maxExtents, const glm::mat4& transform) const {
        // Transform the 8 corners of the AABB
        glm::vec3 corners[8] = {
            glm::vec3(transform * glm::vec4(minExtents.x, minExtents.y, minExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(maxExtents.x, minExtents.y, minExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(minExtents.x, maxExtents.y, minExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(maxExtents.x, maxExtents.y, minExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(minExtents.x, minExtents.y, maxExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(maxExtents.x, minExtents.y, maxExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(minExtents.x, maxExtents.y, maxExtents.z, 1.0f)),
            glm::vec3(transform * glm::vec4(maxExtents.x, maxExtents.y, maxExtents.z, 1.0f)),
        };

        const Plane* planes[] = { &leftFace, &rightFace, &topFace, &bottomFace, &nearFace, &farFace };
        
        for (const Plane* plane : planes) {
            int outCount = 0;
            for (int i = 0; i < 8; i++) {
                if (plane->getSignedDistanceToPlane(corners[i]) < 0.0f) {
                    outCount++;
                }
            }
            if (outCount == 8) return false; // All corners are on the negative side of this plane
        }
        return true;
    }

    static Frustum createFrustumFromMatrix(const glm::mat4& mat) {
        Frustum f;
        f.leftFace.normal = glm::vec3(mat[0][3] + mat[0][0], mat[1][3] + mat[1][0], mat[2][3] + mat[2][0]);
        f.leftFace.distance = -(mat[3][3] + mat[3][0]);

        f.rightFace.normal = glm::vec3(mat[0][3] - mat[0][0], mat[1][3] - mat[1][0], mat[2][3] - mat[2][0]);
        f.rightFace.distance = -(mat[3][3] - mat[3][0]);

        f.bottomFace.normal = glm::vec3(mat[0][3] + mat[0][1], mat[1][3] + mat[1][1], mat[2][3] + mat[2][1]);
        f.bottomFace.distance = -(mat[3][3] + mat[3][1]);

        f.topFace.normal = glm::vec3(mat[0][3] - mat[0][1], mat[1][3] - mat[1][1], mat[2][3] - mat[2][1]);
        f.topFace.distance = -(mat[3][3] - mat[3][1]);

        f.nearFace.normal = glm::vec3(mat[0][3] + mat[0][2], mat[1][3] + mat[1][2], mat[2][3] + mat[2][2]);
        f.nearFace.distance = -(mat[3][3] + mat[3][2]);

        f.farFace.normal = glm::vec3(mat[0][3] - mat[0][2], mat[1][3] - mat[1][2], mat[2][3] - mat[2][2]);
        f.farFace.distance = -(mat[3][3] - mat[3][2]);

        auto normalizePlane = [](Plane& p) {
            float mag = glm::length(p.normal);
            if(mag > 0.0f) {
                p.normal /= mag;
                p.distance /= mag;
            }
        };

        normalizePlane(f.leftFace);
        normalizePlane(f.rightFace);
        normalizePlane(f.topFace);
        normalizePlane(f.bottomFace);
        normalizePlane(f.nearFace);
        normalizePlane(f.farFace);

        return f;
    }
};

}

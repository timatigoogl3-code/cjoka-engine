#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Physics/Physics.h"

namespace cjoka_phys {

struct WheelSetup {
    glm::vec3 localOffset{0.0f};          // Position relative to chassis center
    float radius = 0.35f;                 // Wheel radius in meters
    float width = 0.25f;                  // Wheel width
    float suspensionRestLength = 0.45f;   // Suspension max extension (meters)
    float springRate = 35000.0f;          // Suspension spring rate (N/m)
    float damperRate = 3200.0f;           // Suspension damper rate (N*s/m)
    bool isSteerable = false;             // Front wheels turn with steering input
    bool isDriven = true;                 // Receives engine drive torque
    bool isBraked = true;                 // Receives foot brake torque
    bool isHandbrakeAffected = false;     // Locks on handbrake (typically rear wheels)
    float frictionMultiplier = 1.0f;      // Tire grip scaling

    // Runtime state
    bool isGrounded = false;
    float currentSuspensionLength = 0.45f;
    float rotationAngle = 0.0f;           // Rolling angle in radians
    glm::vec3 contactPoint{0.0f};
    glm::vec3 contactNormal{0.0f, 1.0f, 0.0f};
};

struct WheeledVehicleComponent {
    // --- Chassis & Mass Properties ---
    float massKg = 1450.0f;
    float dragCoefficient = 0.32f;
    float downforceCoefficient = 0.45f;
    glm::vec3 centerOfMassOffset{0.0f, -0.25f, 0.0f};

    // --- Engine & Transmission ---
    float peakTorqueNm = 650.0f;
    float idleRPM = 850.0f;
    float maxRPM = 7600.0f;
    float redlineRPM = 6800.0f;
    float currentRPM = 850.0f;
    int currentGear = 1; // -1: Reverse, 0: Neutral, 1..6: Forward
    const int maxGears = 6;
    float gearRatios[7] = { 3.82f, 3.45f, 2.20f, 1.55f, 1.20f, 0.95f, 0.78f }; // Reverse, 1, 2, 3, 4, 5, 6
    float finalDriveRatio = 3.73f;
    bool automaticTransmission = true;
    float shiftDelay = 0.15f;
    float shiftTimer = 0.0f;

    // --- Steering & Brakes ---
    float maxSteerAngleDeg = 35.0f;
    float steerSpeedDegPerSec = 85.0f;
    float currentSteerAngleDeg = 0.0f;
    float brakeTorqueNm = 3000.0f;
    float handbrakeTorqueNm = 4500.0f;

    // --- 4 Wheels (FL, FR, RL, RR) ---
    WheelSetup wheels[4];

    // --- Generic Control Inputs (driven by user script or controller) ---
    float throttleInput = 0.0f; // -1.0f (reverse) to +1.0f (forward)
    float steerInput = 0.0f;    // -1.0f (right) to +1.0f (left)
    float brakeInput = 0.0f;    // 0.0f to 1.0f
    bool handbrake = false;

    // --- Live Physics Output & Telemetry ---
    float forwardSpeedMps = 0.0f;
    float lateralSpeedMps = 0.0f;
    float speedKmh = 0.0f;
    glm::vec3 linearVelocity{0.0f};
    float yawRate = 0.0f; // deg/sec

    WheeledVehicleComponent() {
        // Standard 4-wheel passenger car layout (track: ~1.6m, wheelbase: ~2.7m)
        float halfTrack = 0.80f;
        float frontZ = 1.35f;
        float rearZ = -1.35f;
        float wheelY = 0.35f;

        // Front Left (0)
        wheels[0].localOffset = glm::vec3(-halfTrack, wheelY, frontZ);
        wheels[0].isSteerable = true;
        wheels[0].isDriven = false;
        wheels[0].isBraked = true;
        wheels[0].isHandbrakeAffected = false;

        // Front Right (1)
        wheels[1].localOffset = glm::vec3(halfTrack, wheelY, frontZ);
        wheels[1].isSteerable = true;
        wheels[1].isDriven = false;
        wheels[1].isBraked = true;
        wheels[1].isHandbrakeAffected = false;

        // Rear Left (2)
        wheels[2].localOffset = glm::vec3(-halfTrack, wheelY, rearZ);
        wheels[2].isSteerable = false;
        wheels[2].isDriven = true;
        wheels[2].isBraked = true;
        wheels[2].isHandbrakeAffected = true;

        // Rear Right (3)
        wheels[3].localOffset = glm::vec3(halfTrack, wheelY, rearZ);
        wheels[3].isSteerable = false;
        wheels[3].isDriven = true;
        wheels[3].isBraked = true;
        wheels[3].isHandbrakeAffected = true;
    }
};

class WheeledVehicleSystem {
public:
    static void Update(Registry& reg, cjoka_phys::World* physWorld, float dt) {
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        for (Entity e : reg.view<WheeledVehicleComponent, Transform>()) {
            auto& veh = reg.get<WheeledVehicleComponent>(e);
            auto& tr = reg.get<Transform>(e);

            // 1. Smooth Steering
            float targetSteer = -veh.steerInput * veh.maxSteerAngleDeg;
            float steerDelta = veh.steerSpeedDegPerSec * dt;
            if (veh.currentSteerAngleDeg < targetSteer) {
                veh.currentSteerAngleDeg = std::min(targetSteer, veh.currentSteerAngleDeg + steerDelta);
            } else if (veh.currentSteerAngleDeg > targetSteer) {
                veh.currentSteerAngleDeg = std::max(targetSteer, veh.currentSteerAngleDeg - steerDelta);
            }

            // Chassis Orientation
            float yawRad = glm::radians(tr.rotation.y);
            glm::vec3 fwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
            glm::vec3 right(fwd.z, 0.0f, -fwd.x);
            glm::vec3 up(0.0f, 1.0f, 0.0f);

            // 2. Wheel Raycast & Suspension Simulation
            int groundedWheelCount = 0;
            for (int i = 0; i < 4; ++i) {
                auto& w = veh.wheels[i];
                glm::vec3 wheelWorldPos = tr.position + right * w.localOffset.x + up * w.localOffset.y + fwd * w.localOffset.z;
                float traceDist = w.suspensionRestLength + w.radius;

                glm::vec3 hitPoint, hitNormal;
                Entity hitEnt = NullEntity;
                if (physWorld) {
                    hitEnt = physWorld->Raycast(reg, wheelWorldPos, glm::vec3(0.0f, -1.0f, 0.0f), traceDist, &hitPoint, &hitNormal);
                }

                if (hitEnt != NullEntity) {
                    w.isGrounded = true;
                    groundedWheelCount++;
                    w.contactPoint = hitPoint;
                    w.contactNormal = hitNormal;
                    float dist = glm::distance(wheelWorldPos, hitPoint);
                    w.currentSuspensionLength = std::clamp(dist - w.radius, 0.05f, w.suspensionRestLength);
                } else {
                    w.isGrounded = false;
                    w.currentSuspensionLength = w.suspensionRestLength;
                }
            }

            // 3. Engine & Transmission Simulation
            if (veh.shiftTimer > 0.0f) {
                veh.shiftTimer -= dt;
            } else if (veh.automaticTransmission) {
                // Auto gear shifting
                if (veh.currentRPM > veh.redlineRPM && veh.currentGear < veh.maxGears && veh.currentGear >= 1) {
                    veh.currentGear++;
                    veh.shiftTimer = veh.shiftDelay;
                    veh.currentRPM = veh.currentRPM * 0.72f;
                } else if (veh.currentRPM < 2000.0f && veh.currentGear > 1) {
                    veh.currentGear--;
                    veh.shiftTimer = veh.shiftDelay;
                    veh.currentRPM = std::min(veh.maxRPM, veh.currentRPM * 1.35f);
                }
            }

            // Engine Torque Output
            float currentRatio = (veh.currentGear >= 1 && veh.currentGear <= 6) ? veh.gearRatios[veh.currentGear] : veh.gearRatios[0];
            float driveTorque = veh.throttleInput * veh.peakTorqueNm * currentRatio * veh.finalDriveRatio;

            // 4. Longitudinal & Lateral Chassis Motion
            float forwardAccel = 0.0f;
            if (groundedWheelCount > 0) {
                float driveForce = (driveTorque / (veh.wheels[0].radius > 0.01f ? veh.wheels[0].radius : 0.35f));
                forwardAccel = driveForce / (veh.massKg > 10.0f ? veh.massKg : 1450.0f);
            }

            // Brakes & Drag
            float currentSpeed = glm::dot(veh.linearVelocity, fwd);
            float dragForce = 0.5f * 1.225f * veh.dragCoefficient * 2.2f * currentSpeed * std::abs(currentSpeed);
            float dragAccel = dragForce / veh.massKg;

            if (veh.brakeInput > 0.01f || veh.handbrake) {
                float totalBrake = (veh.brakeInput * veh.brakeTorqueNm + (veh.handbrake ? veh.handbrakeTorqueNm : 0.0f)) / veh.wheels[0].radius;
                float brakeDecel = (totalBrake / veh.massKg);
                if (std::abs(currentSpeed) > 0.1f) {
                    forwardAccel -= (currentSpeed > 0.0f ? 1.0f : -1.0f) * brakeDecel;
                }
            }
            forwardAccel -= dragAccel;

            // Update Velocity
            currentSpeed += forwardAccel * dt;
            if (std::abs(veh.throttleInput) < 0.01f && veh.brakeInput < 0.01f && !veh.handbrake) {
                currentSpeed *= std::pow(0.985f, dt * 60.0f); // rolling resistance
            }

            // Lateral Grip & Turning
            float steerRad = glm::radians(veh.currentSteerAngleDeg);
            float turnRateDeg = 0.0f;
            if (std::abs(currentSpeed) > 0.1f) {
                float wheelbase = std::abs(veh.wheels[0].localOffset.z - veh.wheels[2].localOffset.z);
                if (wheelbase < 0.5f) wheelbase = 2.7f;
                float curvature = std::tan(steerRad) / wheelbase;
                turnRateDeg = glm::degrees(currentSpeed * curvature);
                if (veh.handbrake) turnRateDeg *= 1.45f; // handbrake yaw kick
            }

            tr.rotation.y += turnRateDeg * dt;
            yawRad = glm::radians(tr.rotation.y);
            fwd = glm::vec3(std::sin(yawRad), 0.0f, std::cos(yawRad));
            right = glm::vec3(fwd.z, 0.0f, -fwd.x);

            veh.linearVelocity = fwd * currentSpeed;
            veh.forwardSpeedMps = currentSpeed;
            veh.lateralSpeedMps = glm::dot(veh.linearVelocity, right);
            veh.speedKmh = currentSpeed * 3.6f;

            // Engine RPM Tracking
            float wheelAngularSpeed = std::abs(currentSpeed) / veh.wheels[0].radius;
            float targetRPM = (wheelAngularSpeed * currentRatio * veh.finalDriveRatio * 60.0f) / (2.0f * 3.14159f);
            if (std::abs(veh.throttleInput) > 0.1f && std::abs(currentSpeed) < 2.0f) {
                targetRPM = std::max(targetRPM, veh.idleRPM + std::abs(veh.throttleInput) * (veh.maxRPM - veh.idleRPM) * 0.6f);
            }
            veh.currentRPM = glm::mix(veh.currentRPM, std::clamp(targetRPM, veh.idleRPM, veh.maxRPM), dt * 10.0f);

            // Wheel Rotation Visuals
            for (int i = 0; i < 4; ++i) {
                veh.wheels[i].rotationAngle += (currentSpeed / veh.wheels[i].radius) * dt;
            }

            // Move chassis
            tr.position += veh.linearVelocity * dt;
        }
    }
};

} // namespace cjoka_phys

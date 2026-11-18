#ifndef SWARM_COMMON_HLSLI
#define SWARM_COMMON_HLSLI

struct DroneData {
    float3 position;
    float hp;
    float3 velocity;
    uint state; // 0: Cruise, 1: Charge, 2: Dead, 3: Crashing
    float3 target;
    float stateTimer;
};

cbuffer SwarmGlobalConstants : register(b0) {
    float3 gPlayerPosition;
    float gDeltaTime;
    uint gDroneCount;
    float gMaxSpeed;
    float gNeighborRadius;
    float gSeparationDist;
    float3 gBoundsMin;
    float gSeparationWeight;
    float3 gBoundsMax;
    float gAlignmentWeight;
    float gCohesionWeight;
    float gTargetWeight;
    float gAttackDistance;
    float gChargeSpeed;
    float3 gCameraPosition;
    float gPadding0;

    float3 gRayOrigin;
    float gRayRadius;
    float3 gRayDirection;
    float gAttackDamage;
};

#endif // SWARM_COMMON_HLSLI

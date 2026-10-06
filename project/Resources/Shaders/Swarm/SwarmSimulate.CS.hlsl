#include "SwarmCommon.hlsli"

StructuredBuffer<DroneData> gPrevDrones : register(t0);
RWStructuredBuffer<DroneData> gNextDrones : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gDroneCount)
    {
        return;
    }

    DroneData self = gPrevDrones[index];

    // 死亡している場合はそのまま保持
    if (self.hp <= 0.0f || self.state == 2)
    {
        self.state = 2;
        self.hp = 0.0f;
        gNextDrones[index] = self;
        return;
    }

    // プレイヤーの射撃（レイキャスト）判定
    if (gAttackDamage > 0.0f)
    {
        float3 toDrone = self.position - gRayOrigin;
        float rayT = dot(toDrone, gRayDirection);
        if (rayT > 0.0f)
        {
            float3 closestPt = gRayOrigin + gRayDirection * rayT;
            float distToRay = length(self.position - closestPt);
            if (distToRay <= gRayRadius)
            {
                self.hp -= gAttackDamage;
                if (self.hp <= 0.0f)
                {
                    self.state = 2; // Dead
                    self.hp = 0.0f;
                    gNextDrones[index] = self;
                    return;
                }
            }
        }
    }

    // タイマー更新
    self.stateTimer += gDeltaTime;

    float3 toPlayer = gPlayerPosition - self.position;
    float distToPlayer = length(toPlayer);
    float3 dirToPlayer = distToPlayer > 0.001f ? (toPlayer / distToPlayer) : float3(0, 0, 1);

    // AI判定: 巡航状態かつプレイヤーに接近した場合、一定確率で突撃(Kamikaze)へ遷移
    if (self.state == 0)
    {
        if (distToPlayer < gAttackDistance && self.stateTimer > 1.5f)
        {
            // インデックスに基づいた擬似乱数で一斉に突撃しないよう分散
            float pseudoRand = frac(sin(dot(self.position.xy, float2(12.9898, 78.233)) + self.stateTimer) * 43758.5453);
            if (pseudoRand < 0.25f)
            {
                self.state = 1; // Kamikaze
                self.stateTimer = 0.0f;
            }
        }
    }

    // 挙動分岐
    if (self.state == 1) // Kamikaze (特攻自爆)
    {
        // プレイヤーに極めて接近したら自爆判定
        if (distToPlayer < 2.5f)
        {
            self.hp = 0.0f;
            self.state = 2; // Dead
            gNextDrones[index] = self;
            return;
        }

        // プレイヤーへ直進
        float3 targetVel = dirToPlayer * gKamikazeSpeed;
        self.velocity = lerp(self.velocity, targetVel, saturate(gDeltaTime * 4.0f));
    }
    else // Cruise (Boids群れ移動)
    {
        float3 separation = float3(0, 0, 0);
        float3 alignment = float3(0, 0, 0);
        float3 cohesion = float3(0, 0, 0);
        uint neighborCount = 0;

        // 近傍探索 (サンプル数を絞るか全走査)
        // 負荷考慮: 全ドローン走査 (1024機程度なら256スレッドで高速に動作)
        for (uint i = 0; i < gDroneCount; ++i)
        {
            if (i == index) continue;

            DroneData other = gPrevDrones[i];
            if (other.hp <= 0.0f || other.state == 2) continue;

            float3 diff = self.position - other.position;
            float dist = length(diff);

            if (dist < gNeighborRadius && dist > 0.0001f)
            {
                // 分離 (距離が近いほど強く反発)
                if (dist < gSeparationDist)
                {
                    separation += (diff / dist) * (1.0f - dist / gSeparationDist);
                }

                // 整列 (周囲の平均速度)
                alignment += other.velocity;

                // 結合 (周囲の重心)
                cohesion += other.position;

                neighborCount++;
            }
        }

        float3 acceleration = float3(0, 0, 0);

        if (neighborCount > 0)
        {
            alignment /= (float)neighborCount;
            alignment = length(alignment) > 0.001f ? normalize(alignment) * gMaxSpeed - self.velocity : float3(0, 0, 0);

            cohesion /= (float)neighborCount;
            float3 cohesionDir = cohesion - self.position;
            cohesion = length(cohesionDir) > 0.001f ? normalize(cohesionDir) * gMaxSpeed - self.velocity : float3(0, 0, 0);

            acceleration += separation * gSeparationWeight;
            acceleration += alignment * gAlignmentWeight;
            acceleration += cohesion * gCohesionWeight;
        }

        // プレイヤー誘導 (群れ全体をプレイヤー周辺へ緩やかに導く)
        float3 targetDir = dirToPlayer;
        float3 targetSteer = targetDir * gMaxSpeed - self.velocity;
        acceleration += targetSteer * gTargetWeight;

        // 境界反発 (エリア外に出ないように中央へ押し戻す)
        float3 boundsForce = float3(0, 0, 0);
        if (self.position.x < gBoundsMin.x) boundsForce.x += (gBoundsMin.x - self.position.x);
        if (self.position.x > gBoundsMax.x) boundsForce.x += (gBoundsMax.x - self.position.x);
        if (self.position.y < gBoundsMin.y) boundsForce.y += (gBoundsMin.y - self.position.y);
        if (self.position.y > gBoundsMax.y) boundsForce.y += (gBoundsMax.y - self.position.y);
        if (self.position.z < gBoundsMin.z) boundsForce.z += (gBoundsMin.z - self.position.z);
        if (self.position.z > gBoundsMax.z) boundsForce.z += (gBoundsMax.z - self.position.z);
        acceleration += boundsForce * 2.0f;

        // 速度更新
        self.velocity += acceleration * gDeltaTime;
        float speed = length(self.velocity);
        if (speed > gMaxSpeed)
        {
            self.velocity = (self.velocity / speed) * gMaxSpeed;
        }
        else if (speed < 1.0f)
        {
            self.velocity = length(self.velocity) > 0.001f ? normalize(self.velocity) * 1.0f : float3(0, 0, 2.0f);
        }
    }

    // 位置更新
    self.position += self.velocity * gDeltaTime;

    gNextDrones[index] = self;
}

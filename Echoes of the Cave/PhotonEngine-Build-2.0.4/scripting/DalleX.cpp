#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class OscillatingPlatform : public Engine::Scripting::NativeScript {
public:
    float moveSpeed = 2.0f;
    float offsetX = 3.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
    float waitTime = 0.0f;
    float pushForce = 5.0f;   // Force de répulsion sur le joueur
    float boxSizeX = 1.0f;   // Taille de la hitbox du cube sur X
    float boxSizeY = 1.0f;   // Taille de la hitbox du cube sur Y
    float boxSizeZ = 1.0f;   // Taille de la hitbox du cube sur Z

    float baseX = 0.0f, baseY = 0.0f, baseZ = 0.0f;
    float timer = 0.0f;
    bool  initialized = false;
    bool  goingForward = true;
    bool  waiting = false;

    Engine::ECS::Entity playerEntity = Engine::ECS::NULL_ENTITY;

    void OnInit() override {
        Inspect("Move Speed", &moveSpeed);
        Inspect("Offset X", &offsetX);
        Inspect("Offset Y", &offsetY);
        Inspect("Offset Z", &offsetZ);
        Inspect("Wait Time", &waitTime);
        Inspect("Push Force", &pushForce);
        Inspect("Box Size X", &boxSizeX);
        Inspect("Box Size Y", &boxSizeY);
        Inspect("Box Size Z", &boxSizeZ);
    }

    void OnCreate() override {
        FindPlayer();
    }

    void FindPlayer() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Player") {
                playerEntity = e;
                break;
            }
        }
    }

    bool CheckAABB(const glm::vec3& cubePos, const glm::vec3& playerPos) {
        float halfX = boxSizeX * 0.5f;
        float halfY = boxSizeY * 0.5f;
        float halfZ = boxSizeZ * 0.5f;

        return (playerPos.x >= cubePos.x - halfX && playerPos.x <= cubePos.x + halfX) &&
            (playerPos.y >= cubePos.y - halfY && playerPos.y <= cubePos.y + halfY) &&
            (playerPos.z >= cubePos.z - halfZ && playerPos.z <= cubePos.z + halfZ);
    }

    void OnUpdate(float dt) override {
        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;

        auto& self = registry->GetComponent<Engine::Components::Transform>(entityID);

        if (!initialized) {
            baseX = self.Position.x;
            baseY = self.Position.y;
            baseZ = self.Position.z;
            initialized = true;
        }

        // --- Mouvement oscillant ---
        if (!waiting) {
            glm::vec3 origin(baseX, baseY, baseZ);
            glm::vec3 target = origin + glm::vec3(offsetX, offsetY, offsetZ);
            glm::vec3 desired = goingForward ? target : origin;
            glm::vec3 diff = desired - self.Position;

            if (glm::length(diff) > 0.05f) {
                self.Position += glm::normalize(diff) * moveSpeed * dt;
            }
            else {
                self.Position = desired;
                waiting = true;
                timer = waitTime;
            }
        }
        else {
            timer -= dt;
            if (timer <= 0.0f) {
                waiting = false;
                goingForward = !goingForward;
            }
        }

        // --- Collision avec le Player ---
        if (playerEntity == Engine::ECS::NULL_ENTITY) {
            FindPlayer();
            return;
        }

        if (!registry->HasComponent<Engine::Components::Transform>(playerEntity)) return;

        auto& player = registry->GetComponent<Engine::Components::Transform>(playerEntity);
        auto  physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();
        if (!physicsSystem) return;

        if (CheckAABB(self.Position, player.Position)) {
            // Calcule la direction de répulsion (du cube vers le joueur)
            glm::vec3 pushDir = player.Position - self.Position;
            if (glm::length(pushDir) > 0.001f)
                pushDir = glm::normalize(pushDir);
            else
                pushDir = glm::vec3(1.0f, 0.0f, 0.0f); // fallback

            physicsSystem->AddImpulse(playerEntity, pushDir * pushForce);
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new OscillatingPlatform();
}
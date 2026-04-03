#include "script_pch.h"

#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class MovingPlatform : public Engine::Scripting::NativeScript {
public:
    float raycastDistance = 10.0f;
    float moveSpeed = 0.5f;
    float targetZ = 0.0f; 
    float baseZ = 0.0f; 
    float tolerance = 0.05f;

    bool characterDetected = false;

    Engine::ECS::Entity characterEntity = Engine::ECS::NULL_ENTITY;

    void OnInit() override {
        Inspect("Raycast Distance", &raycastDistance);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Target Z", &targetZ);
        Inspect("Tolerance", &tolerance);
    }

    void OnCreate() override {
        // Sauvegarde la position Y de départ
        if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
            auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
            baseZ = t.Position.z;
        }
        FindCharacter();
    }

    void FindCharacter() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Character") {
                characterEntity = e;
                break;
            }
        }
    }

    void OnUpdate(float dt) override {
        if (characterEntity == Engine::ECS::NULL_ENTITY) {
            FindCharacter();
            if (characterEntity == Engine::ECS::NULL_ENTITY) return;
        }

        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;

        auto& self = registry->GetComponent<Engine::Components::Transform>(entityID);
        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();

        if (!physicsSystem) return;

        // Raycast vers le haut depuis le centre de la dalle
        glm::vec3 rayStart = self.Position;
        glm::vec3 rayEnd = self.Position + glm::vec3(0.0f, raycastDistance, 0.0f);

        Engine::Systems::PhysicsUtils::RaycastDebugOptions debugOpts;
        debugOpts.enabled = true;
        debugOpts.duration = 0.0f; // une seule frame

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
            rayStart,
            rayEnd,
            entityID, // ignore la dalle elle-même
            debugOpts
        );

        // Vérifie si l'entité touchée est bien "Character"
        characterDetected = hit.hasHit && (hit.hitEntity == characterEntity);

        // Déplace la dalle vers targetY si détecté, sinon retour à baseY
        float desiredZ = characterDetected ? targetZ : baseZ;
        float diff = desiredZ - self.Position.z;

        if (std::abs(diff) > tolerance) {
            self.Position.z += glm::sign(diff) * moveSpeed * dt;
        }
        else {
            self.Position.z = desiredZ;
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new MovingPlatform();
}
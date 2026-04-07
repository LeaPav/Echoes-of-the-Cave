#include "script_pch.h"

#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class MovingPlatform : public Engine::Scripting::NativeScript {
public:
    float raycastDistance = 10.0f;
    float moveSpeed = 2.0f;
    float offsetX = 3.0f;  // Distance de déplacement sur X quand détecté
    float tolerance = 0.05f;

    float baseX = 0.0f;
    bool  initialized = false;  // Capture baseX au premier Update (pas OnCreate)
    bool  characterDetected = false;

    Engine::ECS::Entity characterEntity = Engine::ECS::NULL_ENTITY;

    void OnInit() override {
        Inspect("Raycast Distance", &raycastDistance);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Offset X", &offsetX);  // Règle ça dans l'inspecteur
        Inspect("Tolerance", &tolerance);
    }

    void OnCreate() override {
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

        // Capture la position X réelle au premier frame de jeu
        if (!initialized) {
            baseX = self.Position.x;
            initialized = true;
        }

        glm::vec3 rayStart = self.Position;
        glm::vec3 rayEnd = self.Position + glm::vec3(raycastDistance, 0.0f, 0.0f);

        Engine::Systems::PhysicsUtils::RaycastDebugOptions debugOpts;
        debugOpts.enabled = true;
        debugOpts.duration = 0.0f;

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
            rayStart, rayEnd, entityID, debugOpts
        );

        characterDetected = hit.hasHit && (hit.hitEntity == characterEntity);

        // Se déplace de offsetX depuis sa position de départ quand détecté
        float desiredX = characterDetected ? baseX + offsetX : baseX;
        float diff = desiredX - self.Position.x;

        if (std::abs(diff) > tolerance) {
            self.Position.x += glm::sign(diff) * moveSpeed * dt;
        }
        else {
            self.Position.x = desiredX;
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new MovingPlatform();
}
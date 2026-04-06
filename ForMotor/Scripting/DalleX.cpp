#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class MovingPlatform : public Engine::Scripting::NativeScript {
public:
    float detectionRange = 3.0f; 
    float moveSpeed = 2.0f;
    float offsetX = 3.0f;
    float tolerance = 0.05f;

    float baseX = 0.0f;
    bool  initialized = false;
    bool  characterDetected = false;

    Engine::ECS::Entity characterEntity = Engine::ECS::NULL_ENTITY;

    void OnInit() override {
        Inspect("Detection Range", &detectionRange);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Offset X", &offsetX);
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
        if (!registry->HasComponent<Engine::Components::Transform>(characterEntity)) return;

        auto& self = registry->GetComponent<Engine::Components::Transform>(entityID);
        auto& character = registry->GetComponent<Engine::Components::Transform>(characterEntity);

        if (!initialized) {
            baseX = self.Position.x;
            initialized = true;
        }

        float dist = glm::length(character.Position - self.Position);
        characterDetected = dist < detectionRange;

        // Log debug
        printf("CubeX: %.2f | CharX: %.2f | Dist: %.2f | Detected: %d\n",
            self.Position.x, character.Position.x, dist, (int)characterDetected);

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
#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class PickupThrow : public Engine::Scripting::NativeScript {
private:
    Engine::ECS::Entity heldEntity = Engine::ECS::NULL_ENTITY;
    Engine::ECS::Entity cameraEntity = Engine::ECS::NULL_ENTITY;
    bool isHolding = false;

public:
    float pickupRange = 3.0f;
    float holdDistance = 2.0f;
    float throwForce = 8.0f;
    float throwAngle = 0.1f;

private:
    void TryPickup(Engine::Components::Transform& camTransform, Engine::Systems::PhysicsSystem* physicsSystem) {
        if (!physicsSystem) return;

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(camTransform.Position, camTransform.Position + camTransform.Forward * pickupRange,
            entityID, { true, pickupRange, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}, {0.5f, 0.5f, 0.5f}, 0.05f, 0.012f });

        std::string hitName = registry->GetEntityName(hit.hitEntity);

        if (hit.hitEntity == Engine::ECS::NULL_ENTITY) {
            if (TerminalInstance) TerminalInstance->info("PickupThrow: Rien à ramasser");
            return;
        }

        if (hitName.find("Pickable") == std::string::npos) {
            if (TerminalInstance) TerminalInstance->info("PickupThrow: '" + hitName + "' n'est pas ramassable");
        }


        heldEntity = hit.hitEntity;
        isHolding = true;

        if (physicsSystem) {
            physicsSystem->SetLinearVelocity(heldEntity, glm::vec3(0));
        }
        if (TerminalInstance)
            TerminalInstance->info("PickupThrow: '" + hitName + "'ramassé ! ");
    }

    void DropObject(Engine::Systems::PhysicsSystem* physicsSystem) {

    }

    void ThrowObject(Engine::Components::Transform& camTransform, Engine::Systems::PhysicsSystem* physicsSystem) {

    }

public:
    void OnInit() override {
        Inspect("Pickup Range", &pickupRange);
        Inspect("Hold Range", &holdDistance);
        Inspect("Throw Range", &throwForce);
    }
    void OnCreate() override {
        FindCamera();

        if (TerminalInstance)
            TerminalInstance->info("PickupThrow: Pret.");
    }

    void FindCamera() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "MainCamera") {
                cameraEntity = e;
                break;
            }
        }
    }

    void OnUpdate(float deltaTime) override {
        if (cameraEntity == Engine::ECS::NULL_ENTITY) {
            FindCamera();
            if(cameraEntity)
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new PickupThrow();
}
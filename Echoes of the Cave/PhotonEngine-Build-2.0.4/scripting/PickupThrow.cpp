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
    float holdDistance = 0.07f;
    float throwForce = 5.0f;
    float throwAngle = 0.1f;

private:
    void TryPickup(Engine::Components::Transform& camTransform, Engine::Systems::PhysicsSystem* physicsSystem) {


        if (!physicsSystem) {
            TerminalInstance->info("DEBUG: pas de physicsSystem !");
            return;
        }

        TerminalInstance->info("DEBUG: raycast depuis " +
            std::to_string(camTransform.Position.x) + " " +
            std::to_string(camTransform.Position.y) + " " +
            std::to_string(camTransform.Position.z));
        TerminalInstance->info("DEBUG: direction Forward " +
            std::to_string(camTransform.Forward.x) + " " +
            std::to_string(camTransform.Forward.y) + " " +
            std::to_string(camTransform.Forward.z));

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
            camTransform.Position,
            camTransform.Position + camTransform.Forward * pickupRange,
            entityID,
            { true, pickupRange, {1,0,0}, {1,1,0}, {0,1,1}, {0.5f,0.5f,0.5f}, 0.05f, 0.012f }
        );

        TerminalInstance->info("DEBUG: hit entity = " +
            registry->GetEntityName(hit.hitEntity));


        std::string hitName = registry->GetEntityName(hit.hitEntity);

        if (hit.hitEntity == Engine::ECS::NULL_ENTITY) {
            if (TerminalInstance) TerminalInstance->info("PickupThrow: Rien à ramasser");
            return;
        }

        if (hitName.find("Pickable") == std::string::npos) {
            if (TerminalInstance) TerminalInstance->info("PickupThrow: '" + hitName + "' n'est pas ramassable");
            return;
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
        if (physicsSystem && heldEntity != Engine::ECS::NULL_ENTITY) {
            physicsSystem->SetLinearVelocity(heldEntity, glm::vec3(0.0f));
        }
        if (TerminalInstance) TerminalInstance->info("PickupThrow: Objet déposé");
        isHolding = false;
        heldEntity = Engine::ECS::NULL_ENTITY;
    }

    void ThrowObject(Engine::Components::Transform& camTransform, Engine::Systems::PhysicsSystem* physicsSystem) {
        if (heldEntity == Engine::ECS::NULL_ENTITY) return;
        isHolding = false;

        glm::vec3 throwDir = glm::normalize(camTransform.Forward + glm::vec3(0.0f, throwAngle, 0.0f));

        if (physicsSystem) {
            physicsSystem->SetLinearVelocity(heldEntity, throwDir * throwForce);
        }
        if (TerminalInstance) {
            TerminalInstance->info("PickupThrow: Object lancé");
        }
        heldEntity = Engine::ECS::NULL_ENTITY;
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
            if (registry->GetEntityName(e) == "Main Camera") {
                cameraEntity = e;
                break;
            }
        }
    }

    void OnUpdate(float deltaTime) override {

        if (cameraEntity == Engine::ECS::NULL_ENTITY) {
            FindCamera();
            if (cameraEntity == Engine::ECS::NULL_ENTITY) return;
        }

        auto& camTransform = registry->GetComponent<Engine::Components::Transform>(cameraEntity);
        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();

        if (InputSysteminstance->GetKeyPressed(GLFW_KEY_E)) {
            TerminalInstance->info("E pressé ! cameraEntity valide: " +
                std::to_string(cameraEntity != Engine::ECS::NULL_ENTITY));
            if (isHolding) {
                DropObject(physicsSystem);
            }
            else {
                TryPickup(camTransform, physicsSystem);
            }
        }

        if (InputSysteminstance->GetMouseButtonPressed(0) && isHolding) {
            ThrowObject(camTransform, physicsSystem);
        }

        if (isHolding && heldEntity != Engine::ECS::NULL_ENTITY) {
            if (!registry->HasComponent<Engine::Components::Transform>(heldEntity)) {
                isHolding = false;
                heldEntity = Engine::ECS::NULL_ENTITY;
                return;
            }
            glm::vec3 targetPos = camTransform.Position + camTransform.Forward * holdDistance;


            if (physicsSystem) {
                glm::vec3 currentPos = registry->GetComponent<Engine::Components::Transform>(heldEntity).Position;
                glm::vec3 diff = targetPos - currentPos;

                physicsSystem->SetLinearVelocity(heldEntity, diff * 15.0f);
            }
        }

    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new PickupThrow();
}
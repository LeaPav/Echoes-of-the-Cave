#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class DoorInteraction : public Engine::Scripting::NativeScript {
private:
    Engine::ECS::Entity cameraEntity = Engine::ECS::NULL_ENTITY;
    float interactionRange = 3.0f;

    void FindCamera() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Main Camera") {
                cameraEntity = e;
                break;
            }
        }
    }
public:
    void OnInit() override {
        Inspect("Interaction Range", &interactionRange);
    }

    void OnCreate() override {
        FindCamera();
        if (TerminalInstance)
            TerminalInstance->info("DoorInteraction : Initialize");
    }
    
    void OnUpdate(float deltaTime) override {
        if (cameraEntity == Engine::ECS::NULL_ENTITY) {
            FindCamera();
            if (cameraEntity == Engine::ECS::NULL_ENTITY) return;
        }
        if (!InputSysteminstance->GetKeyPressed(GLFW_KEY_E)) return;

        auto& camTransform = registry->GetComponent<Engine::Components::Transform>(cameraEntity);
        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();
        if (!physicsSystem) return;

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
            camTransform.Position,
            camTransform.Position + camTransform.Forward * interactionRange,
            cameraEntity, 
            { true, interactionRange, {1,0,0}, {1,1,0}, {0,1,1}, {0.5f,0.5f,0.5f}, 0.05f, 0.012f }
        );

        if (hit.hitEntity == Engine::ECS::NULL_ENTITY) return;

        std::string hitName = registry->GetEntityName(hit.hitEntity);
        if (hitName.find("Door") == std::string::npos) return;

        TerminalInstance->info("DoorInteraction: Porte '" + hitName + "' détectée !");

        auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        if (!funcSys) {
            TerminalInstance->error("DoorInteraction: FunctionRegistrySystem non trouvé");
            return;
        }

   
        std::string nextLevel = "";
        try {
            std::any result = funcSys->Call("GameManager.GetNextLevel", {});
            if (result.has_value() && result.type() == typeid(std::string)) {
                nextLevel = std::any_cast<std::string>(result);
            }
        }
        catch (...) {
            TerminalInstance->error("DoorInteraction: GameManager pas encore prêt");
            return;
        }

        if (nextLevel.empty()) {
            TerminalInstance->info("DoorInteraction: Pas de niveau suivant");
            return;
        }


        TerminalInstance->info("DoorInteraction: Chargement de " + nextLevel);

        funcSys->Call("GameManager.IncrementLevel", {});

        auto sceneSystem = engine->GetSystem<Engine::Systems::SceneSerializerSystem>();
        if (sceneSystem) {
            sceneSystem->RequestSceneDeserialization(nextLevel);
        }
        else {
            TerminalInstance->error("DoorInteraction: SceneSerializerSystem non trouvé");
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new DoorInteraction();
}
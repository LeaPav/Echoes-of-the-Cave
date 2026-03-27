/*
 * template_V3.cpp
 * A comprehensive "kitchen sink" template for the Photon Engine.
 * Updated for modern ECS Architecture.
 */

#include "script_pch.h" 
#include <json.hpp> 

// Depending on your pch, you may need to ensure these namespaces are accessible
using json = nlohmann::json;
using namespace Engine;

class TemplateV3 : public Engine::Scripting::NativeScript {
public:
    // --- Public Configurable Variables ---
    float moveSpeed = 5.0f;
    float rotationSpeed = 80.0f;
    int health = 100;
    std::string message = "Hello from TemplateV3!";

    std::string serverIP = "127.0.0.1";
    int serverPort = 7777;

private:
    // --- System Pointers ---
    // Systems are safe to cache as they persist for the engine's lifetime
    Systems::FunctionRegisterySystem* funcRegistry = nullptr;
    Systems::TerminalSystem* terminal = nullptr;
    Systems::InputSystem* inputSystem = nullptr; // Assuming this matches the new System architecture

    // --- Internal State ---
    float internalTimer = 0.0f;
    bool isLightOn = true;
    float animBlend = 0.0f;
    std::string registryName = "TemplateV3.TakeDamage"; 

public:
    // --- ========================== ---
    // --- SCRIPT LIFECYCLE METHODS   ---
    // --- ========================== ---

    void OnCreate() override {
        // --- 1. Get Core Systems ---
        // Retrieved via the new engine system manager
        terminal = engine->GetSystem<Systems::TerminalSystem>();
        funcRegistry = engine->GetSystem<Systems::FunctionRegisterySystem>();
        inputSystem = engine->GetSystem<Systems::InputSystem>();

        if (!funcRegistry && terminal) {
            terminal->error("TemplateV3: FunctionRegisterySystem not found!");
        }

        // --- 1.5. Load Configuration ---
        LoadNetworkConfig();

        // --- 2. Get Required Components ---
        // Queried through the ECS Registry instead of the Entity pointer
        if (!registry->HasComponent<Components::Transform>(entityID)) {
            if (terminal) terminal->error("TemplateV3: 'Transform' component is missing!");
            return; 
        }
        
        auto& transform = registry->GetComponent<Components::Transform>(entityID);
        transform.Position.y = 2.0f; // Note: Properties are now PascalCase

        // --- 3. Check for/Add Components ---
        if (!registry->HasComponent<Components::Light>(entityID)) {
            if (terminal) terminal->warn("TemplateV3: 'Light' component missing, adding one.");
            registry->AddComponent(entityID, Components::Light{}); 
            auto& light = registry->GetComponent<Components::Light>(entityID);
            light.Intensity = 1.5f;
            light.Type = Components::LightType::POINT;
            light.Color = glm::vec3(0.8f, 1.0f, 0.8f);
            light.Range = 15.0f; 
            light.CastShadows = true;
        } 
        
        auto& light = registry->GetComponent<Components::Light>(entityID);
        isLightOn = (light.Intensity > 0.0f);

        // --- 4. Register with Function Registry ---
        RegisterSelf();

        if (terminal) terminal->debug("TemplateV3: OnCreate() successful. Health: " + std::to_string(health));
    }

    void OnUpdate(float deltaTime) override {
        // Fetch the component fresh each frame (ECS memory can shift)
        if (!registry->HasComponent<Components::Transform>(entityID)) return;

        internalTimer += deltaTime;

        // --- 1. Handle Input ---
        HandleInput(deltaTime);

        // --- 2. Perform Logic ---
        if (registry->HasComponent<Components::Light>(entityID)) {
            auto& light = registry->GetComponent<Components::Light>(entityID);
            if (isLightOn) {
                light.Intensity = 1.0f + 0.5f * sin(internalTimer * 2.0f);
            }
        }

        // --- 3. Update Animator ---
        // if (registry->HasComponent<Components::Animator>(entityID) && inputSystem) {
        //     auto& animator = registry->GetComponent<Components::Animator>(entityID);
            
        //     float targetBlend = (inputSystem->GetKeyState(GLFW_KEY_W) || inputSystem->GetKeyState(GLFW_KEY_S)) ? 1.0f : 0.0f;
        //     animBlend = glm::lerp(animBlend, targetBlend, deltaTime * 5.0f); 
            
        //     animator.SetAnimationWeight("Idle", 1.0f - animBlend);
        //     animator.SetAnimationWeight("Walking", animBlend); 
        // }
    }

    void OnDestroy() override {
        if (funcRegistry) {
            funcRegistry->Unregister(registryName);
            if (terminal) terminal->info("TemplateV3: Unregistered function '" + registryName + "'");
        }
        if (terminal) terminal->warn("TemplateV3: OnDestroy() called.");
    }

public:
    // --- =================================== ---
    // --- PUBLIC METHODS (for Registry)       ---
    // --- =================================== ---

    void TakeDamage(int amount) {
        health -= amount;
        if (terminal) terminal->warn("TemplateV3: Ouch! Took " + std::to_string(amount) + " damage. Health is now " + std::to_string(health));
        
        if (health <= 0) {
            if (terminal) terminal->error("TemplateV3: I am dead!");
            // e.g., registry->DestroyEntity(entityID);
        }
    }

private:
    // --- ======================== ---
    // --- PRIVATE HELPER METHODS   ---
    // --- ======================== ---

    void LoadNetworkConfig() {
        const std::string filename = "Config/GameMaster_config.json";
        std::ifstream configFile(filename);
        
        if (configFile.is_open()) {
            try {
                json j;
                configFile >> j;
                if (j.contains("ip")) serverIP = j["ip"];
                if (j.contains("port")) serverPort = j["port"];
                
                if (terminal) terminal->debug("TemplateV3: Loaded config from " + filename);
            } catch (const std::exception& e) {
                if (terminal) terminal->error("TemplateV3: JSON Parse Error in " + filename + ": " + e.what());
            }
        } else {
            if (terminal) terminal->warn("TemplateV3: " + filename + " not found. Using defaults.");
        }
    }

    void HandleInput(float dt) {
        if (!inputSystem) return;

        // --- One-Shot Input (Action) ---
        if (inputSystem->GetKeyPressed(GLFW_KEY_F)) {
            if (terminal) terminal->info(message);
            
            if (funcRegistry) {
                if (terminal) terminal->info("TemplateV3: Sending '10' damage to 'DebugDrawMeshGenerator.DebugLog'");
                funcRegistry->Call("DebugDrawMeshGenerator.DebugLog", { 10 });
            }
        }
        
        if (inputSystem->GetKeyPressed(GLFW_KEY_H)) {
            if (funcRegistry) {
                if (terminal) terminal->info("TemplateV3: Sending 'StartGame event' to 'GameMaster.GM_StartGame'");
                funcRegistry->Call("GameMaster.GM_StartGame", { serverIP, serverPort });
            }
        }

        // --- Toggle Input ---
        if (inputSystem->GetKeyPressed(GLFW_KEY_L)) {
            if (registry->HasComponent<Components::Light>(entityID)) {
                auto& light = registry->GetComponent<Components::Light>(entityID);
                isLightOn = !isLightOn;
                light.Intensity = isLightOn ? 1.0f : 0.0f;
                if (terminal) terminal->info(isLightOn ? "TemplateV3: Light ON" : "TemplateV3: Light OFF");
            }
        }

        // --- Entity Creation Example ---
        if (inputSystem->GetMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE)) { 
            if (terminal) terminal->info("TemplateV3: Creating a new light entity!");
            
            // Entities are now represented by an ID generated by the registry
            ECS::Entity newLightEntity = registry->CreateEntity();
            auto& myTransform = registry->GetComponent<Components::Transform>(entityID);
            
            auto& newTransform = registry->GetOrAddComponent<Components::Transform>(newLightEntity);
            newTransform.Position = myTransform.Position + glm::vec3(0, 2, 0);
            
            auto& newLight = registry->GetOrAddComponent<Components::Light>(newLightEntity);
            newLight.Color = glm::vec3(1, 0, 0); 
            newLight.Type = Components::LightType::POINT;
            newLight.Intensity = 5.0f;
            newLight.Range = 10.0f;
        }
    }

    void RegisterSelf() {
        if (funcRegistry) {
            funcRegistry->Register(registryName, [this](std::vector<std::any> args) -> std::any {
                if (args.size() != 1) {
                    if (terminal) terminal->error(registryName + " expected 1 argument, got " + std::to_string(args.size()));
                    return {}; 
                }
                try {
                    int damage = std::any_cast<int>(args[0]);
                    this->TakeDamage(damage); 
                } catch (const std::bad_any_cast& e) {
                    if (terminal) terminal->error(registryName + " bad argument cast: " + std::string(e.what()));
                }
                return {}; 
            });

            if (terminal) terminal->info("TemplateV3: Registered function '" + registryName + "'");
        }
    }
};

extern "C" __declspec(dllexport) Engine::Scripting::NativeScript* CreateScript() {
    return new TemplateV3();
}
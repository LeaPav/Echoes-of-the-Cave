#include "script_pch.h"
#include <imgui.h>
#include "Systems/FunctionRegistrySystem.h" // Make sure to include this

#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class HUDScript : public Engine::Scripting::NativeScript {
public:
    int playerHealth = 100;
    int playerAmmo = 30;

    // 1. Move the ImGui code out of OnUpdate into its own method
    void DrawHUD() {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("Player HUD", nullptr, window_flags);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "HEALTH: %d", playerHealth);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "AMMO: %d", playerAmmo);
        ImGui::End();
    }

    // 2. Register the function when the script starts
    void OnInit() override { // Or OnInit(), depending on your base class
        TerminalInstance->info("[HUDScript]: OnInit Called.");
        Engine::Systems::FunctionRegistrySystem* funcReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        if (funcReg) {
            // Bind this specific instance's DrawHUD method to a string key.
            // (Adjust the lambda signature based on what your Register function expects)

            // funcReg->Register("DrawPlayerHUD", [this](std::vector<std::any> args) -> std::any {
            //     this->DrawHUD();
            //     return {};
            // });
        }
        if (Engine::Systems::ImGuiSystem* ImGuiSystem = engine->GetSystem<Engine::Systems::ImGuiSystem>()) {
            ImGuiSystem->RegisterUICallback("DrawPlayerHUD", [this]() {
                this->DrawHUD();
            });
        }
    }

    void OnUpdate(float dt) override {
        // Just game logic here now! No ImGui code.
    }
    
    // Optional but recommended: Unregister when the script is destroyed
    // to prevent the ImGuiSystem from calling a deleted script instance.
    void OnDestroy() override {
        TerminalInstance->info("[HUDScript]: OnDestroy Called.");
        auto funcReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        if (funcReg) {
            // funcReg->Unregister("DrawPlayerHUD"); // If your system supports this
        }
        if (Engine::Systems::ImGuiSystem* ImGuiSystem = engine->GetSystem<Engine::Systems::ImGuiSystem>()) {
            ImGuiSystem->UnregisterUICallback("DrawPlayerHUD");
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new HUDScript();
}
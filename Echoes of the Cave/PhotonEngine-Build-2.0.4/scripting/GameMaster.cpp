#include "script_pch.h"
#include <imgui.h>
#include "Systems/FunctionRegistrySystem.h" // Make sure to include this
#include "Components/HealthComponent.h" // Make sure to include this

#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class GameMaster : public Engine::Scripting::NativeScript {
private:
    Engine::Systems::AudioRegistry* m_AudioReg = nullptr; 
    Engine::Systems::FunctionRegistrySystem* m_GlobalReg = nullptr; 
public:
    void InitGameMasterAudioControl() {
        m_GlobalReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        // Handle Voice Data
        // m_GlobalReg->Register("GM_OnVoiceData", [this](std::vector<std::any> args) -> std::any {
        //     if (args.empty()) return {};
        //     auto packet = std::any_cast<std::vector<uint8_t>>(args[0]);
        //     if (IsServer()) { if (m_NetReg) m_NetReg->Call("Voice_Broadcast", { 0, packet }); }
        //     else { if (m_NetReg) m_NetReg->Call("Voice_Send", { packet }); }
        //     return {};
        // });

        // m_GlobalReg->Register("GM_OnVoiceReceived", [this](std::vector<std::any> args) -> std::any {
        //     if (args.size() < 2) return {};
        //     uint32_t senderId = std::any_cast<uint32_t>(args[0]);
        //     auto packet = std::any_cast<std::vector<uint8_t>>(args[1]);
        //     if (m_NetReg) m_NetReg->Call("Voice_Recieved", { senderId, packet });
        //     return {};
        // });

        // Handle UDP (Game State + Audio Position Sync)
        m_GlobalReg->Register("GM_OnUDPReceived", [this](std::vector<std::any> args) -> std::any {
            if (args.size() < 2) return {};
            auto packet = std::any_cast<std::vector<uint8_t>>(args[1]);

            // Protocol: [NetID(4)] [Pos(12)] ...
            if (packet.size() >= 16) {
                uint32_t netId;
                float px, py, pz;
                memcpy(&netId, packet.data(), 4);
                memcpy(&px, packet.data() + 4, 4);
                memcpy(&py, packet.data() + 8, 4);
                memcpy(&pz, packet.data() + 12, 4);

                // Route to Script
                std::string funcName = "NetRecv_" + std::to_string(netId);
                if (m_GlobalReg->HasBeenRegistered(funcName)) {
                    m_GlobalReg->Call(funcName, { packet });
                }

                // Update Audio Source
                if (m_AudioReg) {
                    m_AudioReg->SetClientPosition(netId, px, py, pz);
                }
            }
            return {};
        });

        TerminalInstance->info("GameMaster: Ready (Stereo Spatial Audio)");
    }

    void DrawHUD() {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("Game Master Debug", nullptr, window_flags);
        ImGui::TextColored({1.0f,0.5f,0.5f,1.0f}, "--- Config ---");
        std::vector<Engine::ECS::Entity> EntitiesWithHealthComponent = registry->View<HealthComponent>();

        for (Engine::ECS::Entity entity : EntitiesWithHealthComponent) {
            if (auto* health = &registry->GetComponent<HealthComponent>(entity)) {
                std::string text = "Player Health (" + registry->GetEntityName(entity) + ")";
                ImGui::InputFloat(text.c_str(), &health->currentHealth);
            }
        }

        ImGui::End();
    }

    // Register the functions when the script starts
    void OnInit() override {
        TerminalInstance->info("[GameMaster]: OnInit Called.");
        Engine::Systems::FunctionRegistrySystem* funcReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        Engine::Systems::AudioSystem* audioSystem = engine->GetSystem<Engine::Systems::AudioSystem>();
        if (audioSystem) m_AudioReg = audioSystem->ClaimGameMasterAccess(this);
        if (Engine::Systems::ImGuiSystem* ImGuiSystem = engine->GetSystem<Engine::Systems::ImGuiSystem>()) {
            ImGuiSystem->RegisterUICallback("DrawGameMasterHUD", [this]() {
                this->DrawHUD();
            });
        }
    }

    void OnUpdate(float dt) override {
        // Just game logic here now! No ImGui code.
    }
    
    void OnDestroy() override {
        TerminalInstance->info("[GameMaster]: OnDestroy Called.");
        auto funcReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
        if (Engine::Systems::ImGuiSystem* ImGuiSystem = engine->GetSystem<Engine::Systems::ImGuiSystem>()) {
            ImGuiSystem->UnregisterUICallback("DrawGameMasterHUD");
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new GameMaster();
}
#include "Scripting/DynamicSystem.h"
#include "Core/ISystem.h"
#include <iostream>

namespace Engine::Systems {

    class MyCustomNetworkingSystem : public Core::ISystem {
    public:
        void OnInit() override {
            std::cout << "Custom Networking System Initialized!" << std::endl;
        }

        void OnPlayUpdate(float deltaTime) override {
            // Process networking logic here
        }

        void OnFixedUpdate(float deltaTime) override {

        }
        
        void OnShutdown() override {
            std::cout << "Custom Networking System Shutting down!" << std::endl;
        }

        

        
        EDITOR_METHOD(
            void OnEditorUpdate(float deltaTime) override  {
            }
        )

        EDITOR_METHOD(
            void OnEditorGUI() override {
            }
        )
    };
}

// Export the system factory for the Engine to find
EXPORT_SYSTEM Engine::Core::ISystem* CreateSystem() {
    return new Engine::Systems::MyCustomNetworkingSystem();
}
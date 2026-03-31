#include "script_pch.h" 

// Platform-agnostic export macro
#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class CustomNetworkingScript : public Engine::Scripting::NativeScript {
public:
    void OnInit() override {
        
    }

    void OnCreate() override {
        Engine::Core::ISystem* netSystem = engine->GetDynamicSystem("MyCustomNetworkingSystem");
        if (netSystem) {
            TerminalInstance->info("Successfully got the MyCustomNetworkingSystem pointer !!!");
        }
    }

    void OnUpdate(float dt) override {
        
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new CustomNetworkingScript();
}
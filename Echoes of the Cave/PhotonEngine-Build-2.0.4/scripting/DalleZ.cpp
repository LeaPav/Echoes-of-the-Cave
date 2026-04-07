#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class OscillatingPlatform : public Engine::Scripting::NativeScript {
public:
    float moveSpeed = 2.0f;
    float offsetX = 0.0f;  // Distance de déplacement (peut être négatif)
    float offsetY = 0.0f;
    float offsetZ = 3.0f;
    float waitTime = 1.0f;  // Pause en secondes à chaque extrémité

    float baseX = 0.0f, baseY = 0.0f, baseZ = 0.0f;
    float timer = 0.0f;
    bool  initialized = false;
    bool  goingForward = true;  // Direction actuelle
    bool  waiting = false;

    void OnInit() override {
        Inspect("Move Speed", &moveSpeed);
        Inspect("Offset X", &offsetX);
        Inspect("Offset Y", &offsetY);
        Inspect("Offset Z", &offsetZ);
        Inspect("Wait Time", &waitTime);
    }

    void OnCreate() override {}

    void OnUpdate(float dt) override {
        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;

        auto& self = registry->GetComponent<Engine::Components::Transform>(entityID);

        if (!initialized) {
            baseX = self.Position.x;
            baseY = self.Position.y;
            baseZ = self.Position.z;
            initialized = true;
        }

        // Gestion de la pause aux extrémités
        if (waiting) {
            timer -= dt;
            if (timer <= 0.0f) {
                waiting = false;
                goingForward = !goingForward;
            }
            return;
        }

        glm::vec3 origin(baseX, baseY, baseZ);
        glm::vec3 target = origin + glm::vec3(offsetX, offsetY, offsetZ);

        glm::vec3 desired = goingForward ? target : origin;
        glm::vec3 diff = desired - self.Position;

        if (glm::length(diff) > 0.05f) {
            self.Position += glm::normalize(diff) * moveSpeed * dt;
        }
        else {
            self.Position = desired;
            // Arrive à l'extrémité : pause puis demi-tour
            waiting = true;
            timer = waitTime;
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new OscillatingPlatform();
}
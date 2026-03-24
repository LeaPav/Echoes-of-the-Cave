#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class FPSCamera : public Engine::Scripting::NativeScript {
public:
    float sensitivity = 0.1f;
    float moveSpeed = 1.0f;
    float headHeight = 1.7f;

    float yaw = 0.0f;
    float pitch = 0.0f;

    bool invertX = false;
    bool invertY = false;

    Engine::ECS::Entity targetEntity = Engine::ECS::NULL_ENTITY;
    bool isMouseCaptured = false;

    void OnInit() override {
        Inspect("Sensitivity", &sensitivity);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Head Height", &headHeight);
        Inspect("Invert X", &invertX);
        Inspect("Invert Y", &invertY);
    }

    void OnCreate() override {
        FindTarget();

        auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegisterySystem>();
        if (funcSys) {
            funcSys->Register("OnStep", [this](std::vector<std::any> args) -> std::any {
                return {};
                });
        }
    }

    void FindTarget() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Player") {
                targetEntity = e;
                break;
            }
        }
    }

    void OnUpdate(float dt) override {

        if (InputSysteminstance->GetMouseButtonPressed(1)) {
            isMouseCaptured = !isMouseCaptured;
            InputSysteminstance->SetMouseCapture(isMouseCaptured);
        }

        if (targetEntity == Engine::ECS::NULL_ENTITY) {
            FindTarget();
            if (targetEntity == Engine::ECS::NULL_ENTITY) return;
        }

        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;
        if (!registry->HasComponent<Engine::Components::Transform>(targetEntity)) return;

        auto& cam = registry->GetComponent<Engine::Components::Transform>(entityID);
        auto& player = registry->GetComponent<Engine::Components::Transform>(targetEntity);
        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();

        if (isMouseCaptured) {
            glm::vec2 look = InputSysteminstance->lookInput;

            if (invertX) yaw += look.x * sensitivity;
            else         yaw -= look.x * sensitivity;

            if (invertY) pitch -= look.y * sensitivity;
            else         pitch += look.y * sensitivity;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }
        cam.Rotation.x = pitch;
        cam.Rotation.y = yaw;
        cam.Rotation.z = 0.0f;

        glm::mat4 rotationMatrix = glm::mat4(1.0f);
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cam.Rotation.y), glm::vec3(0, 1, 0));
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cam.Rotation.x), glm::vec3(1, 0, 0));
        cam.Forward = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

        glm::vec3 flatForward = glm::normalize(glm::vec3(cam.Forward.x, 0.0f, cam.Forward.z));
        glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 inputDirection(0.0f);

        if (isMouseCaptured) {
            if (InputSysteminstance->GetKeyState(GLFW_KEY_W)) inputDirection += flatForward;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_S)) inputDirection -= flatForward;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_D)) inputDirection += flatRight;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_A)) inputDirection -= flatRight;
        }

        if (glm::length(inputDirection) > 0.0f)
            inputDirection = glm::normalize(inputDirection) * moveSpeed;

        if (physicsSystem) {
            glm::vec3 currentVel = physicsSystem->GetLinearVelocity(targetEntity);
            physicsSystem->SetLinearVelocity(targetEntity,
                glm::vec3(inputDirection.x, currentVel.y, inputDirection.z));

            // Saut
            if (InputSysteminstance->GetKeyPressed(GLFW_KEY_SPACE)) {
                auto& t = registry->GetComponent<Engine::Components::Transform>(targetEntity);
                physicsSystem->AddImpulse(targetEntity, t.Up * 2.0f);
            }
        }

        player.Rotation.y = yaw + 180.0f;

        cam.Position = player.Position + glm::vec3(0.f, 0.07f, -0.05f);
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new FPSCamera();
}
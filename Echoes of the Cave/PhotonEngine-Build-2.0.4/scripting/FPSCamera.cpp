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
    float distance = 0.1f;
    float targetHeightOffset = 0.1f;

    float yaw = 0.0f;
    float pitch = 20.0f;

    bool invertX = false;
    bool invertY = false;

    bool canJump = true;
    bool isJumping = false;
    float jumpCooldownTimer = 0.0f;
    float jumpCooldown = 0.3f;

    bool isSprinting = false;
    float sprintMultiplier = 2.0f;

    Engine::ECS::Entity targetEntity = Engine::ECS::NULL_ENTITY;
    bool isMouseCaptured = false;

    bool isButMousePressed = false;
    bool entitietouch = false;

    //////climb
    bool climbable = false;
    Engine::ECS::Entity climbableEntity = Engine::ECS::NULL_ENTITY;

    void OnInit() override {
        Inspect("Sensitivity", &sensitivity);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Head Height", &distance);
        Inspect("Invert X", &invertX);
        Inspect("Invert Y", &invertY);
    }

    void OnCreate() override {
        FindTarget();

        /* auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegisterySystem>();
         if (funcSys) {
             funcSys->Register("OnStep", [this](std::vector<std::any> args) -> std::any {
                 return {};
                 });
         }*/
    }

    void FindTarget() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Player") {
                targetEntity = e;
                break;
            }
        }
    }

    //auto& cam = registry->GetComponent<Engine::Components::Transform>(entityID);

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
            if (InputSysteminstance->GetKeyState(GLFW_KEY_W)) player.Position += flatForward * moveSpeed * dt;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_S)) player.Position -= flatForward * moveSpeed * dt;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_D)) player.Position += flatRight * moveSpeed * dt;
            if (InputSysteminstance->GetKeyState(GLFW_KEY_A)) player.Position -= flatRight * moveSpeed * dt;
        }

        if (glm::length(inputDirection) > 0.0f)
            inputDirection = glm::normalize(inputDirection) * moveSpeed;

        if (isMouseCaptured) {
            isSprinting = InputSysteminstance->GetKeyState(GLFW_KEY_LEFT_SHIFT);
        }

        float currentSpeed = moveSpeed;
        if (isSprinting && glm::length(inputDirection) > 0.0f) {
            currentSpeed = moveSpeed * sprintMultiplier;
        }

        if (glm::length(inputDirection) > 0.0f) {
            inputDirection = glm::normalize(inputDirection) * currentSpeed;
        }

        player.Rotation.y = yaw + 180.0f;

        //cam.Position = player.Position + glm::vec3(0.f, 0.07f, -0.05f);

        glm::vec3 orbitPos = player.Position + glm::vec3(0.0f, targetHeightOffset, 0.0f);
        cam.Position = orbitPos + (cam.Forward * distance);

        if (physicsSystem) {
            glm::vec3 currentVel = physicsSystem->GetLinearVelocity(targetEntity);
            auto& t = registry->GetComponent<Engine::Components::Transform>(targetEntity);

            if (jumpCooldownTimer > 0.0f) {
                jumpCooldownTimer -= dt;
            }

            physicsSystem->SetLinearVelocity(targetEntity,
                glm::vec3(inputDirection.x, currentVel.y, inputDirection.z));

            bool isGrounded = (jumpCooldownTimer <= 0.0f) && (currentVel.y > -0.5f && currentVel.y <= 0.1f);

            if (isGrounded && isJumping) {
                canJump = true;
                isJumping = false;
            }

            // Saut
            if (InputSysteminstance->GetKeyPressed(GLFW_KEY_SPACE) && canJump) {
                physicsSystem->AddImpulse(targetEntity, t.Up * 2.0f);
                canJump = false;
                isJumping = true;
                jumpCooldownTimer = jumpCooldown;

            }

            if (InputSysteminstance->GetKeyPressed(GLFW_KEY_B)) {

                glm::vec3 rayOrigin = cam.Position;
                glm::vec3 rayEnd = rayOrigin + cam.Forward * 0.1f;

                Engine::Systems::PhysicsUtils::RaycastHit hitResult = physicsSystem->Raycast(
                    rayOrigin,           // Départ : caméra
                    rayEnd,              // Fin : direction du regard
                    targetEntity,
                    { true, 0.1f, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}, {0.5, 0.5, 0.5}, 0.05f, 0.012f }
                );

                std::string hitName = registry->GetEntityName(hitResult.hitEntity);

                if (hitName.find("Climbable") == std::string::npos) {
                    TerminalInstance->info("FPSCamera: '" + hitName + "' n'est pas climbable");
                    climbable = false;
                    return;
                }
                else
                {
                    TerminalInstance->info("FPSCamera: '" + hitName + "' est climbable");
                    climbable = true;
                    
                    climbableEntity = hitResult.hitEntity;

                    auto& objClimb = registry->GetComponent<Engine::Components::Transform>(climbableEntity);

                    physicsSystem->AddImpulse(targetEntity, glm::vec3(0.0f, objClimb.Scale.y, 0.0f) * 14.0f);
                    TerminalInstance->info("FPSCamera: impulse");
                    
                    return;
                }
                
                
            }
            
        }
        
    }

};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new FPSCamera();
}
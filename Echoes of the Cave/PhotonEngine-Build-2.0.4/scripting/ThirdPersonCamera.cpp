#include "script_pch.h"
#include "Systems/AnimatorSystem.h" // Include AnimatorSystem to control weights

#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class ThirdPersonCamera : public Engine::Scripting::NativeScript {
public:
    float distance = 0.2f;
    float sensitivity = 0.1f;
    float moveSpeed = 1.0f; // Speed of the character
    float animationBlendSpeed = 10.0f; // How fast animations transition

    float yaw = 0.0f; 
    float pitch = 20.0f;
    float targetHeightOffset = 0.1f; 
    
    bool invertX = false;
    bool invertY = false;
    
    Engine::ECS::Entity targetEntity = Engine::ECS::NULL_ENTITY;
    bool isMouseCaptured = false;
    bool animationsInitialized = false;

    // --- Animation Paths (Replace these with your actual asset paths!) ---
    std::string idleAnim = "Assets\\Mixamo\\Idle_anim_mixamorig_Hips.pa";
    std::string forwardAnim = "Assets\\Mixamo\\Jogging_anim_mixamo_Hips.pa";
    std::string backwardAnim = "Assets\\Mixamo\\Jog Backward_anim_mixamorig_Hips.pa";

    // 1.0 = full forward, 0.0 = idle, -1.0 = full backward
    float currentMoveState = 0.0f; 

    void OnInit() override {
        Inspect("Distance", &distance);
        Inspect("Sensitivity", &sensitivity);
        Inspect("Move Speed", &moveSpeed);
        Inspect("Height Offset", &targetHeightOffset);
        Inspect("Invert X", &invertX);
        Inspect("Invert Y", &invertY);
    }

    void OnCreate() override {
        FindTarget();
    }

    void FindTarget() {
        for (auto e : registry->View<Engine::Components::Transform>()) {
            if (registry->GetEntityName(e) == "Player") {
                targetEntity = e;
                break;
            }
        }
    }

    void InitAnimations() {
        auto animSys = engine->GetSystem<Engine::Systems::AnimatorSystem>(); //
        if (animSys && registry->HasComponent<Engine::Components::Animator>(targetEntity)) {
            // Play all animations simultaneously (looping = true)
            animSys->PlayBlendAnimation(targetEntity, idleAnim, true);
            animSys->PlayBlendAnimation(targetEntity, forwardAnim, true);
            animSys->PlayBlendAnimation(targetEntity, backwardAnim, true);
            
            animationsInitialized = true;
        }
    }

    void OnUpdate(float dt) override {
        if (targetEntity == Engine::ECS::NULL_ENTITY) {
            FindTarget();
            if (targetEntity == Engine::ECS::NULL_ENTITY) return; 
        }

        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();

        // Initialize animations once the target is found
        if (!animationsInitialized) {
            InitAnimations();
        }

        // JUMP LOGIC
        if (InputSysteminstance->GetKeyPressed(GLFW_KEY_SPACE) && physicsSystem) {
            auto& targetTransform = registry->GetComponent<Engine::Components::Transform>(targetEntity);

            // Ignore the targetEntity (character) so the raycast doesn't hit its own capsule
            Engine::Systems::PhysicsUtils::RaycastHit hitResult = physicsSystem->Raycast(
                targetTransform.Position,
                targetTransform.Position - glm::vec3(0.0, 0.05, 0.0),
                targetEntity,
                { true, 0.1f, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}, {0.5, 0.5, 0.5}, 0.05f, 0.012f }
            );

            std::string HitEntityName = registry->GetEntityName(hitResult.hitEntity);
            TerminalInstance->print("RayCast Hit " + HitEntityName);

            if (HitEntityName == "Plane") {
                // Apply an upward impulse to the character body
                physicsSystem->AddImpulse(targetEntity, targetTransform.Up * 2.0f);
            }
        }

        if (!isMouseCaptured && Engine::Core::UIState::IsMouseCaptured()) {
            return;
        }

        // 1. Mouse Look (Orbit Camera)
        if (InputSysteminstance->GetMouseButtonState(1)) { 
            if (!isMouseCaptured) {
                InputSysteminstance->SetMouseCapture(true);
                isMouseCaptured = true;
            }

            glm::vec2 look = InputSysteminstance->lookInput;
            
            if (invertX) yaw += look.x * sensitivity;
            else         yaw -= look.x * sensitivity;

            if (invertY) pitch -= look.y * sensitivity;
            else         pitch += look.y * sensitivity;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        } else {
            if (isMouseCaptured) {
                InputSysteminstance->SetMouseCapture(false);
                isMouseCaptured = false;
            }
        }

        if (registry->HasComponent<Engine::Components::Transform>(entityID) && 
            registry->HasComponent<Engine::Components::Transform>(targetEntity)) {
            
            auto& cameraTransform = registry->GetComponent<Engine::Components::Transform>(entityID);
            auto& targetTransform = registry->GetComponent<Engine::Components::Transform>(targetEntity);

            // 2. Camera Rotation
            cameraTransform.Rotation.x = pitch;
            cameraTransform.Rotation.y = yaw; 
            cameraTransform.Rotation.z = 0.0f; 

            glm::mat4 rotationMatrix = glm::mat4(1.0f);
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cameraTransform.Rotation.y), glm::vec3(0, 1, 0)); 
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(cameraTransform.Rotation.x), glm::vec3(1, 0, 0)); 
            
            cameraTransform.Forward = glm::normalize(glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));

            // 3. Character Movement & Animation Logic
            float targetMoveState = 0.0f; // Target blending value based on pure input
            
            // Flatten camera forward vector to move character along the XZ plane
            glm::vec3 flatForward = glm::normalize(glm::vec3(cameraTransform.Forward.x, 0.0f, cameraTransform.Forward.z));
            glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (isMouseCaptured) { // Optional: only move character if camera is captured
                if (InputSysteminstance->GetKeyState(GLFW_KEY_W)) { //
                    targetTransform.Position += flatForward * moveSpeed * dt;
                    targetMoveState = 1.0f; // Walk forward
                    
                    // Optional: Rotate character model to face the forward direction
                    targetTransform.Rotation.y = yaw + 180.0f;
                }
                if (InputSysteminstance->GetKeyState(GLFW_KEY_S)) {
                    targetTransform.Position -= flatForward * moveSpeed * dt;
                    targetMoveState = -1.0f; // Walk backward
                    
                    targetTransform.Rotation.y = yaw + 180.0f;
                }
                if (InputSysteminstance->GetKeyState(GLFW_KEY_D)) {
                    targetTransform.Position += flatRight * moveSpeed * dt;
                    targetMoveState = 1.0f;
                }
                if (InputSysteminstance->GetKeyState(GLFW_KEY_A)) {
                    targetTransform.Position -= flatRight * moveSpeed * dt;
                    targetMoveState = -1.0f;
                }
            }

            // Smoothly interpolate the current move state toward the target input to create smooth blending
            currentMoveState += (targetMoveState - currentMoveState) * animationBlendSpeed * dt;

            // 4. Calculate and Apply Animation Weights
            if (auto animSys = engine->GetSystem<Engine::Systems::AnimatorSystem>()) { //
                // Split the current state into specific weights (must be >= 0)
                float forwardWeight = std::max(0.0f, currentMoveState);
                float backwardWeight = std::max(0.0f, -currentMoveState);
                
                // Idle takes whatever weight is left over
                float idleWeight = 1.0f - std::abs(currentMoveState);

                // Apply weights to the Animator System
                animSys->SetAnimationWeight(targetEntity, idleAnim, idleWeight);
                animSys->SetAnimationWeight(targetEntity, forwardAnim, forwardWeight);
                animSys->SetAnimationWeight(targetEntity, backwardAnim, backwardWeight);
            }

            // 5. Update Camera Position based on the newly moved target
            glm::vec3 targetFocusPos = targetTransform.Position + glm::vec3(0.0f, targetHeightOffset, 0.0f);
            cameraTransform.Position = targetFocusPos - (cameraTransform.Forward * distance);
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new ThirdPersonCamera();
}
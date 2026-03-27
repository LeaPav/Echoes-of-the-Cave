#include "script_pch.h" 

// Platform-agnostic export macro
#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class EditorCamera : public Engine::Scripting::NativeScript {
public:
    float speed = 0.5f;
    float sensitivity = 0.1f;
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool isMouseCaptured = false;
    
    // --- NEW: Track the click state to debounce the input ---
    bool wasRightClickPressed = false; 

    void OnInit() override {
        // Register variables to the Inspector
        Inspect("Move Speed", &speed);
        Inspect("Sensitivity", &sensitivity);
    }

    void OnCreate() override {
        // Initialize rotation
        if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
            auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
            yaw = t.Rotation.y;
            pitch = t.Rotation.x;
        }
    }

    void OnUpdate(float dt) override {
        // If the UI owns the mouse, ensure we reset the click tracker and bail out.
        if (!isMouseCaptured && Engine::Core::UIState::IsMouseCaptured()) {
            wasRightClickPressed = false;
            return;
        }
        
        // 1. Mouse Look (Right Click to Move)
        if (InputSysteminstance->GetMouseButtonState(1)) { // Right Mouse Button
            
            // --- NEW: 1-Frame Debounce ---
            // Wait 1 frame to ensure ImGui has updated its hover state for this frame.
            if (!wasRightClickPressed) {
                wasRightClickPressed = true;
                return; 
            }

            if (!isMouseCaptured && !Engine::Core::UIState::IsMouseCaptured()) {
                InputSysteminstance->SetMouseCapture(true);
                isMouseCaptured = true;
            }

            glm::vec2 look = InputSysteminstance->lookInput;
            yaw += look.x * sensitivity;
            pitch += look.y * sensitivity;

            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
                auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
                t.Rotation.x = pitch;
                t.Rotation.y = -yaw;
                
                // Recalculate Forward/Right/Up
                glm::vec3 front;
                front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                front.y = sin(glm::radians(pitch));
                front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                t.Forward = glm::normalize(front);
            }
        } else {
            // Reset state when releasing the right mouse button
            wasRightClickPressed = false;
            
            if (isMouseCaptured) {
                InputSysteminstance->SetMouseCapture(false);
                isMouseCaptured = false;
            }
        }

        // 2. Keyboard Movement (Only when right mouse held)
        if (isMouseCaptured) {
            if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
                auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
                float velocity = speed * dt;
                
                // Boost speed with Shift
                if (InputSysteminstance->GetKeyState(GLFW_KEY_LEFT_SHIFT)) {
                    velocity *= 3.0f;
                }

                if (InputSysteminstance->GetKeyState(GLFW_KEY_W))
                    t.Position -= t.Right() * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_S))
                    t.Position += t.Right() * velocity;
                
                glm::vec3 forward = glm::normalize(glm::cross(t.Right(), glm::vec3(0.0f, 1.0f, 0.0f)));
                if (InputSysteminstance->GetKeyState(GLFW_KEY_A))
                    t.Position += forward * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_D))
                    t.Position -= forward * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_SPACE))
                    t.Position += glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_LEFT_CONTROL))
                    t.Position -= glm::vec3(0.0f, 1.0f, 0.0f) * velocity;
            }
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new EditorCamera();
}
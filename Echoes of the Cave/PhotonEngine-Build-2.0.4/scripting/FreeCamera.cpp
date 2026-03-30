#include "script_pch.h" 

// Platform-agnostic export macro
#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class FreeCamera : public Engine::Scripting::NativeScript {
public:
    float speed = 0.5f;
    float sensitivity = 0.1f;
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool isMouseCaptured = false;
    bool wasRightClickPressed = false; 

    void OnInit() override {
        Inspect("Move Speed", &speed);
        Inspect("Sensitivity", &sensitivity);
    }

    void OnCreate() override {
        if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
            auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
            yaw = t.Rotation.y;
            pitch = t.Rotation.x;
        }
    }

    void OnUpdate(float dt) override {
        bool is2D = Engine::Core::UIState::GetIs2DMode();

        if (!isMouseCaptured && Engine::Core::UIState::IsMouseCaptured()) {
            wasRightClickPressed = false;
            return;
        }
        
        // 1. Handle Right Click Capture
        if (InputSysteminstance->GetMouseButtonState(1)) { 
            if (!wasRightClickPressed) {
                wasRightClickPressed = true;
                return; 
            }

            if (!isMouseCaptured && !Engine::Core::UIState::IsMouseCaptured()) {
                InputSysteminstance->SetMouseCapture(true);
                isMouseCaptured = true;
            }

            // 2. Specialized Movement Logic
            if (registry->HasComponent<Engine::Components::Transform>(entityID)) {
                auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
                glm::vec2 mouseDelta = InputSysteminstance->lookInput;

                if (is2D) {
                    // --- 2D MODE: Hold and Drag to Pan ---
                    // We move opposite to the mouse direction to simulate "dragging" the world
                    float panSpeed = sensitivity * 0.1f; 
                    t.Position.x -= mouseDelta.x * panSpeed;
                    t.Position.y -= mouseDelta.y * panSpeed;
                } 
                else {
                    // --- 3D MODE: Mouse Look ---
                    yaw += mouseDelta.x * sensitivity;
                    pitch += mouseDelta.y * sensitivity;

                    if (pitch > 89.0f) pitch = 89.0f;
                    if (pitch < -89.0f) pitch = -89.0f;

                    t.Rotation.x = pitch;
                    t.Rotation.y = -yaw;
                    
                    glm::vec3 front;
                    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
                    front.y = sin(glm::radians(pitch));
                    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
                    t.Forward = glm::normalize(front);
                }
            }
        } else {
            wasRightClickPressed = false;
            if (isMouseCaptured) {
                InputSysteminstance->SetMouseCapture(false);
                isMouseCaptured = false;
            }
        }

        // 3. Keyboard Movement (Still useful for 2D panning or 3D flying)
        if (isMouseCaptured && registry->HasComponent<Engine::Components::Transform>(entityID)) {
            auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
            float velocity = speed * dt;
            
            if (InputSysteminstance->GetKeyState(GLFW_KEY_LEFT_SHIFT)) velocity *= 3.0f;

            if (is2D) {
                // 2D Keyboard Panning
                if (InputSysteminstance->GetKeyState(GLFW_KEY_W)) t.Position.y += velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_S)) t.Position.y -= velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_A)) t.Position.x -= velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_D)) t.Position.x += velocity;
            } else {
                // 3D Flying
                if (InputSysteminstance->GetKeyState(GLFW_KEY_W)) t.Position -= t.Right() * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_S)) t.Position += t.Right() * velocity;
                
                glm::vec3 forward = glm::normalize(glm::cross(t.Right(), glm::vec3(0.0f, 1.0f, 0.0f)));
                if (InputSysteminstance->GetKeyState(GLFW_KEY_A)) t.Position += forward * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_D)) t.Position -= forward * velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_E)) t.Position.y += velocity;
                if (InputSysteminstance->GetKeyState(GLFW_KEY_Q)) t.Position.y -= velocity;
            }
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new FreeCamera();
}
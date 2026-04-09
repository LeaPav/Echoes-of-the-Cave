#include "script_pch.h" 
#include "Belt.h" 

// Platform-agnostic export macro
#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class BeltGeometryGenerator : public Engine::Scripting::NativeScript {
private:
    Engine::Systems::FunctionRegistrySystem* m_GlobalReg = nullptr;
    Engine::Systems::RenderSystem* m_RenderSystem = nullptr;
    Engine::Components::MeshRenderer* m_MeshRenderer = nullptr;
    Engine::Components::RigidBody* m_RigidBody = nullptr;

    const char* NORMAL_VIS_VERT_SRC = R"(
        #version 330 core
        layout (location = 0) in vec3 inPos;
        layout (location = 1) in vec3 inNormal;
        layout (location = 2) in vec2 inUV; 

        uniform mat4 uModel;
        uniform mat4 uView;
        uniform mat4 uProjection;

        out vec3 fragPosWorld;
        out vec3 fragNormalWorld;
        out vec2 fragUV; // <--- ADDED THIS

        void main() {
            fragPosWorld = (uModel * vec4(inPos, 1.0)).xyz;
            fragNormalWorld = mat3(transpose(inverse(uModel))) * inNormal;
            fragUV = inUV;   // <--- ADDED THIS
            gl_Position = uProjection * uView * uModel * vec4(inPos, 1.0);
        }
    )";

    const char* NORMAL_VIS_FRAG_SRC = R"(
        #version 330 core
        out vec4 FragColor;

        in vec3 fragPosWorld;
        in vec3 fragNormalWorld;
        in vec2 fragUV;
        
        // Lighting
        uniform vec3 uViewPos;
        uniform vec3 uLightPos;
        uniform vec3 uLightColor;
        uniform float uLightIntensity;
        
        // Movement
        uniform vec2 uUVOffset;     
        
        // --- MATERIAL UNIFORMS ---
        uniform sampler2D uAlbedoTexture;
        uniform int uUseAlbedoMap;
        uniform vec4 uAlbedoFactor;
        
        void main() {
            // 1. Apply the conveyor belt movement
            vec2 movingUV = fragUV + uUVOffset;
            
            // 2. Resolve Material Base Color
            vec4 baseColor = uAlbedoFactor;
            if (uUseAlbedoMap == 1) {
                // Sample using our scrolling UVs!
                baseColor *= texture(uAlbedoTexture, movingUV); 
            }
            
            // 3. Basic Blinn-Phong Lighting
            vec3 normal = normalize(fragNormalWorld);
            vec3 lightDir = normalize(uLightPos - fragPosWorld);
            float diff = max(dot(normal, lightDir), 0.0);
            
            vec3 ambient = 0.2 * baseColor.rgb;
            vec3 diffuse = diff * baseColor.rgb * uLightColor * uLightIntensity;
            
            // 4. Output (FIXED: actually uses the calculated lighting)
            FragColor = vec4(ambient + diffuse, 1.0); 
        }
    )";

    float RunningTime = 0.0f;
public:
    void OnCreate() override {
        m_MeshRenderer = &registry->GetOrAddComponent<Engine::Components::MeshRenderer>(entityID);
        m_RigidBody = &registry->GetOrAddComponent<Engine::Components::RigidBody>(entityID);

        // --- Load and Assign Custom Shader ---
        m_RenderSystem = engine->GetSystem<Engine::Systems::RenderSystem>(); 
        const std::string shaderName = "normal_visualizer_transparent";

        if (m_RenderSystem) {
            if (!m_RenderSystem->HasShader(shaderName)) { 
                TerminalInstance->info("Loading custom shader: " + shaderName);
                m_RenderSystem->AddShader(shaderName, NORMAL_VIS_VERT_SRC, NORMAL_VIS_FRAG_SRC);
            }
            
            // --- START MODIFICATION ---
            // 1. Now we can call AddComponent WITH an argument!
            Engine::Components::MaterialOverride* material = &registry->GetOrAddComponent<Engine::Components::MaterialOverride>(entityID, shaderName);

            auto beltMaterial = std::make_shared<Engine::Graphics::Material>();
            beltMaterial->albedoColor = glm::vec4(0.0, 0.0, 1.0f, 1.0f); // Blue base
            
            // uniform vec3 uLightPos;
            // uniform vec3 uLightColor;
            // uniform float uLightIntensity;
            material->Uniforms["uLightPos"] = std::array<float, 3>{1.0f, 1.0f, 1.0f};
            material->Uniforms["uLightColor"] = std::array<float, 3>{1.0f, 1.0f, 1.0f};
            material->Uniforms["uLightIntensity"] = 1.0f;

            material->Uniforms["uAlbedoFactor"] = std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f};
            material->Uniforms["uUseAlbedoMap"] = 1;
            material->Uniforms["beltMaterial"] = beltMaterial;


            // 2. Add the Transparent component back. This is required.
            registry->GetOrAddComponent<Engine::Components::Transparent>(entityID);
            // --- END MODIFICATION ---
            
        } else {
            TerminalInstance->error("Could not get RenderSystem to load custom shader.");
        }

        m_GlobalReg = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();

        m_GlobalReg->Register("BeltGeometryGenerator.GenerateBelt", [this](std::vector<std::any> args) -> std::any {
            // if (args.size() < 5) return {};
            glm::vec3 p1 = std::any_cast<glm::vec3>(args[0]);
            glm::vec3 p2 = std::any_cast<glm::vec3>(args[1]);
            glm::vec3 dir1 = std::any_cast<glm::vec3>(args[2]);
            glm::vec3 dir2 = std::any_cast<glm::vec3>(args[3]);


            Belt _Belt = Belt(p1, p2);
            _Belt.belt_width = 0.25f;
            _Belt.generatePath(0.05f, dir1, dir2);
            _Belt.generateGeometryFromTemplateSlice(_Belt.getSliceTemplate(), 0.1f);
            // _Belt.generateGeometry();
            return {_Belt};
        });
    }

    void OnInit() override {
        std::any result = m_GlobalReg->Call("BeltGeometryGenerator.GenerateBelt", {glm::vec3(0.5, 0.0, 0.0), glm::vec3(-0.5, 0.5, 0.0), glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, 0.0, -1.0)});
        if (result.has_value() && result.type() == typeid(Belt)) {
            Belt _Belt = std::any_cast<Belt>(result);
            if (m_MeshRenderer) {
                // _Belt.getVertexData()
                // m_MeshRenderer->meshes
                auto mesh = std::make_shared<Engine::Graphics::Mesh>();
                mesh->vertex_data = _Belt.getVertexData();
                mesh->indices = _Belt.getIndices();
                
                mesh->upload();
        
                m_MeshRenderer->active = true;
                m_MeshRenderer->meshes.push_back(std::move(mesh));

                m_RigidBody->shapeType = Engine::Components::CollisionShapeType::MESH_CONCAVE;
                m_RigidBody->dirty = true;
            }
            
        }
    }

    void OnUpdate(float dt) override {
        RunningTime += dt;
        if (Engine::Components::MeshRenderer* _MeshRenderer = &registry->GetComponent<Engine::Components::MeshRenderer>(entityID)) {
            if (!_MeshRenderer->materials.empty()) {
                if (Engine::Components::MaterialOverride* material = &registry->GetComponent<Engine::Components::MaterialOverride>(entityID)) {
                    material->Uniforms["beltMaterial"] = _MeshRenderer->materials.at(0);
                    material->Uniforms["uUVOffset"] = std::array<float, 2>{1.0f, sinf(RunningTime)};
                }
            }
        }
    }
    
    void OnDestroy() override {
        m_GlobalReg->Unregister("BeltGeometryGenerator.GenerateBelt");
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new BeltGeometryGenerator();
}
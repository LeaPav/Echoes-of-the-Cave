#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class Projectile : public Engine::Scripting::NativeScript {
public:
	float damageForce = 5.0f;
public:
	void OnInit() override {
		Inspect("Damage Force", &damageForce);
	}

	void OnUpdate(float deltaTime) override {
        auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();
        if (!physicsSystem) return;

 
        glm::vec3 vel = physicsSystem->GetLinearVelocity(entityID);
        float speed = glm::length(vel);

        if (speed < 1.0f) return; 
        auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
        glm::vec3 dir = glm::length(vel) > 0 ? glm::normalize(vel) : t.Forward;

        Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
            t.Position,
            t.Position + dir * 0.3f,
            entityID,
            { true, 0.3f, {1,0,0}, {1,1,0}, {0,1,1}, {0.5f,0.5f,0.5f}, 0.05f, 0.012f }
        );

        if (hit.hitEntity == Engine::ECS::NULL_ENTITY) return;

        std::string hitName = registry->GetEntityName(hit.hitEntity);

        if (hitName.find("Chain") != std::string::npos) {
            auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
            if (funcSys) {
                std::string callName = "Breakable.Hit." + hitName;
                funcSys->Call(callName, { damageForce });
            }
        }
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new Projectile();
}
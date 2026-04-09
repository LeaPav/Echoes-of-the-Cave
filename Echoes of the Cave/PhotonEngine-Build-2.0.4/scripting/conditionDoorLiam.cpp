#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif



class conditionDoor : public Engine::Scripting::NativeScript
{
private:
	Engine::ECS::Entity Entity = Engine::ECS::NULL_ENTITY;
	Engine::ECS::Entity colEntity = Engine::ECS::NULL_ENTITY;
	int collectible = 0;

public:


	void OnInit() override {

		Inspect("collectible", &collectible);
	}

	void OnCreate() override {
		FindEntity();
	}

	void FindEntity() {
		for (auto e : registry->View<Engine::Components::Transform>()) {
			if (registry->GetEntityName(e) == "Box col") {
				Entity = e;
				break;
			}
		}
	}

	void OnUpdate(float deltaTime) override {

		if (!registry->HasComponent<Engine::Components::Transform>(Entity)) return;

		auto& box = registry->GetComponent<Engine::Components::Transform>(entityID);
		auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();

		Engine::Systems::PhysicsUtils::RaycastHit hit = physicsSystem->Raycast(
			box.Position,
			box.Position + box.Up * 0.50f,
			entityID,
			{ true, 0.1f,{1,0,0}, {1,1,0}, {0,1,1}, {0.5f,0.5f,0.5f}, 0.01f, 0.012f }
		);

	}
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new conditionDoor();
}
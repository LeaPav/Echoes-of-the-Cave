#include "script_pch.h"

#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class InteractEntities : public Engine::Scripting::NativeScript {
public:

	Engine::ECS::Entity targetEntity = Engine::ECS::NULL_ENTITY;
	bool isCaptured = false;
	bool isMouseCaptured = false;

	void OnInit() override {

	}

	void OnCreate() override {
		FindTarget();
	}

	void FindTarget() {
		for (auto e : registry->View<Engine::Components::Transform>()) {
			if (registry->GetEntityName(e) == "Cube") {
				targetEntity = e;
				break;
			}
		}
	}

	void OnUpdate(float dt) override {

		if (targetEntity == Engine::ECS::NULL_ENTITY) {
			FindTarget();
			if (targetEntity == Engine::ECS::NULL_ENTITY) return;
		}

		auto& targetTransform = registry->GetComponent<Engine::Components::Transform>(targetEntity);

		if (InputSysteminstance->GetMouseButtonPressed(0)) {
			isCaptured = true;
		}
		
		if (isCaptured)
		{
			Engine::Systems::PhysicsUtils::RaycastHit hitResult = physicsSystem->Raycast(
				targetTransform.Position,
				targetTransform.Position - glm::vec3(0.05, 0.0, 0.0),
				targetEntity,
				{ true, 0.1f, {1, 0, 0}, {1, 1, 0}, {0, 1, 1}, {0.5, 0.5, 0.5}, 0.05f, 0.012f }
			);

			std::string HitEntityName = registry->GetEntityName(hitResult.hitEntity);
			TerminalInstance->print("RayCast Hit " + HitEntityName);

			//targetTransform.Positon = MousePos * dt;
		}
	}
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new InteractEntities();
}
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

		
	}
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new InteractEntities();
}
#include "script_pch.h"
#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class Breakable : public Engine::Scripting::NativeScript {
public:
	float impactForce = 2.f;
	std::string onBreakCall = "";
	bool alreadyBroken = false;

private:
	void OnHit(float force) {
		if (alreadyBroken) return;
		std::string myName = registry->GetEntityName(entityID);

		if (force < impactForce) {
			if (TerminalInstance)
				TerminalInstance->info("Breakable: impact trop faible sur '" +
					myName + "' (" + std::to_string(force) + ")");
			return;
		}

		alreadyBroken = true;
		if (TerminalInstance)
			TerminalInstance->info("Breakable: chaîne '" + myName + "' brisée !");


		std::string blockedName = "Climbable_Blocked_" + myName;
		for (auto e : registry->View<Engine::Components::Transform>()) {
			if (registry->GetEntityName(e) == blockedName) {
				if (registry->HasComponent<Engine::Components::RigidBody>(e)) {
					auto& rb = registry->GetComponent<Engine::Components::RigidBody>(e);

					rb.isStatic = false;  
					rb.dirty = true;
					rb.mass = 50.f;
				}
				auto physicsSystem = engine->GetSystem<Engine::Systems::PhysicsSystem>();
				if (physicsSystem) {
					
					physicsSystem->SetLinearVelocity(e, glm::vec3(0.0f, 0.0f, 0.0f));

					//physicsSystem->AddImpulse(e, glm::vec3(0.0f, -1.0f, 0.0f) * 2.0f);
				}
				if (TerminalInstance)
					TerminalInstance->info("Breakable: '" + blockedName + "' libéré !");
				break;
			}
		}

		auto& t = registry->GetComponent<Engine::Components::Transform>(entityID);
		t.Position.y -= 20.0f;

		if (!onBreakCall.empty()) {
			auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
			if (funcSys) funcSys->Call(onBreakCall, {});
		}
	}
public:
	void OnInit() override {
		Inspect("Impact Force Min", &impactForce);
	}

	void OnCreate() override {
		auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
		if (!funcSys) return;

		std::string myName = registry->GetEntityName(entityID);

		funcSys->Register("Breakable.Hit." + myName,
			[this](std::vector<std::any> args) -> std::any {
				float force = 0.0f;
				if (!args.empty()) {
					try { force = std::any_cast<float>(args[0]); }
					catch (...) {}
				}
				OnHit(force);
				return {};
			});

		if (TerminalInstance)
			TerminalInstance->info("Breakable: Prêt.");
	}

	void OnDestroy() override {
		auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
		if (funcSys) {
			std::string myName = registry->GetEntityName(entityID);
			funcSys->Unregister("Breakable.Hit." + myName);
		}
	}
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new Breakable();
}
#include "script_pch.h"

#ifdef _WIN32
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __attribute__((visibility("default")))
#endif

class GameManager : public Engine::Scripting::NativeScript {
private:
	int puzzlesSolved = 0;
	bool gameWon = false;

	std::string puzzle1Door = "Door_Puzzle1";
	std::string puzzle2Door = "Door_Puzzle2";
	std::string puzzle3Door = "Door_Puzzle3";

public:
	int totalPuzzles = 3;

	std::string doorEntityName = "FinalDoor";
	std::string treasureEntityName = "Treasure";

private:
	void OnPuzzleSolved(int puzzleID) {
		puzzlesSolved++;

		if (TerminalInstance)
			TerminalInstance->info("GameManager: Puzzle " + std::to_string(puzzleID) +
				" résolu ! (" + std::to_string(puzzlesSolved) + "/" +
				std::to_string(totalPuzzles) + ")");

		OpenDoor(puzzleID);
		
		if (puzzlesSolved >= totalPuzzles) {
			WinGame();
		}
	}

	void OpenDoor(int puzzleID) {
		std::string doorName;
		if (puzzleID == 1) { doorName = puzzle1Door; }
		else if (puzzleID == 2) { doorName = puzzle2Door; }
		else if (puzzleID == 3) { doorName = puzzle3Door; }
		else return;

		for (auto e : registry->View<Engine::Components::Transform>()) {
			if (registry->GetEntityName(e) == doorName) {
				auto& t = registry->GetComponent<Engine::Components::Transform>(e); 
				t.Position.y -= 3.0f;
				if (TerminalInstance)
					TerminalInstance->info("GameManager: Porte '" + doorName + "' ouverte !");
				break;
			}
		}
	}

	void WinGame() {
		gameWon = true;
		if (TerminalInstance)
			TerminalInstance->info("GameManager: VICTOIRE !");

		for (auto e : registry->View<Engine::Components::Transform>()) {
			if (registry->GetEntityName(e) == doorEntityName) {
				auto& t = registry->GetComponent<Engine::Components::Transform>(e);
				t.Position.y -= 5.f;
				if (TerminalInstance)
					TerminalInstance->info("GameManager: Porte finale ouverte!");
				break;
			}
		}
	}
public:
	void OnInit() override {
		Inspect("Total Puzzles", &totalPuzzles);
	}
	void OnCreate() override {
		auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
		if (!funcSys) return;

		funcSys->Register("GameManager.PuzzleSolved", [this](std::vector<std::any> args) -> std::any {
			if (args.empty()) return {};
			try {
				int puzzleID = std::any_cast<int>(args[0]);
				OnPuzzleSolved(puzzleID);
			}
			catch (const std::bad_any_cast&) {
				if (TerminalInstance) TerminalInstance->error("GameManager: PuzzleSolved - argument invalide");
			}
			return {};
			});


		funcSys->Register("GameManager.GetProgress", [this](std::vector<std::any> args) -> std::any {
			return (float)puzzlesSolved / (float)totalPuzzles;
			});

		funcSys->Register("GameManager.IsGameWon", [this](std::vector<std::any> args) -> std::any {
			return gameWon;
			});

		if (TerminalInstance) TerminalInstance->info("GameManager: Initialisé — " + std::to_string(totalPuzzles) + " puzzles à résoudre");
	}

	void OnUpdate(float deltaTime) override {

	}

	void OnDestroy() override {
		auto funcSys = engine->GetSystem<Engine::Systems::FunctionRegistrySystem>();
		if (funcSys) {
			funcSys->Unregister("GameManager.PuzzleSolved");
			funcSys->Unregister("GameManager.GetProgress");
			funcSys->Unregister("GameManager.IsGameWon");
		}
	}
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
	return new GameManager();
}
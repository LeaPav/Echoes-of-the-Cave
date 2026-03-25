#pragma once

// --- Standard Library ---
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

// --- Third-Party Libraries ---
// GLM Math
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// GLFW (Needed for Keycodes and Mouse Button Codes)
// #include <GLFW/glfw3.h>
#include "Graphics\GLWrapper.h"

// --- Core Engine & ECS ---
#include "Core/Defines.h"
#include "Core/Engine.h"
#include "ECS/Entity.h"
#include "ECS/Registry.h"

// --- Scripting Base ---
#include "Scripting/NativeScripting.h"
#include "Scripting/NativeScriptingHelpers.h"

// --- Systems ---
#include "Systems/InputSystem.h"
#include "Systems/PhysicsSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/TerminalSystem.h"
#include "Systems/FunctionRegisterySystem.h"

// --- Components ---
// Include commonly used components so scripts don't have to manually include them
// #include "Components/Transform.h"
// #include "Components/Camera.h"
// #include "Components/Light.h"
// #include "Components/MeshRenderer.h"
// #include "Components/RigidBody.h"
// #include "Components/AudioSource.h"
// #include "Components/Animator.h"
// #include "Components/MaterialOverride.h"
// #include "Components/Transparent.h"

#include "Components/Transform.h"
#include "Components/MeshRenderer.h"
#include "Components/Light.h"
#include "Components/Transparent.h"
#include "Components/WireframeRender.h"
#include "Components/MaterialOverride.h"
#include "Components/RigidBody.h"
#include "Components/Camera.h"
#include "Components/NanoLOD.h"
#include "Components/AudioListener.h"
#include "Components/AudioSource.h"
#include "Components/AudioEmitter.h"
#include "Components/BlueprintContext.h"
#include "Components/BVH_Compute.h"
#include "Components/NativeScriptComponent.h"
#include "Components/Animator.h"
#include "Components/DebugBox.h"

#include "Graphics/Mesh.h"

// --- Engine Globals ---
// If g_MouseCapturedByUI is an engine global used by scripts, declare it here
// extern bool g_MouseCapturedByUI;
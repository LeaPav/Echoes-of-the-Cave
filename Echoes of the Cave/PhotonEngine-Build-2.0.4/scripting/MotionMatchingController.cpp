#include "script_pch.h"
#include "Systems/MotionMatchingSystem.h"
#include "Systems/AnimatorSystem.h"
#include "Systems/ImGuiSystem.h"
#include "Components/Transform.h"
#include "Components/DebugBox.h"
#include <imgui.h>

#ifdef _WIN32
    #define SCRIPT_API __declspec(dllexport)
#else
    #define SCRIPT_API __attribute__((visibility("default")))
#endif

class MotionMatchingController : public Engine::Scripting::NativeScript {
public:
    Engine::Systems::MotionDatabase       m_MotionDB;
    Engine::Systems::MotionMatchingConfig m_Config;

    // ── Inspector-exposed parameters ─────────────────────────────────────────
    float moveSpeed               = 0.15f;
    float inputSmoothSpeed        = 10.0f;
    float transitionCooldown      = 0.20f;
    float minCostImprovementRatio   = 0.05f;
    // Minimum fraction of clip duration the playhead must be away from the
    // best-match time before an intra-clip seek is allowed.  Prevents the
    // system from snapping to the start of the same clip every cooldown tick.
    float intraClipSeekMinGapRatio  = 0.10f;  // 10% of clip duration

    // NEW: Blend settings
    float blendDuration           = 0.25f; // Duration of the crossfade in seconds

    // ── Motion-matching state ─────────────────────────────────────────────────
    std::string m_CurrentAnimName     = "None";
    float       m_CurrentAnimTime     = 0.0f;
    std::string m_PreviousAnimName    = "None"; // NEW: Tracks the fading animation
    float       m_BlendTimer          = 0.0f;   // NEW: Tracks blend progress

    int         m_CurrentEntryIdx     = -1;
    float       m_TimeSinceLastSwitch = 999.0f;
    float       m_CurrentEntryCost    = std::numeric_limits<float>::max();
    float       m_LastBestCost        = 0.0f;
    float       m_TicksPerSecond      = 1000.0f;

    // ── Tracking ──────────────────────────────────────────────────────────────
    // Root-motion compensation: measure hip world-XZ delta each frame and
    // subtract it from the entity's Transform so the character stays in place.
    glm::vec3 m_PrevHipWorld{0.0f};
    bool      m_HasPrevHip = false;
    glm::vec3 m_CurrentLocalVelocity{0.0f};

    // Active binding resolved this frame (empty = no binding matched / unfiltered)
    std::string m_ActiveBinding;

    // ── Debug: Trajectory ─────────────────────────────────────────────────────
    std::vector<Engine::ECS::Entity> m_DebugTrajectoryBoxes;

    // ── Debug: DB Sample Visualizer ───────────────────────────────────────────
    // Each visible DB sample spawns two boxes (left foot + right foot).
    // Best-match and current-entry get dedicated oversized boxes so they stand
    // out without needing a colour component.
    struct DBSamplePair {
        Engine::ECS::Entity left  = 0;
        Engine::ECS::Entity right = 0;
    };

    bool                     m_ShowDBSamples      = false;
    bool                     m_ShowTrajectoryArcs = true;   // traj dots for every shown sample
    int                      m_DBSampleStride     = 20;     // show 1 in every N entries
    int                      m_LastStride         = -1;     // detect stride change → respawn
    std::vector<DBSamplePair> m_DebugDBBoxes;               // strided regular samples
    std::vector<Engine::ECS::Entity> m_DebugDBTrajBoxes;    // trajectory dots for each sample

    // Dedicated highlight boxes (always present when DB is built)
    Engine::ECS::Entity m_BestMatchLeft   = 0;
    Engine::ECS::Entity m_BestMatchRight  = 0;
    Engine::ECS::Entity m_CurrentLeft     = 0;
    Engine::ECS::Entity m_CurrentRight    = 0;

    // All debug boxes are drawn at the same world size (1 cm cube).
    static constexpr float kBoxSize = 0.01f;

    // Colours per role  (RGB, matches DebugBox::Color convention)
    //  regular sample        – dark grey
    //  current-anim sample   – white
    //  trajectory arc dot    – yellow
    //  currently playing     – cyan
    //  best match this frame – red
    static constexpr glm::vec3 kColorSample       = glm::vec3(0.35f, 0.35f, 0.35f);
    static constexpr glm::vec3 kColorCurrentAnim  = glm::vec3(1.00f, 1.00f, 1.00f);
    static constexpr glm::vec3 kColorTraj         = glm::vec3(1.00f, 0.85f, 0.00f);
    static constexpr glm::vec3 kColorCurrentEntry = glm::vec3(0.00f, 1.00f, 1.00f);
    static constexpr glm::vec3 kColorBestMatch    = glm::vec3(1.00f, 0.15f, 0.15f);

    // ── Debug: Bone Resolution ───────────────────────────────────────────────
    // Filled after every DB build by scanning the animator's boneInfoMap.
    std::string              m_ResolvedLeftFoot  = "(not resolved)";
    std::string              m_ResolvedRightFoot = "(not resolved)";
    std::string              m_ResolvedHip       = "(not resolved)";
    std::vector<std::string> m_AllBoneNames;          // every bone on the rig
    bool                     m_ShowBoneList      = false;
    char                     m_BoneFilter[128]   = {};  // search box

    // ─────────────────────────────────────────────────────────────────────────

    void OnInit() override {
        Inspect("Move Speed",              &moveSpeed);
        Inspect("Input Smooth Speed",      &inputSmoothSpeed);
        Inspect("Transition Cooldown (s)", &transitionCooldown);
        Inspect("Min Cost Improve Ratio",  &minCostImprovementRatio);
        Inspect("Intra-Clip Seek Min Gap", &intraClipSeekMinGapRatio);
        Inspect("Blend Duration (s)",      &blendDuration);
    }

    void OnCreate() override {
        m_Config.databaseSampleStep   = 1000.0f / 30.0f;
        m_Config.trajectorySampleStep = 0.2f * 1000.0f;

        // Default bindings – tune speed thresholds to match your animation set.
        // These are populated here as a starting point; edit them in the HUD.
        // minSpeed/maxSpeed are in game-space units per tick (scaled by moveSpeed).
        m_Config.bindings = {
            // Standing still → idle
            { "", 0.0f,  0.01f },   // animationName filled in HUD once DB is built
            // Moving → walk (fill animationName via HUD binding section)
            { "", 0.01f, 999.0f },
        };

        if (auto* imguiSys = engine->GetSystem<Engine::Systems::ImGuiSystem>()) {
            imguiSys->RegisterUICallback("MotionMatchingDebug", [this]() {
                DrawDebugHUD();
            });
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ImGui Debug HUD
    // ─────────────────────────────────────────────────────────────────────────

    void DrawDebugHUD() {
        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("Motion Matching Debug", nullptr, flags);

        // ── Status ────────────────────────────────────────────────────────────
        ImGui::TextColored({0.0f,1.0f,0.0f,1.0f}, "--- System Status ---");
        ImGui::Text("Database Built: %s", m_MotionDB.isBuilt ? "YES" : "NO (Press G)");
        ImGui::Text("Total DB Entries: %zu", m_MotionDB.entries.size());

        ImGui::Separator();

        // ── Playback ──────────────────────────────────────────────────────────
        ImGui::TextColored({1.0f,0.8f,0.0f,1.0f}, "--- Playback State ---");
        ImGui::Text("Current Anim: %s", m_CurrentAnimName.c_str());
        ImGui::Text("Anim Time (Ticks): %.1f", m_CurrentAnimTime);
        ImGui::Text("Local Vel: X:%.2f  Z:%.2f", m_CurrentLocalVelocity.x, m_CurrentLocalVelocity.z);

        ImGui::Separator();

        // ── Cost ──────────────────────────────────────────────────────────────
        ImGui::TextColored({0.0f,1.0f,1.0f,1.0f}, "--- Cost Analytics ---");
        ImGui::Text("Current State Cost: %.3f", m_CurrentEntryCost);
        ImGui::Text("Last Best Cost:     %.3f", m_LastBestCost);

        ImGui::Separator();

        // ── Weights ───────────────────────────────────────────────────────────
        ImGui::TextColored({1.0f,0.5f,0.5f,1.0f}, "--- Config Weights ---");
        ImGui::TextWrapped("Lower pose weights to force trajectory matching!");
        ImGui::SliderFloat("Pose Position Weight",  &m_Config.posePositionWeight,        0.0f, 5.0f);
        ImGui::SliderFloat("Traj Position Weight",  &m_Config.trajectoryPositionWeight,  0.0f, 5.0f);
        ImGui::SliderFloat("Traj Direction Weight", &m_Config.trajectoryDirectionWeight, 0.0f, 5.0f);

        ImGui::Separator();

        // ── Animation Bindings ────────────────────────────────────────────────
        ImGui::TextColored({0.5f,1.0f,0.6f,1.0f}, "--- Animation Bindings ---");
        ImGui::TextWrapped("Map speed ranges to animations. First match wins.");
        ImGui::TextDisabled("Active binding: %s",
            m_ActiveBinding.empty() ? "(unfiltered)" : m_ActiveBinding.c_str());

        // Collect known animation names for combo selection
        std::vector<const char*> animNames;
        animNames.push_back("(none)");
        std::vector<std::string> animNameStrs;
        for (const auto& e : m_MotionDB.entries) {
            bool found = false;
            for (auto& s : animNameStrs) if (s == e.animationName) { found = true; break; }
            if (!found) animNameStrs.push_back(e.animationName);
        }
        for (const auto& s : animNameStrs) animNames.push_back(s.c_str());

        for (int bi = 0; bi < static_cast<int>(m_Config.bindings.size()); ++bi) {
            auto& b = m_Config.bindings[bi];
            ImGui::PushID(bi);

            // Animation name combo
            int sel = 0;
            for (int ai = 1; ai < static_cast<int>(animNames.size()); ++ai)
                if (b.animationName == animNames[ai]) { sel = ai; break; }
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::Combo("##anim", &sel, animNames.data(), static_cast<int>(animNames.size())))
                b.animationName = (sel == 0) ? "" : animNames[sel];

            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputFloat("##min", &b.minSpeed, 0.0f, 0.0f, "%.2f");
            ImGui::SameLine(); ImGui::TextDisabled("-");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputFloat("##max", &b.maxSpeed, 0.0f, 0.0f, "%.2f");
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                m_Config.bindings.erase(m_Config.bindings.begin() + bi);
                ImGui::PopID();
                goto bindings_done;
            }
            ImGui::PopID();
        }
        if (ImGui::SmallButton("+ Add Binding"))
            m_Config.bindings.push_back({"", 0.0f, 999.0f});
        bindings_done:;

        ImGui::Separator();

        // ── Bone Resolution ───────────────────────────────────────────────────
        ImGui::TextColored({1.0f, 0.75f, 0.2f, 1.0f}, "--- Bone Resolution ---");

        // Show which bone name was actually picked for each slot
        auto boneRow = [&](const char* label, const std::string& resolved,
                           std::vector<std::string>& candidates) {
            const bool ok = (resolved != "(not resolved)");
            ImGui::TextColored(ok ? ImVec4{0.4f,1.0f,0.4f,1.0f} : ImVec4{1.0f,0.3f,0.3f,1.0f},
                               "%s: %s", label, resolved.c_str());

            // Show and edit the candidate priority list
            if (ImGui::TreeNode(label)) {
                ImGui::TextDisabled("Candidates (first match wins):");
                for (int ci = 0; ci < static_cast<int>(candidates.size()); ++ci) {
                    char buf[128];
                    strncpy(buf, candidates[ci].c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf)-1] = '\0';
                    ImGui::PushID(ci);
                    if (ImGui::InputText("##cand", buf, sizeof(buf)))
                        candidates[ci] = buf;
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X") && candidates.size() > 1)
                        candidates.erase(candidates.begin() + ci);
                    ImGui::PopID();
                }
                if (ImGui::SmallButton("+ Add")) candidates.push_back("NewBone");
                ImGui::TreePop();
            }
        };

        boneRow("Left Foot",  m_ResolvedLeftFoot,  m_Config.leftFootCandidates);
        boneRow("Right Foot", m_ResolvedRightFoot, m_Config.rightFootCandidates);
        boneRow("Hip / Root", m_ResolvedHip,       m_Config.hipCandidates);

        ImGui::Spacing();

        // ── All-bones browser ─────────────────────────────────────────────────
        if (ImGui::Checkbox("Show All Bones On Rig", &m_ShowBoneList) && m_ShowBoneList)
            UpdateBoneResolution();  // refresh list on open

        if (m_ShowBoneList && !m_AllBoneNames.empty()) {
            ImGui::InputText("Filter", m_BoneFilter, sizeof(m_BoneFilter));
            const std::string filter(m_BoneFilter);

            ImGui::BeginChild("BoneList", ImVec2(0.0f, 180.0f), true);
            for (const auto& boneName : m_AllBoneNames) {
                if (!filter.empty() &&
                    boneName.find(filter) == std::string::npos) continue;

                // Highlight bones that are currently assigned to a slot
                const bool isLeft  = (boneName == m_ResolvedLeftFoot);
                const bool isRight = (boneName == m_ResolvedRightFoot);
                const bool isHip   = (boneName == m_ResolvedHip);

                if      (isLeft)  ImGui::TextColored({0.4f,1.0f,0.4f,1.0f}, "[LF] %s", boneName.c_str());
                else if (isRight) ImGui::TextColored({0.4f,0.8f,1.0f,1.0f}, "[RF] %s", boneName.c_str());
                else if (isHip)   ImGui::TextColored({1.0f,0.8f,0.2f,1.0f}, "[HIP] %s", boneName.c_str());
                else              ImGui::TextDisabled("%s", boneName.c_str());

                // Right-click context menu: assign bone to a slot
                if (ImGui::BeginPopupContextItem(boneName.c_str())) {
                    ImGui::TextDisabled("Assign '%s' to:", boneName.c_str());
                    if (ImGui::MenuItem("Left Foot")) {
                        m_Config.leftFootCandidates.insert(m_Config.leftFootCandidates.begin(), boneName);
                        m_ResolvedLeftFoot = boneName;
                    }
                    if (ImGui::MenuItem("Right Foot")) {
                        m_Config.rightFootCandidates.insert(m_Config.rightFootCandidates.begin(), boneName);
                        m_ResolvedRightFoot = boneName;
                    }
                    if (ImGui::MenuItem("Hip / Root")) {
                        m_Config.hipCandidates.insert(m_Config.hipCandidates.begin(), boneName);
                        m_ResolvedHip = boneName;
                    }
                    ImGui::EndPopup();
                }
                ImGui::OpenPopupOnItemClick(boneName.c_str(), ImGuiPopupFlags_MouseButtonRight);
            }
            ImGui::EndChild();

            ImGui::TextDisabled("%zu bones found", m_AllBoneNames.size());
        }

        ImGui::Separator();

        // ── DB Sample Visualizer ──────────────────────────────────────────────
        ImGui::TextColored({0.8f,0.6f,1.0f,1.0f}, "--- DB Sample Visualizer ---");

        if (ImGui::Checkbox("Show DB Samples", &m_ShowDBSamples)) {
            if (!m_ShowDBSamples) {
                HideDBSampleBoxes();
            } else if (m_MotionDB.isBuilt) {
                RespawnDBSampleBoxes();
            }
        }

        if (m_ShowDBSamples) {
            ImGui::Checkbox("Show Trajectory Arcs", &m_ShowTrajectoryArcs);

            // Stride slider – rebuild boxes if it changes
            int prevStride = m_DBSampleStride;
            ImGui::SliderInt("Sample Stride", &m_DBSampleStride, 1, 100);
            if (m_DBSampleStride != prevStride && m_MotionDB.isBuilt)
                RespawnDBSampleBoxes();

            // Legend
            ImGui::Spacing();
            ImGui::TextDisabled("All boxes: %.0fmm", kBoxSize * 1000.0f);
            ImGui::TextDisabled("Colors: grey=other  white=active clip  yellow=traj");
            ImGui::TextDisabled("        cyan=playing  red=best match");

            // Show breakdown by animation
            if (!m_MotionDB.entries.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("Animations in DB:");
                std::unordered_map<std::string, int> counts;
                for (const auto& e : m_MotionDB.entries) counts[e.animationName]++;
                for (const auto& [name, cnt] : counts)
                    ImGui::TextDisabled("  %s: %d frames", name.c_str(), cnt);
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Force Rebuild Database")) {
            auto* mmSys = engine->GetSystem<Engine::Systems::MotionMatchingSystem>();
            if (mmSys) {
                mmSys->BuildFeatureDatabase(entityID, m_MotionDB, m_Config);
                ResetState();
                if (m_ShowDBSamples) RespawnDBSampleBoxes();
                UpdateBoneResolution();
            }
        }

        ImGui::End();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // OnUpdate
    // ─────────────────────────────────────────────────────────────────────────

    void OnUpdate(float dt) override {
        auto* mmSys  = engine->GetSystem<Engine::Systems::MotionMatchingSystem>();
        auto* animSys = engine->GetSystem<Engine::Systems::AnimatorSystem>();
        if (!mmSys || !animSys) return;

        // ── Build DB on G ─────────────────────────────────────────────────────
        if (InputSysteminstance->GetKeyPressed(GLFW_KEY_G)) {
            TerminalInstance->info("Building Motion Database...");
            mmSys->BuildFeatureDatabase(entityID, m_MotionDB, m_Config);
            if (m_MotionDB.isBuilt) {
                ResetState();
                if (m_ShowDBSamples) RespawnDBSampleBoxes();
                SpawnHighlightBoxes();
                UpdateBoneResolution();
            }
        }

        if (!m_MotionDB.isBuilt) return;

        // ── 1. Input → smooth local velocity ─────────────────────────────────
        glm::vec3 targetDir(0.0f);
        if (InputSysteminstance->GetKeyState(GLFW_KEY_UP))    targetDir.z += 1.0f;
        if (InputSysteminstance->GetKeyState(GLFW_KEY_DOWN))  targetDir.z -= 1.0f;
        if (InputSysteminstance->GetKeyState(GLFW_KEY_LEFT))  targetDir.x += 1.0f;
        if (InputSysteminstance->GetKeyState(GLFW_KEY_RIGHT)) targetDir.x -= 1.0f;

        if (glm::length(targetDir) > 0.0f) targetDir = glm::normalize(targetDir);
        m_CurrentLocalVelocity = glm::mix(
            m_CurrentLocalVelocity,
            targetDir * moveSpeed,
            glm::clamp(dt * inputSmoothSpeed, 0.0f, 1.0f));

        // ── 2. Build desired trajectory ───────────────────────────────────────
        const int   numTotal = m_Config.numTrajectorySamples;
        const int   numPast  = m_Config.numPastSamples;
        const float step     = m_Config.trajectorySampleStep;

        std::vector<Engine::Systems::TrajectoryPoint> desiredTrajectory;
        desiredTrajectory.reserve(numTotal);

        for (int i = 0; i < numTotal; i++) {
            const float timeOffsetTicks   = static_cast<float>(i - numPast) * step;
            const float timeOffsetSeconds = timeOffsetTicks / m_TicksPerSecond;
            const glm::vec3 posOffset     = m_CurrentLocalVelocity * timeOffsetSeconds;

            Engine::Systems::TrajectoryPoint pt;
            pt.position = glm::vec2(posOffset.x, posOffset.z);

            const float speed2D = glm::length(glm::vec2(m_CurrentLocalVelocity.x, m_CurrentLocalVelocity.z));
            // When standing still use a zero direction so the cost function
            // penalises walking frames (which have a non-zero facing direction)
            // and naturally prefers idle frames that also store (0,0).
            pt.direction = (speed2D > 0.01f)
                ? glm::normalize(glm::vec2(m_CurrentLocalVelocity.x, m_CurrentLocalVelocity.z))
                : glm::vec2(0.0f, 0.0f);

            desiredTrajectory.push_back(pt);
        }

        UpdateDebugTrajectory(desiredTrajectory);

        // ── 3. Build query feature (foot positions + trajectory) ────────────────
        auto query = mmSys->ComputeQueryFeature(entityID, desiredTrajectory, m_Config);

        // ── Root-motion compensation ──────────────────────────────────────────
        auto getBonePos = [&](const std::vector<std::string>& candidates) -> glm::vec3 {
            for (const auto& name : candidates) {
                glm::vec3 p = animSys->GetBoneGlobalPos(entityID, name);
                if (glm::length(p) > 1e-5f) return p;
            }
            return glm::vec3(0.0f);
        };

        // // 1. Get the offset that is currently applied
        // glm::vec3 currentOffset = animSys->GetRootOffset(entityID);
        
        // // 2. Find the 'raw' hip position by stripping out the current offset
        // glm::vec3 rawHipPos = getBonePos(m_Config.hipCandidates) - currentOffset;

        // // 3. Force the hip to perfectly align with the Entity's XZ origin (0,0)
        // // Leave Y alone so the hips can still bounce up and down natively.
        // glm::vec3 newOffset = glm::vec3(-rawHipPos.x, currentOffset.y, -rawHipPos.z);
        
        // animSys->SetRootOffset(entityID, newOffset);
        
        // m_PrevHipWorld = rawHipPos;
        // m_HasPrevHip   = true;

        // ── Resolve active animation binding ─────────────────────────────────
        // Walk the binding list to find which animation to restrict the DB search
        // to. Speed is measured in the same units as m_CurrentLocalVelocity.
        const float speed2D = glm::length(glm::vec2(m_CurrentLocalVelocity.x, m_CurrentLocalVelocity.z));
        m_ActiveBinding = "";
        for (const auto& binding : m_Config.bindings) {
            if (speed2D >= binding.minSpeed && speed2D < binding.maxSpeed) {
                m_ActiveBinding = binding.animationName;
                break;
            }
        }

        // ── Fetch state info for playback wrap ────────────────────────────────
        Engine::Systems::Animation::AnimationUtils::AnimationState* state = nullptr;
        float durationTicks = 1000.0f;

        if (m_CurrentAnimName != "None") {
            state = animSys->FindActiveState(entityID, m_CurrentAnimName);
            if (state && state->animation) {
                const float animTicks = state->animation->GetTicksPerSecond();
                if (animTicks > 0.0f) m_TicksPerSecond = animTicks;

                durationTicks = state->animation->GetDuration();
                if (durationTicks <= 0.0f) durationTicks = 1000.0f;
            }
        }

        // ── 4. Advance playhead ────────────────────────────────────────────────
        m_CurrentAnimTime += dt * m_TicksPerSecond;
        if (m_CurrentAnimTime >= durationTicks) {
            m_CurrentAnimTime = fmod(m_CurrentAnimTime, durationTicks);
            // m_HasPrevHip = false; // Prevent teleporting when the animation loops!
        }
        m_TimeSinceLastSwitch += dt;

        // Keep m_CurrentEntryIdx tracking the actual playhead position
        AdvanceCurrentEntryIdx();

        // ── 5. Query database ─────────────────────────────────────────────────
        int bestIdx = -1;
        if (m_TimeSinceLastSwitch >= transitionCooldown) {
            bestIdx = mmSys->QueryDatabase(m_MotionDB, query, m_Config, m_ActiveBinding);

            if (bestIdx >= 0) {
                const auto& bestEntry = m_MotionDB.entries[bestIdx];

                const float bestCost = mmSys->ComputeFeatureCost(
                    query, bestEntry.feature, m_MotionDB.normalization, m_Config);
                m_LastBestCost = bestCost;

                m_CurrentEntryCost = std::numeric_limits<float>::max();
                if (m_CurrentEntryIdx >= 0 &&
                    m_CurrentEntryIdx < static_cast<int>(m_MotionDB.entries.size())) {
                    m_CurrentEntryCost = mmSys->ComputeFeatureCost(
                        query,
                        m_MotionDB.entries[m_CurrentEntryIdx].feature,
                        m_MotionDB.normalization, m_Config);
                }

                const bool isDifferentAnim = (bestEntry.animationName != m_CurrentAnimName);
                const bool isBetterMatch   = (bestCost < m_CurrentEntryCost * (1.0f - minCostImprovementRatio));

                // Suppress intra-clip seeks that only jump to a nearby frame —
                // this stops the end-to-start oscillation when the best DB match
                // is just the beginning of the clip we are already playing.
                // bool seekAllowed = isDifferentAnim || isBetterMatch;
                bool seekAllowed = isDifferentAnim;
                if (seekAllowed && !isDifferentAnim) {
                    const float gap = std::abs(bestEntry.animationTime - m_CurrentAnimTime);
                    const float minGap = durationTicks * intraClipSeekMinGapRatio;
                    if (gap < minGap) seekAllowed = false;
                }

                if (seekAllowed) {
                    SwitchToEntry(animSys, bestIdx, bestCost);
                    state = animSys->FindActiveState(entityID, m_CurrentAnimName);
                }
            }
        }

        // ── 6. Sync Animator to Script ────────────────────────────────────────
        if (state) state->currentTime = m_CurrentAnimTime;

        // ── 7. Update world-space DB visualizer ───────────────────────────────
        if (m_ShowDBSamples && m_MotionDB.isBuilt)
            UpdateDBSampleVisualization(bestIdx);
            
        // ── 8. Smooth Weight Blending ─────────────────────────────────────────
        if (m_BlendTimer < blendDuration && m_PreviousAnimName != "None") {
            m_BlendTimer += dt;
            
            // Protect against divide-by-zero if you set blend duration to 0 in inspector
            float t = (blendDuration > 0.001f) ? glm::clamp(m_BlendTimer / blendDuration, 0.0f, 1.0f) : 1.0f;
            
            // Smoothstep function for a more natural ease-in / ease-out curve
            float smoothT = t * t * (3.0f - 2.0f * t);

            animSys->SetAnimationWeight(entityID, m_CurrentAnimName, smoothT);
            animSys->SetAnimationWeight(entityID, m_PreviousAnimName, 1.0f - smoothT);

            if (t >= 1.0f) {
                // Setting weight to 0.0f natively tells AnimatorSystem to delete it
                animSys->SetAnimationWeight(entityID, m_PreviousAnimName, 0.0f);
                m_PreviousAnimName = "None";
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // OnDestroy
    // ─────────────────────────────────────────────────────────────────────────

    void OnDestroy() override {
        if (auto* imguiSys = engine->GetSystem<Engine::Systems::ImGuiSystem>())
            imguiSys->UnregisterUICallback("MotionMatchingDebug");

        for (auto e : m_DebugTrajectoryBoxes) registry->DestroyEntity(e);
        m_DebugTrajectoryBoxes.clear();

        DestroyDBSampleBoxes();
    }

private:

    // =========================================================================
    // Trajectory Visualizer
    // =========================================================================

    void UpdateDebugTrajectory(const std::vector<Engine::Systems::TrajectoryPoint>& traj) {
        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;
        auto& charTransform = registry->GetComponent<Engine::Components::Transform>(entityID);

        while (m_DebugTrajectoryBoxes.size() < traj.size()) {
            Engine::ECS::Entity e = registry->CreateEntity();
            registry->AddComponent(e, Engine::Components::Transform{});
            Engine::Components::DebugBox dbTraj{};
            dbTraj.Color = kColorTraj;
            registry->AddComponent(e, dbTraj);
            m_DebugTrajectoryBoxes.push_back(e);
        }

        glm::mat4 model = LocalToWorldMatrix(charTransform);

        for (size_t i = 0; i < traj.size(); ++i) {
            auto& t = registry->GetComponent<Engine::Components::Transform>(m_DebugTrajectoryBoxes[i]);
            glm::vec3 localPos(traj[i].position.x, 0.05f, traj[i].position.y);
            t.Position = glm::vec3(model * glm::vec4(localPos, 1.0f));
            t.Scale    = glm::vec3(0.01f);
            t.Rotation = charTransform.Rotation;
        }
    }

    // =========================================================================
    // DB Sample Visualizer – Spawn / Destroy / Update
    // =========================================================================

    // Build a world matrix for the character's root: position + Y-rotation only.
    // Scale is intentionally excluded here because DB feature positions are already
    // pre-multiplied by the entity scale before being passed in (see UpdateDBSampleVisualization).
    // Keeping scale out avoids double-scaling and lets Y remain in world units.
    glm::mat4 LocalToWorldMatrix(const Engine::Components::Transform& t) const {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), t.Position);
        m = glm::rotate(m, glm::radians(t.Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        return m;
    }

    Engine::ECS::Entity SpawnBox(const glm::vec3& color) {
        Engine::ECS::Entity e = registry->CreateEntity();
        Engine::Components::Transform xf{};
        xf.Scale = glm::vec3(kBoxSize);
        registry->AddComponent(e, xf);
        Engine::Components::DebugBox db{};
        db.Color = color;
        registry->AddComponent(e, db);
        return e;
    }

    void HideBox(Engine::ECS::Entity e) {
        if (!e || !registry->HasComponent<Engine::Components::Transform>(e)) return;
        // Move far below the scene so it's invisible without destroying it
        registry->GetComponent<Engine::Components::Transform>(e).Position = glm::vec3(0.0f, -9999.0f, 0.0f);
    }

    // Destroy all pooled DB sample boxes and the highlight boxes
    void DestroyDBSampleBoxes() {
        for (auto& pair : m_DebugDBBoxes) {
            if (pair.left)  registry->DestroyEntity(pair.left);
            if (pair.right) registry->DestroyEntity(pair.right);
        }
        m_DebugDBBoxes.clear();

        for (auto e : m_DebugDBTrajBoxes) if (e) registry->DestroyEntity(e);
        m_DebugDBTrajBoxes.clear();

        if (m_BestMatchLeft)  { registry->DestroyEntity(m_BestMatchLeft);  m_BestMatchLeft  = 0; }
        if (m_BestMatchRight) { registry->DestroyEntity(m_BestMatchRight); m_BestMatchRight = 0; }
        if (m_CurrentLeft)    { registry->DestroyEntity(m_CurrentLeft);    m_CurrentLeft    = 0; }
        if (m_CurrentRight)   { registry->DestroyEntity(m_CurrentRight);   m_CurrentRight   = 0; }
    }

    // (Re)spawn exactly the right number of box entities for the current stride
    void RespawnDBSampleBoxes() {
        DestroyDBSampleBoxes();
        if (!m_MotionDB.isBuilt || m_MotionDB.entries.empty()) return;

        const int stride    = glm::max(m_DBSampleStride, 1);
        const int numShown  = static_cast<int>(
            std::ceil(static_cast<float>(m_MotionDB.entries.size()) / stride));
        const int numTraj   = m_Config.numTrajectorySamples;

        m_DebugDBBoxes.resize(numShown);
        for (auto& pair : m_DebugDBBoxes) {
            pair.left  = SpawnBox(kColorSample);
            pair.right = SpawnBox(kColorSample);
        }

        // Trajectory arc dots: numShown samples × numTraj points each
        const int numTrajDots = numShown * numTraj;
        m_DebugDBTrajBoxes.resize(numTrajDots);
        for (auto& e : m_DebugDBTrajBoxes)
            e = SpawnBox(kColorTraj);

        SpawnHighlightBoxes();
        m_LastStride = stride;
    }

    void SpawnHighlightBoxes() {
        if (m_BestMatchLeft)  registry->DestroyEntity(m_BestMatchLeft);
        if (m_BestMatchRight) registry->DestroyEntity(m_BestMatchRight);
        if (m_CurrentLeft)    registry->DestroyEntity(m_CurrentLeft);
        if (m_CurrentRight)   registry->DestroyEntity(m_CurrentRight);

        m_BestMatchLeft   = SpawnBox(kColorBestMatch);
        m_BestMatchRight  = SpawnBox(kColorBestMatch);
        m_CurrentLeft     = SpawnBox(kColorCurrentEntry);
        m_CurrentRight    = SpawnBox(kColorCurrentEntry);
    }

    void HideDBSampleBoxes() {
        for (auto& pair : m_DebugDBBoxes) {
            HideBox(pair.left);
            HideBox(pair.right);
        }
        for (auto e : m_DebugDBTrajBoxes) HideBox(e);
        HideBox(m_BestMatchLeft);
        HideBox(m_BestMatchRight);
        HideBox(m_CurrentLeft);
        HideBox(m_CurrentRight);
    }

    // Called every frame while m_ShowDBSamples is true
    void UpdateDBSampleVisualization(int bestIdx) {
        if (!registry->HasComponent<Engine::Components::Transform>(entityID)) return;
        auto& charXf = registry->GetComponent<Engine::Components::Transform>(entityID);

        // DB foot / trajectory positions are stored in FBX model space (hip-relative,
        // post-invGlobalTransform). The entity's Transform.Scale converts those model
        // units to world units (e.g. 0.01 for a cm-scale Mixamo import -> metres).
        // We scale XZ manually so Y can stay fixed in world units (0.02 m above ground).
        // Assumes uniform horizontal scale -- Scale.x is the representative factor.
        const float modelToWorld = charXf.Scale.x > 1e-6f ? charXf.Scale.x : 1.0f;
        const glm::mat4 toWorld  = LocalToWorldMatrix(charXf);

        const int stride   = glm::max(m_DBSampleStride, 1);
        const int numTraj  = m_Config.numTrajectorySamples;

        // Rebuild entity pool if stride changed externally (e.g. from HUD slider)
        if (stride != m_LastStride) {
            RespawnDBSampleBoxes();
            return; // positions will be correct next frame
        }

        int boxIdx = 0;

        for (int i = 0; i < static_cast<int>(m_MotionDB.entries.size()); i += stride) {
            if (boxIdx >= static_cast<int>(m_DebugDBBoxes.size())) break;

            const auto& entry = m_MotionDB.entries[i];
            auto& pair        = m_DebugDBBoxes[boxIdx];

            // Colour: white for the active clip, grey for everything else
            const bool isCurrentAnim = (entry.animationName == m_CurrentAnimName);
            const glm::vec3 footColor = isCurrentAnim ? kColorCurrentAnim : kColorSample;

            // Left foot  (scale XZ from model-space units to world metres)
            if (registry->HasComponent<Engine::Components::Transform>(pair.left)) {
                glm::vec3 lLocal(entry.feature.leftFootPosition.x * modelToWorld,
                                 0.02f,
                                 entry.feature.leftFootPosition.z * modelToWorld);
                auto& xf  = registry->GetComponent<Engine::Components::Transform>(pair.left);
                xf.Position = glm::vec3(toWorld * glm::vec4(lLocal, 1.0f));
                if (registry->HasComponent<Engine::Components::DebugBox>(pair.left))
                    registry->GetComponent<Engine::Components::DebugBox>(pair.left).Color = footColor;
            }

            // Right foot  (scale XZ from model-space units to world metres)
            if (registry->HasComponent<Engine::Components::Transform>(pair.right)) {
                glm::vec3 rLocal(entry.feature.rightFootPosition.x * modelToWorld,
                                 0.02f,
                                 entry.feature.rightFootPosition.z * modelToWorld);
                auto& xf  = registry->GetComponent<Engine::Components::Transform>(pair.right);
                xf.Position = glm::vec3(toWorld * glm::vec4(rLocal, 1.0f));
                if (registry->HasComponent<Engine::Components::DebugBox>(pair.right))
                    registry->GetComponent<Engine::Components::DebugBox>(pair.right).Color = footColor;
            }

            // Trajectory arc dots for this sample
            if (m_ShowTrajectoryArcs) {
                const int trajBase = boxIdx * numTraj;
                for (int s = 0; s < numTraj; s++) {
                    const int dotIdx = trajBase + s;
                    if (dotIdx >= static_cast<int>(m_DebugDBTrajBoxes.size())) break;

                    Engine::ECS::Entity dotE = m_DebugDBTrajBoxes[dotIdx];
                    if (!registry->HasComponent<Engine::Components::Transform>(dotE)) continue;

                    const auto& tp = entry.feature.trajectory[s];
                    // trajectory.position is (X, Z) in root-local space
                    glm::vec3 tLocal(tp.position.x * modelToWorld, 0.04f, tp.position.y * modelToWorld);
                    auto& xf   = registry->GetComponent<Engine::Components::Transform>(dotE);
                    xf.Position = glm::vec3(toWorld * glm::vec4(tLocal, 1.0f));
                }
            } else {
                // Hide trajectory dots if the option is off
                const int trajBase = boxIdx * numTraj;
                for (int s = 0; s < numTraj; s++) {
                    const int dotIdx = trajBase + s;
                    if (dotIdx < static_cast<int>(m_DebugDBTrajBoxes.size()))
                        HideBox(m_DebugDBTrajBoxes[dotIdx]);
                }
            }

            ++boxIdx;
        }

        // ── Best-match highlight ──────────────────────────────────────────────
        auto placeHighlight = [&](Engine::ECS::Entity e, const glm::vec3& localPos) {
            if (!e || !registry->HasComponent<Engine::Components::Transform>(e)) return;
            registry->GetComponent<Engine::Components::Transform>(e).Position =
                glm::vec3(toWorld * glm::vec4(localPos, 1.0f));
        };

        if (bestIdx >= 0 && bestIdx < static_cast<int>(m_MotionDB.entries.size())) {
            const auto& best = m_MotionDB.entries[bestIdx];
            placeHighlight(m_BestMatchLeft,
                glm::vec3(best.feature.leftFootPosition.x * modelToWorld,  0.08f, best.feature.leftFootPosition.z * modelToWorld));
            placeHighlight(m_BestMatchRight,
                glm::vec3(best.feature.rightFootPosition.x * modelToWorld, 0.08f, best.feature.rightFootPosition.z * modelToWorld));
        } else {
            HideBox(m_BestMatchLeft);
            HideBox(m_BestMatchRight);
        }

        // ── Currently-playing entry highlight ─────────────────────────────────
        if (m_CurrentEntryIdx >= 0 &&
            m_CurrentEntryIdx < static_cast<int>(m_MotionDB.entries.size())) {
            const auto& cur = m_MotionDB.entries[m_CurrentEntryIdx];
            placeHighlight(m_CurrentLeft,
                glm::vec3(cur.feature.leftFootPosition.x * modelToWorld,  0.06f, cur.feature.leftFootPosition.z * modelToWorld));
            placeHighlight(m_CurrentRight,
                glm::vec3(cur.feature.rightFootPosition.x * modelToWorld, 0.06f, cur.feature.rightFootPosition.z * modelToWorld));
        } else {
            HideBox(m_CurrentLeft);
            HideBox(m_CurrentRight);
        }
    }

    // =========================================================================
    // Bone Resolution Helper
    // =========================================================================

    // Scans the Animator component's boneInfoMap to populate m_AllBoneNames and
    // determine which candidate name was actually resolved for each feature slot.
    // Safe to call anytime after the Animator is initialised.
    void UpdateBoneResolution() {
        m_AllBoneNames.clear();

        if (!registry->HasComponent<Engine::Components::Animator>(entityID)) return;
        auto& animator = registry->GetComponent<Engine::Components::Animator>(entityID);

        // Collect every bone name from the boneInfoMap
        m_AllBoneNames.reserve(animator.boneInfoMap.size());
        for (const auto& [name, _] : animator.boneInfoMap)
            m_AllBoneNames.push_back(name);
        std::sort(m_AllBoneNames.begin(), m_AllBoneNames.end());

        // Replicate ResolveBoneName logic: first candidate present in the map wins
        auto resolve = [&](const std::vector<std::string>& candidates) -> std::string {
            for (const auto& name : candidates)
                if (animator.boneInfoMap.count(name)) return name;
            return "(not resolved)";
        };

        m_ResolvedLeftFoot  = resolve(m_Config.leftFootCandidates);
        m_ResolvedRightFoot = resolve(m_Config.rightFootCandidates);
        m_ResolvedHip       = resolve(m_Config.hipCandidates);

        TerminalInstance->info("[MotionMatching] Bone resolution: LF='" +
            m_ResolvedLeftFoot + "' RF='" + m_ResolvedRightFoot + "' HIP='" + m_ResolvedHip + "'");
    }

    // =========================================================================
    // Motion Matching Helpers
    // =========================================================================

    // Advance m_CurrentEntryIdx to the DB entry whose animationTime is closest
    // to the current playhead, within the currently-playing clip.
    void AdvanceCurrentEntryIdx() {
        if (m_CurrentAnimName == "None" || m_MotionDB.entries.empty()) return;

        float bestDiff = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(m_MotionDB.entries.size()); ++i) {
            const auto& e = m_MotionDB.entries[i];
            if (e.animationName != m_CurrentAnimName) continue;
            const float diff = std::abs(e.animationTime - m_CurrentAnimTime);
            if (diff < bestDiff) {
                bestDiff          = diff;
                m_CurrentEntryIdx = i;
            }
        }
    }

    void SwitchToEntry(Engine::Systems::AnimatorSystem* animSys, int entryIdx, float cost) {
        const auto& entry = m_MotionDB.entries[entryIdx];

        if (entry.animationName != m_CurrentAnimName) {
            
            if (m_CurrentAnimName == "None") {
                // First time playing, no blend needed
                animSys->PlayAnimation(entityID, entry.animationName, true);
            } else {
                // Clean up any interrupted blend
                if (m_PreviousAnimName != "None" && m_PreviousAnimName != entry.animationName) {
                    animSys->SetAnimationWeight(entityID, m_PreviousAnimName, 0.0f);
                }
                
                // Start the crossfade
                m_PreviousAnimName = m_CurrentAnimName;
                m_BlendTimer = 0.0f;

                // Play additively so it doesn't kill the previous animation
                animSys->PlayBlendAnimation(entityID, entry.animationName, true);
                
                // Force the new animation to start at 0 weight so it fades in smoothly
                animSys->SetAnimationWeight(entityID, entry.animationName, 0.0f);
            }
            
            // Lock the hip bones natively in the animator so root motion is stripped!
            animSys->SetBonesToLock(entityID, entry.animationName, m_Config.hipCandidates);
        }

        m_CurrentAnimTime     = entry.animationTime;
        m_CurrentAnimName     = entry.animationName;
        m_CurrentEntryIdx     = entryIdx;
        m_CurrentEntryCost    = cost;
        m_TimeSinceLastSwitch = 0.0f;
    }

    void ResetState() {
        m_CurrentAnimName      = "None";
        m_PreviousAnimName     = "None";  // NEW
        m_BlendTimer           = 0.0f;    // NEW
        m_CurrentAnimTime      = 0.0f;
        m_CurrentEntryIdx      = -1;
        m_CurrentEntryCost     = std::numeric_limits<float>::max();
        m_TimeSinceLastSwitch  = 999.0f;
        m_CurrentLocalVelocity = glm::vec3(0.0f);
        m_ActiveBinding        = "";
    }
};

extern "C" SCRIPT_API Engine::Scripting::NativeScript* CreateScript() {
    return new MotionMatchingController();
}
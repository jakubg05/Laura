#include "SceneManager.h"

namespace Laura
{

	LR_GUID SceneManager::CreateScene(const std::string& name) {
		if (m_InRuntimeSimulation) {
			LOG_ENGINE_WARN("CreateScene: Cannot create scenes while in runtime simulation");
			return LR_GUID::INVALID;
		}

		auto scene = std::make_shared<Scene>();
		scene->name = name;
		(*m_ActiveScenes)[scene->guid] = scene;
		LOG_ENGINE_INFO("CreateScene: created new scene \"{0}\" with GUID {1}", name, (uint64_t)scene->guid);
		return scene->guid;
	}

	void SceneManager::DeleteScene(LR_GUID guid) {
		if (m_InRuntimeSimulation) {
			LOG_ENGINE_WARN("DeleteScene: Cannot delete scenes while in runtime simulation");
			return;
		}

		if (m_OpenSceneGuid == guid) {
			m_OpenSceneGuid = LR_GUID::INVALID;
			LOG_ENGINE_INFO("DeleteScene: closed open scene (GUID {0}) because it was deleted", (uint64_t)guid);
		}

		size_t removed = m_ActiveScenes->erase(guid);
		if (removed == 0) {
			LOG_ENGINE_WARN("DeleteScene: no scene found with GUID {0}; nothing deleted", (uint64_t)guid);
		} else {
			LOG_ENGINE_INFO("DeleteScene: successfully removed scene with GUID {0}", (uint64_t)guid);
		}
	}

	bool SceneManager::SetOpenSceneGuid(LR_GUID guid) {
		auto scene = find(guid);
		if (!scene && guid != LR_GUID::INVALID) { // allow setting to LR_GUID::INVALID (no scene selected)
			LOG_ENGINE_WARN("SetOpenScene: cannot open scene; no scene registered with GUID {0}", (uint64_t)guid);
			return false;
		}

		m_OpenSceneGuid = guid;
		LOG_ENGINE_INFO("SetOpenScene: now tracking scene with GUID {0} as the active scene", (uint64_t)guid);
		return true;
	}

	LR_GUID SceneManager::GetOpenSceneGuid() const {
		return m_OpenSceneGuid;
	}

	std::shared_ptr<Scene> SceneManager::GetOpenScene() const {
		return find(m_OpenSceneGuid);
	}


	void SceneManager::EnterRuntimeSimulation() {
		if (m_InRuntimeSimulation) { return; }

		m_InRuntimeSimulation = true;
		
		m_RuntimeSimulationScenes.clear(); // remove all data
		for (const auto& [guid, scene] : m_Scenes) {
			std::shared_ptr<Scene> sceneCopy = Scene::Copy(scene);
			m_RuntimeSimulationScenes.emplace(guid, sceneCopy);
		}

		m_OpenSceneGuidCache = m_OpenSceneGuid; // cache the currently open scene id
		m_ActiveScenes = &m_RuntimeSimulationScenes;
	}


	void SceneManager::ExitRuntimeSimulation() {
		if (!m_InRuntimeSimulation) { return; }

		m_InRuntimeSimulation = false;
		m_RuntimeSimulationScenes.clear(); // remove all data
		m_OpenSceneGuid = m_OpenSceneGuidCache; // recover the scene id
		m_ActiveScenes = &m_Scenes;
	}


	void SceneManager::SaveScenesToFolder(const std::filesystem::path& folderpath) const {
		// Delete orphaned .lrscene files
		for (const auto& scenepath : FindFilesInFolder(folderpath, SCENE_FILE_EXTENSION)) {
			LR_GUID guid = ExtractGuidFromScenepath(scenepath);
			if (guid == LR_GUID::INVALID) {
				LOG_ENGINE_WARN("SaveScenesToFolder: invalid GUID in filename \"{0}\"; skipping orphaned file", scenepath.string());
				continue;
			}

			if (!find(guid)) {
				LOG_ENGINE_INFO("SaveScenesToFolder: deleting orphaned scene file \"{0}\" (GUID {1})", scenepath.string(), (uint64_t)guid);
				std::filesystem::remove(scenepath);
			}
		}

		// Serialize in-memory scenes
		for (const auto& [guid, scene] : m_Scenes) { // always save the real scenes
			std::filesystem::path scenepath = ComposeScenepathFromGuid(folderpath, guid);
			if (!SaveSceneFile(scenepath, scene)) {
				LOG_ENGINE_WARN("SaveScenesToFolder: failed to serialize scene GUID {0} to \"{1}\"", (uint64_t)guid, scenepath.string());
			} else {
				LOG_ENGINE_INFO("SaveScenesToFolder: successfully saved scene GUID {0} to \"{1}\"", (uint64_t)guid, scenepath.string());
			}
		}
	}

	void SceneManager::LoadScenesFromFolder(const std::filesystem::path& folderpath) {
		for (const auto& scenepath : FindFilesInFolder(folderpath, SCENE_FILE_EXTENSION)) {
			auto scene = LoadSceneFile(scenepath);
			if (!scene) {
				LOG_ENGINE_WARN("LoadScenesFromFolder: failed to deserialize scene file \"{0}\"; skipping", scenepath.string());
				continue;
			}
			LR_GUID guid = scene->guid;
			m_Scenes[guid] = scene;
			LOG_ENGINE_INFO("LoadScenesFromFolder: loaded scene \"{0}\" with GUID {1}", scenepath.string(), (uint64_t)guid);
		}
	}
}

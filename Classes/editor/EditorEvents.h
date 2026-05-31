// EditorEvents.h - 编辑器事件（ugf::EventBus）
#ifndef EDITOR_EVENTS_H
#define EDITOR_EVENTS_H

#include <string>
#include "EventSystem.hpp"

// 节点交互
struct NodeSelectedEvent { std::string nodeName; };
struct NodeAddedEvent { std::string nodeName; std::string parentName; };
struct NodeRemovedEvent { std::string nodeName; };
struct NodeRenamedEvent { std::string oldName; std::string newName; };

// 属性
struct PropertyChangedEvent { std::string key, oldValue, newValue; };

// 时间轴
struct FrameChangedEvent { int currentFrame; };
struct PlayStateChangedEvent { bool isPlaying; };

// 项目/文件
struct ProjectLoadedEvent { std::string projectPath; std::string projectName; };
struct SceneDataChangedEvent {};

#endif

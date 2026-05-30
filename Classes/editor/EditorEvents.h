// EditorEvents.h - 编辑器事件（ugf::EventBus）
#ifndef EDITOR_EVENTS_H
#define EDITOR_EVENTS_H

#include <string>
#include "EventSystem.hpp"

struct NodeSelectedEvent { std::string nodeName; };
struct PropertyChangedEvent { std::string key, oldValue, newValue; };
struct FrameChangedEvent { int currentFrame; };
struct PlayStateChangedEvent { bool isPlaying; };

#endif

# CMakeLists.txt - 外部 CMake 封装，编译 behavac 核心库
# 不修改 Library/behaviac/ 内的任何文件
cmake_minimum_required(VERSION 3.6)

set(BEHAVIAC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/behaviac)

# --- 平台特定源文件 ---
if(UNIX AND NOT APPLE)
    set(BEHAVIAC_PLATFORM_SRC
        ${BEHAVIAC_ROOT}/src/common/file/filesystem_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/system_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/mutex_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/wrapper_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/socket/defaultsocketwrapper_gcc.cpp
    )
elseif(WIN32)
    set(BEHAVIAC_PLATFORM_SRC
        ${BEHAVIAC_ROOT}/src/common/file/filesystem_vcc.cpp
        ${BEHAVIAC_ROOT}/src/common/system_vcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/mutex_vcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/wrapper_vcc.cpp
        ${BEHAVIAC_ROOT}/src/common/socket/defaultsocketwrapper_vcc.cpp
    )
else()
    set(BEHAVIAC_PLATFORM_SRC
        ${BEHAVIAC_ROOT}/src/common/file/filesystem_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/system_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/mutex_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/thread/wrapper_gcc.cpp
        ${BEHAVIAC_ROOT}/src/common/socket/defaultsocketwrapper_gcc.cpp
    )
endif()

# --- 通用源码 ---
set(BEHAVIAC_COMMON_SRC
    ${BEHAVIAC_ROOT}/src/agent/agent.cpp
    ${BEHAVIAC_ROOT}/src/agent/context.cpp
    ${BEHAVIAC_ROOT}/src/agent/namedevent.cpp
    ${BEHAVIAC_ROOT}/src/agent/propertynode.cpp
    ${BEHAVIAC_ROOT}/src/agent/state_t.cpp
    ${BEHAVIAC_ROOT}/src/agent/taskmethod.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/behaviortree.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/behaviortree_task.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/registernodes.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/attachments/attachaction.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/attachments/Effector.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/attachments/event.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/attachments/Precondition.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/action.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/assignment.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/compute.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/end.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/noop.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/wait.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/waitforsignal.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/actions/waitframes.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/ifelse.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/parallel.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/referencebehavior.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/selector.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/selectorloop.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/selectorprobability.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/selectorstochastic.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/sequence.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/sequencestochastic.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/compositestochastic.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/composites/withprecondition.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/and.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/conditionbase.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/condition.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/false.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/or.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/conditions/true.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratoralwaysfailure.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratoralwaysrunning.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratoralwayssuccess.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorcount.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorcountlimit.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorfailureuntil.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorframes.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratoriterator.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorlog.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorloop.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorloopuntil.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratornot.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorrepeat.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorsuccessuntil.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratortime.cpp
    ${BEHAVIAC_ROOT}/src/behaviortree/nodes/decorators/decoratorweight.cpp
    ${BEHAVIAC_ROOT}/src/common/assert.cpp
    ${BEHAVIAC_ROOT}/src/common/base.cpp
    ${BEHAVIAC_ROOT}/src/common/convertutf.cpp
    ${BEHAVIAC_ROOT}/src/common/crc.cpp
    ${BEHAVIAC_ROOT}/src/common/factory.cpp
    ${BEHAVIAC_ROOT}/src/common/file/file.cpp
    ${BEHAVIAC_ROOT}/src/common/file/filemanager.cpp
    ${BEHAVIAC_ROOT}/src/common/file/listfiles.cpp
    ${BEHAVIAC_ROOT}/src/common/logger/logger.cpp
    ${BEHAVIAC_ROOT}/src/common/logger/logmanager.cpp
    ${BEHAVIAC_ROOT}/src/common/memory/memory.cpp
    ${BEHAVIAC_ROOT}/src/common/meta.cpp
    ${BEHAVIAC_ROOT}/src/common/object/tagobject.cpp
    ${BEHAVIAC_ROOT}/src/common/operation.cpp
    ${BEHAVIAC_ROOT}/src/common/profiler/profiler.cpp
    ${BEHAVIAC_ROOT}/src/common/randomgenerator/randomgenerator.cpp
    ${BEHAVIAC_ROOT}/src/common/rttibase.cpp
    ${BEHAVIAC_ROOT}/src/common/serialization/textnode.cpp
    ${BEHAVIAC_ROOT}/src/common/socket/socketconnect_base.cpp
    ${BEHAVIAC_ROOT}/src/common/socket/socketconnect.cpp
    ${BEHAVIAC_ROOT}/src/common/string/stringcrc.cpp
    ${BEHAVIAC_ROOT}/src/common/string/stringutils.cpp
    ${BEHAVIAC_ROOT}/src/common/thread/wrapper.cpp
    ${BEHAVIAC_ROOT}/src/common/workspace.cpp
    ${BEHAVIAC_ROOT}/src/common/xml/xmlanalyzer.cpp
    ${BEHAVIAC_ROOT}/src/common/xml/xml.cpp
    ${BEHAVIAC_ROOT}/src/fsm/alwaystransition.cpp
    ${BEHAVIAC_ROOT}/src/fsm/fsm.cpp
    ${BEHAVIAC_ROOT}/src/fsm/fsmstate.cpp
    ${BEHAVIAC_ROOT}/src/fsm/startcondition.cpp
    ${BEHAVIAC_ROOT}/src/fsm/transitioncondition.cpp
    ${BEHAVIAC_ROOT}/src/fsm/waitframesstate.cpp
    ${BEHAVIAC_ROOT}/src/fsm/waitstate.cpp
    ${BEHAVIAC_ROOT}/src/fsm/waittransition.cpp
    ${BEHAVIAC_ROOT}/src/htn/agentproperties.cpp
    ${BEHAVIAC_ROOT}/src/htn/agentstate.cpp
    ${BEHAVIAC_ROOT}/src/htn/htnmethod.cpp
    ${BEHAVIAC_ROOT}/src/htn/planner.cpp
    ${BEHAVIAC_ROOT}/src/htn/plannertask.cpp
    ${BEHAVIAC_ROOT}/src/htn/task.cpp
    ${BEHAVIAC_ROOT}/src/network/network.cpp
    ${BEHAVIAC_ROOT}/src/property/properties.cpp
    ${BEHAVIAC_ROOT}/src/property/property.cpp
)

add_library(libbehaviac STATIC
    ${BEHAVIAC_COMMON_SRC}
    ${BEHAVIAC_PLATFORM_SRC}
)

target_include_directories(libbehaviac PUBLIC ${BEHAVIAC_ROOT}/inc)

# 静态库不需要 DLL 导出宏
target_compile_definitions(libbehaviac PUBLIC BEHAVIAC_STATIC)

# 忽略旧代码风格的警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(libbehaviac PRIVATE
        -Wno-deprecated-declarations
        -Wno-register
    )
endif()

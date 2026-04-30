cmake_minimum_required(VERSION 3.26)
project(uuv_sim)

set(CMAKE_CXX_STANDARD 17)

add_executable(uuv_sim main.cpp
        vec2.cpp
        vec2.h
        agent.cpp
        agent.h
        decision_tree.cpp
        decision_tree.h
        field.cpp
        field.h
        event_controller.cpp
        event_controller.h
        world_state.h
        neighbor_info.h
        data_logger.cpp
        data_logger.h
        behavior_params.cpp
        behavior_params.h)

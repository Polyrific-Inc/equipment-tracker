add_executable(equipment_tracker_app main.cpp)
target_link_libraries(equipment_tracker_app PRIVATE equipment_tracker)

# Install
install(TARGETS equipment_tracker_app
    RUNTIME DESTINATION bin
)
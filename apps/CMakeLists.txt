file (GLOB sender_sources sender.c)

set(EXAMPLE_BIN ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/sender CACHE STRING "sender dir")

add_executable(sender ${sender_sources})

target_link_libraries(sender of_iface)



file (GLOB receiver_sources receiver.c)

set(EXAMPLE_BIN ${PROJECT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/receiver CACHE STRING "receiver dir")

add_executable(receiver ${receiver_sources})

target_link_libraries(receiver of_iface)
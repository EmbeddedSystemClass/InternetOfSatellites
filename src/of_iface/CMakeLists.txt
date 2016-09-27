file (GLOB lib_of_sources ./lib_sender.c ./lib_receiver.c ./lib_socket_utils.c)

add_library(of_iface  SHARED  ${lib_of_sources})

set_target_properties(of_iface PROPERTIES
			VERSION 0.0.1
			SOVERSION 1) 

target_link_libraries(of_iface openfec m)
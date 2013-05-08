add_executable (query_world query_world.cpp)
target_link_libraries (query_world owl-common owl-solver pthread)

#If you wanted to install these examples in the owl binary directory you should
#use a target like the one below:
#INSTALL(TARGETS query_world RUNTIME DESTINATION bin/owl)

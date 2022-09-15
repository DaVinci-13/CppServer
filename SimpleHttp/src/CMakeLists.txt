cmake_minimum_required(VERSION 3.10)

# set the project name
project(SimpleHttp)

# add the executable
add_executable(SimpleHttp main.c Server.c)

target_include_directories(SimpleHttp PUBLIC 
                        "${PROJECT_BINARY_DIR}" 
                        "${PROJECT_SOURCE_DIR}" 
)
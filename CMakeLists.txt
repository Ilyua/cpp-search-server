cmake_minimum_required(VERSION 3.24)
project(search_server)

set(CMAKE_CXX_STANDARD 17)

FILE(GLOB MyCSources ./search-server/*.cpp)
ADD_EXECUTABLE(search_server ${MyCSources} )


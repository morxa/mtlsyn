add_library(mtl INTERFACE)
target_include_directories(mtl INTERFACE include)
target_link_libraries(mtl INTERFACE utilities)

find_package(Protobuf QUIET)

if (Protobuf_FOUND)
  message(STATUS "Protobuf found, building MTL proto library")
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS mtl.proto)
  add_library(mtl_proto SHARED mtl_proto.cpp ${PROTO_SRCS} ${PROTO_HDRS})
  target_link_libraries(mtl_proto PUBLIC mtl protobuf::libprotobuf)
  target_include_directories(mtl_proto PUBLIC include ${CMAKE_BINARY_DIR}/src)
else()
  message(STATUS "Protobuf not found, skipping MTL proto library")
endif()

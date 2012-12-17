add_subdirectory(udt)

add_library(asio-udt
    src/asio-udt/acceptor.cc
    src/asio-udt/error-category.cc
    src/asio-udt/service.cc
    src/asio-udt/socket.cc
)

target_link_libraries(asio-udt udt)

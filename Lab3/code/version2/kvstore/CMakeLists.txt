
# === Example replicated calculator ===
add_executable(kvstoresystem
               kvstoresystem.cxx
               logger.cc
               in_memory_log_store.cxx)
add_dependencies(kvstoresystem
                 static_lib)
target_link_libraries(kvstoresystem
                      ${BUILD_DIR}/${LIBRARY_OUTPUT_NAME}
                      ${LIBRARIES})

add_executable(client
               client.cxx
               resp.hxx)



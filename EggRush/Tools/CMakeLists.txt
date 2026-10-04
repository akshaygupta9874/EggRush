# eggrush_bot: headless balance / fairness / calibration runner (Section 67).
add_executable(eggrush_bot eggrush_bot.cpp)
target_link_libraries(eggrush_bot PRIVATE er_content er_sim Qt6::Core)
er_set_warnings(eggrush_bot)
set_target_properties(eggrush_bot PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

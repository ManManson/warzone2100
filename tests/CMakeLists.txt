enable_testing()

add_test(NAME replay_rewind_fixtures
	COMMAND sh "${CMAKE_SOURCE_DIR}/tests/replay/run.sh")
set_tests_properties(replay_rewind_fixtures PROPERTIES
	LABELS "slow"
	ENVIRONMENT "WZ=${CMAKE_BINARY_DIR}/src/warzone2100"
)

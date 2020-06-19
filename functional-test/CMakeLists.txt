add_custom_target(
        functional-test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/functional-test"
        COMMAND ../build/buildsyspp test.lua --clean
        COMMAND sha256sum -c result.txt
        COMMAND ../build/buildsyspp test.lua
        COMMAND sha256sum -c result.txt
)

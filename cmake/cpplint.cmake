find_program(CPPLINT cpplint)
if (CPPLINT)
    add_custom_target(
            cpplint
            COMMAND ${CPPLINT}
            ${SOURCE_FILES}
    )
endif ()
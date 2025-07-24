function (link_compile_commands name)
    if (MSVC OR NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        return()
    endif ()

    string(REGEX REPLACE "\/|\\|\ |\." "_" _dir ${CMAKE_CURRENT_LIST_DIR})
    string(REGEX REPLACE "\/|\\|\ |\." "_" _name ${name})

    set(cmd_target "${CMAKE_CURRENT_LIST_DIR}/${name}")
    add_custom_command(
        OUTPUT ${cmd_target} #
        COMMAND ${CMAKE_COMMAND} -E #
                create_symlink "${CMAKE_BINARY_DIR}/${name}" "${CMAKE_CURRENT_LIST_DIR}/${name}"
    )

    add_custom_target(${_dir}_${name} ALL DEPENDS ${cmd_target})
endfunction ()

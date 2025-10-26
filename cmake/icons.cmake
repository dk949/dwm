set(ICONPREFIX
    "${CMAKE_INSTALL_PREFIX}/share/pixmaps"
    CACHE INTERNAL ""
)
function (target_add_icon_loc target access)
    target_compile_definitions(${target} ${access} -DICONDIR="${ICONPREFIX}")
endfunction ()
function (install_icons)
    install(
        FILES
              ${CMAKE_SOURCE_DIR}/icons/dwm-error.svg
              ${CMAKE_SOURCE_DIR}/icons/dwm-icon-black.svg
              ${CMAKE_SOURCE_DIR}/icons/dwm-icon.svg
              ${CMAKE_SOURCE_DIR}/icons/dwm-warning.svg
        DESTINATION ${ICONPREFIX}
    )
endfunction ()

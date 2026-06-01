zephyr_include_directories(.)

SET(uwb_src
    uwb_impl.cpp
    aliro_uwb.c
    uwbrange.c
    uwb_cli.c
    uwbproto.c
    uwbcanned.c
    uciproto.c
    hbciproto.c
    nrfspi.c
    )

add_compile_definitions(DEBUG_ASSERT_PRODUCTION_CODE=0)

zephyr_library_sources(${uwb_src})


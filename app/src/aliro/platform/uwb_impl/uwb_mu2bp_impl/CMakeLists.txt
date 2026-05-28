zephyr_include_directories(.)

SET(uwb_src
		uwb_impl.cpp
		aliro_uwb.c
        uwb_cli.c
        uwbproto.c
		uwbcanned.c
		uciproto.c
		hbciproto.c
		nrfspi.c
	)

zephyr_library_sources(${uwb_src})


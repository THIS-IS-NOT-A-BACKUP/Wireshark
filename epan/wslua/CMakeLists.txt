# CMakeLists.txt
#
# Wireshark - Network traffic analyzer
# By Gerald Combs <gerald@wireshark.org>
# Copyright 1998 Gerald Combs
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

add_subdirectory(lrexlib)

set(WSLUA_MODULES
	${CMAKE_CURRENT_SOURCE_DIR}/lua_bitop.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_address.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_byte_array.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_capture_info.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_column.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_conversation.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_dir.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_dissector.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_dumper.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_field.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_file.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_file_common.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_file_handler.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_frame_info.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_gui.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_int64.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_internals.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_listener.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_nstime.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_pinfo.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_pref.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_proto.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_proto_expert.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_proto_field.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_struct.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_tree.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_tvb.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_utility.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_wtap.c
	${CMAKE_CURRENT_SOURCE_DIR}/wslua_gcrypt.c
)

set(WSLUA_FILES
	${WSLUA_MODULES}
	${CMAKE_CURRENT_BINARY_DIR}/taps_wslua.c
	${CMAKE_CURRENT_BINARY_DIR}/register_wslua.c
	${CMAKE_CURRENT_SOURCE_DIR}/init_wslua.c
)
source_group(wslua FILES ${WSLUA_FILES})

set_source_files_properties(
	${WSLUA_FILES}
	PROPERTIES
	COMPILE_FLAGS "${WERROR_COMMON_FLAGS}"
)

# make-taps.py depends on the current working directory
# to find the dissector files (contents of taps file
# depend on this actually, so just changing the paths
# in these lists won't help).

# Used with untypical CWD
set(WSLUA_TAPS_USED
	${CMAKE_SOURCE_DIR}/epan/dissectors/packet-http.h
	${CMAKE_SOURCE_DIR}/epan/dissectors/packet-ip.h
	${CMAKE_SOURCE_DIR}/epan/dissectors/packet-udp.h
	${CMAKE_SOURCE_DIR}/epan/dissectors/packet-h225.h
	${CMAKE_SOURCE_DIR}/epan/dissectors/packet-ieee80211.h
)

add_custom_command(
	OUTPUT
		${CMAKE_BINARY_DIR}/epan/wslua/taps_wslua.c
		# XXX taps.txt doesn't appear to be used anywhere.
		${CMAKE_BINARY_DIR}/epan/wslua/taps.txt
	COMMAND
		${Python3_EXECUTABLE}
		${CMAKE_SOURCE_DIR}/epan/wslua/make-taps.py
		${CMAKE_BINARY_DIR}/epan/wslua/taps_wslua.c
		${CMAKE_BINARY_DIR}/epan/wslua/taps.txt
	DEPENDS
		${CMAKE_SOURCE_DIR}/epan/wslua/taps.ini
		${CMAKE_SOURCE_DIR}/epan/wslua/make-taps.py
		# Only here to add dependencies for the taps "source" files
		${WSLUA_TAPS_USED}
)


add_custom_command(
	OUTPUT
		declare_wslua.h
		register_wslua.c
	COMMAND
		${Python3_EXECUTABLE}
		${CMAKE_SOURCE_DIR}/epan/wslua/make-reg.py
		${WSLUA_MODULES}
	DEPENDS
		${CMAKE_SOURCE_DIR}/epan/wslua/make-reg.py
		${WSLUA_MODULES}
		# ${CMAKE_CURRENT_BINARY_DIR}/taps_wslua.c
)

add_custom_target(
	register_wslua ALL
	DEPENDS
		declare_wslua.h
		register_wslua.c
)

set_target_properties(register_wslua PROPERTIES FOLDER "Libs/epan/wslua")

add_library(wslua OBJECT
	${WSLUA_FILES}
)

if(FETCH_lua)
	add_dependencies(wslua lua54)
endif()

target_include_directories(wslua
	SYSTEM PRIVATE
		${GCRYPT_INCLUDE_DIRS}
		${GNUTLS_INCLUDE_DIRS}
		${LUA_INCLUDE_DIRS}
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(wslua PROPERTIES
	FOLDER "Libs/epan/wslua"
	COMPILE_DEFINITIONS "WS_BUILD_DLL"
)

CHECKAPI(
	NAME
	  wslua
	SWITCHES
	SOURCES
	  ${WSLUA_FILES}
)

#
# Editor modelines  -  https://www.wireshark.org/tools/modelines.html
#
# Local variables:
# c-basic-offset: 8
# tab-width: 8
# indent-tabs-mode: t
# End:
#
# vi: set shiftwidth=8 tabstop=8 noexpandtab:
# :indentSize=8:tabSize=8:noTabs=false:
#

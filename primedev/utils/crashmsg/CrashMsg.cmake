# CrashMsg

add_executable(CrashMsg
               "tier0/filestream.cpp"
               "tier0/filestream.h"
               "tier0/utils.cpp"
			   "tier0/utils.h"
               "utils/crashmsg/crashmsg.cpp"
)

target_precompile_headers(CrashMsg PRIVATE core/stdafx.h)

target_compile_definitions(CrashMsg PRIVATE
                           _CRT_SECURE_NO_WARNINGS
                           UNICODE
                           _UNICODE
)

target_link_libraries(CrashMsg PRIVATE
                      shlwapi.lib
                      kernel32.lib
                      user32.lib
                      gdi32.lib
                      winspool.lib
					  dbghelp.lib
                      comdlg32.lib
                      advapi32.lib
                      shell32.lib
                      ole32.lib
                      oleaut32.lib
                      uuid.lib
                      odbc32.lib
                      odbccp32.lib
                      WS2_32.lib
)

target_include_directories(CrashMsg PRIVATE utils/crashmsg)

set_target_properties(CrashMsg PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${NS_BINARY_DIR}/bin
					  OUTPUT_NAME crashmsg
                      COMPILE_FLAGS "/W4"
                      LINK_FLAGS "/MANIFEST:NO /DEBUG /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
)

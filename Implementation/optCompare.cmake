set(OPTCOMPARE_NAME optCompare)

file(GLOB OPTCOMPARE_SOURCES   ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB OPTCOMPARE_INCS      ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB OPTCOMPARE_INC_TD    ${NATID_SDK_INC}/td/*.h)
file(GLOB OPTCOMPARE_INC_GUI   ${NATID_SDK_INC}/gui/*.h)
file(GLOB OPTCOMPARE_INC_CNT   ${NATID_SDK_INC}/cnt/*.h)
file(GLOB OPTCOMPARE_INC_DENSE ${NATID_SDK_INC}/dense/*.h)

set(OPTCOMPARE_PLIST ${CMAKE_CURRENT_LIST_DIR}/src/Info.plist)

# add executable
add_executable(${OPTCOMPARE_NAME} ${OPTCOMPARE_INCS} ${OPTCOMPARE_SOURCES}
				${OPTCOMPARE_INC_TD} ${OPTCOMPARE_INC_CNT}
				${OPTCOMPARE_INC_GUI} ${OPTCOMPARE_INC_DENSE})

source_group("inc"        FILES ${OPTCOMPARE_INCS})
source_group("inc\\td"    FILES ${OPTCOMPARE_INC_TD})
source_group("inc\\cnt"   FILES ${OPTCOMPARE_INC_CNT})
source_group("inc\\gui"   FILES ${OPTCOMPARE_INC_GUI})
source_group("inc\\dense" FILES ${OPTCOMPARE_INC_DENSE})
source_group("src"        FILES ${OPTCOMPARE_SOURCES})

target_link_libraries(${OPTCOMPARE_NAME}
			debug ${MU_LIB_DEBUG} debug ${NATGUI_LIB_DEBUG} debug ${MATRIX_LIB_DEBUG}
			optimized ${MU_LIB_RELEASE} optimized ${NATGUI_LIB_RELEASE} optimized ${MATRIX_LIB_RELEASE})

setTargetPropertiesForGUIApp(${OPTCOMPARE_NAME} ${OPTCOMPARE_PLIST})

setIDEPropertiesForGUIExecutable(${OPTCOMPARE_NAME} ${CMAKE_CURRENT_LIST_DIR})

setPlatformDLLPath(${OPTCOMPARE_NAME})

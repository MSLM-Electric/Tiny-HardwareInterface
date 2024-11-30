cmake_minimum_required(VERSION 3.0.2)
set(PROJ_NAME Tiny_HardwareInterface)
project(${PROJ_NAME})
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
#=============================---------------VERSION GENERATOR---------------------===================================
include_directories(${CMAKE_BINARY_DIR}/../version_generator)
set(VERSION_GENERATOR_PATH ${CMAKE_SOURCE_DIR}/version_generator)
add_executable(${PROJ_NAME}_versiontestexecutable ${VERSION_GENERATOR_PATH}/runversiontest.c ${VERSION_GENERATOR_PATH}/funcGetVersion.c ${VERSION_GENERATOR_PATH}/funcGetVersion.h)
find_package(Git)
add_custom_target(version
  ${CMAKE_COMMAND} -D SRC=${CMAKE_SOURCE_DIR}/version_generator/generateheader_version.h.in
                   -D DST=${CMAKE_SOURCE_DIR}/${PROJ_NAME}_version.h
                   -D PROJECT_NAME=${PROJ_NAME}
                   -D SHOWFUNC_SRC=${VERSION_GENERATOR_PATH}/generateruntest_file.c.in
                   -D SHOWFUNC_DST=${VERSION_GENERATOR_PATH}/funcGetVersion.c
                   -D GIT_EXECUTABLE=${GIT_EXECUTABLE}                   
                   -P ${CMAKE_SOURCE_DIR}/version_generator/GenerateVersionHeader.cmake
  )
add_dependencies(${PROJ_NAME}_versiontestexecutable version)
#======================================================================================================================
#add_definitions(-DFREERTOS_V10)
add_definitions(-DDEBUG_ON_VS)

set(ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(SIMPLETIMER ${ROOT_PATH})
set(HARDWAREINTERFACE_UNIT ${ROOT_PATH})

message("Start building project...")

set(SOURCES
    #${BITLOGGER}/BitLogger.c
    ${SIMPLETIMER}/SimpleTimerWP.c
    ${HARDWAREINTERFACE_UNIT}/HardwareInterfaceUnit.c
    ${ROOT_PATH}/mainXC8_RecvContinious.c

    ${ROOT_PATH}/portXC8.c
    ${ROOT_PATH}/usart.c
)

set(HEADERS
    ${ROOT_PATH}/type_def.h
    #${BITLOGGER}/BitLogger.h
    ${SIMPLETIMER}/SimpleTimerWP.h
    ${HARDWAREINTERFACE_UNIT}/HardwareInterfaceUnit.h
	${ROOT_PATH}/vssimulationport.h
    ${ROOT_PATH}/usart.h

)

add_executable(${PROJ_NAME} ${SOURCES} ${HEADERS})

if(${CMAKE_VERSION} VERSION_LESS "2.8.12")
    include_directories(${PROJ_NAME} PUBLIC ${INCLUDE_DIR})
else()
    target_include_directories(${PROJ_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>/include)
endif()

target_link_options(${PROJ_NAME} PRIVATE "/MAP:${PROJECT_SOURCE_DIR}/outputProject.map")

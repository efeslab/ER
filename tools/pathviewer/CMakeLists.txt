#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(pathviewer
  main.cpp
)

set(KLEE_LIBS
  kleeSupport
)

target_link_libraries(pathviewer ${KLEE_LIBS})

install(TARGETS pathviewer RUNTIME DESTINATION bin)

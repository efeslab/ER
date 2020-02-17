#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(prepass
  main.cpp
)

set(KLEE_LIBS
  kleeCore
)

target_link_libraries(prepass ${KLEE_LIBS})

install(TARGETS prepass RUNTIME DESTINATION bin)

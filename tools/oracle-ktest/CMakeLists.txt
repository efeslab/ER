#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(oracle-ktest
  main.cpp
)

set(KLEE_LIBS
  kleeBasic
  kleeSupport
)

target_link_libraries(oracle-ktest ${KLEE_LIBS})

install(TARGETS oracle-ktest RUNTIME DESTINATION bin)

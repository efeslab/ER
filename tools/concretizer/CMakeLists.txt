#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(concretizer
  main.cpp
)

set(KLEE_LIBS
  kleeBasic
  kleeSupport
)

target_link_libraries(concretizer ${KLEE_LIBS})

install(TARGETS concretizer RUNTIME DESTINATION bin)

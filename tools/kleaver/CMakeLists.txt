#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
add_executable(kleaver
  main.cpp
  ExprInPlaceTransformation.cpp
  Drawer.cpp
  GraphvizDOTDrawer.cpp
  JsonDrawer.cpp
  DataRecReplaceVisitor.cpp
)

set(KLEE_LIBS
  kleaverSolver
  kleaverExpr
  kleeModule
)

target_link_libraries(kleaver ${KLEE_LIBS})

install(TARGETS kleaver RUNTIME DESTINATION bin)

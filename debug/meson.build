# SPDX-License-Identifier: MIT

sources = 'debug.c'
libdebug = static_library('debug', sources, include_directories: '..',
  dependencies: gtkdep)
libdebug_dep = declare_dependency(include_directories: '.', link_with: libdebug)

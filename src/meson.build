# SPDX-License-Identifier: MIT

gnome = import('gnome')

gr = gnome.compile_resources('gr', 'fsv-gresource.xml')

srcs = ['about.c', 'animation.c', 'callbacks.c', 'camera.c', 'colexp.c',
  'color.c', 'common.c', 'dialog.c', 'dirtree.c', 'filelist.c', 'fsv.c',
  'geometry.c', 'gui.c', 'ogl.c', 'scanfs.c', 'tmaptext.c',
  'viewport.c', 'window.c']
incdir = include_directories('..', '../lib')
executable('fsv', sources: [srcs, gr],
  dependencies : [libmisc_dep, libdebug_dep, gtkdep, libm, cglm_dep],
  include_directories: incdir)

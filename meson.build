project('sfm', 'c')

ncurses = dependency('ncurses')
magic = dependency('libmagic')

executable('sfm', 'src/sfm.c', 'src/sort.c', dependencies : [ncurses, magic])

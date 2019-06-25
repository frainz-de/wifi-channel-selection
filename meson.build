project('collector', 'cpp',
    default_options : ['c_std=c17', 'cpp_std=c++17'])
add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
src = ['main.cpp','sample.cpp']
eigen_dep = dependency('eigen3')
thread_dep = dependency('threads')
executable('collector', sources : src, dependencies : [thread_dep, eigen_dep])

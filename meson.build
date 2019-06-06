project('collector', 'cpp',
    default_options : ['c_std=c17', 'cpp_std=c++17'])
add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
src = ['main.cpp','sample.cpp']
eigen_dep = dependency('eigen3')
executable('collector', sources : src, dependencies : eigen_dep)

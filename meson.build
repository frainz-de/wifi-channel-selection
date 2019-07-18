project('collector', 'cpp',
    default_options : ['c_std=c17', 'cpp_std=c++17'])
add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
src = ['main.cpp','sample.cpp','collector.cpp','neighbor_manager.cpp','helpers.cpp']
eigen_dep = dependency('eigen3')
thread_dep = dependency('threads')
json_dep = dependency('nlohmann_json')
tclap_dep = dependency('tclap')
executable('collector', sources : src, dependencies : [thread_dep, json_dep, eigen_dep, tclap_dep])

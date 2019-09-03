project('collector', ['cpp','c'],
    default_options : ['c_std=c17', 'cpp_std=c++17'])
#extra_args = ['fstack-protector']
extra_args = ['flto']
add_project_link_arguments(['-lstdc++fs'], language : 'cpp')
src = ['main.cpp','sample.cpp','collector.cpp','neighbor_manager.cpp','helpers.cpp','channel_strategy.cpp','wpa_ctrl.c']
thread_dep = dependency('threads')
#json_dep = dependency('nlohmann_json')
json_subp = subproject('nlohmann_json')
#json_dep = dependency('nlohmann_json', fallback : [json_subp, nlohmann_json_dep])
json_dep = json_subp.get_variable('nlohmann_json_dep')
tclap_dep = dependency('tclap')
executable('collector', sources : src, dependencies : [thread_dep, json_dep, tclap_dep])

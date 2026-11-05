# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "RelWithDebInfo")
  file(REMOVE_RECURSE
  "CMakeFiles/cvat-native_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/cvat-native_autogen.dir/ParseCache.txt"
  "CMakeFiles/cvat_core_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/cvat_core_autogen.dir/ParseCache.txt"
  "CMakeFiles/cvat_core_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/cvat_core_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/cvat_ui_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/cvat_ui_autogen.dir/ParseCache.txt"
  "cvat-native_autogen"
  "cvat_core_autogen"
  "cvat_core_tests_autogen"
  "cvat_ui_autogen"
  )
endif()

# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.28

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/bin/cmake

# The command to remove a file.
RM = /usr/local/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/virtualplatform/SDN/Controller/Distribute/test

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/virtualplatform/SDN/Controller/Distribute/test/build

# Include any dependencies generated for this target.
include CMakeFiles/RaftTest.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/RaftTest.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/RaftTest.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/RaftTest.dir/flags.make

CMakeFiles/RaftTest.dir/unit/buffer.cxx.o: CMakeFiles/RaftTest.dir/flags.make
CMakeFiles/RaftTest.dir/unit/buffer.cxx.o: /home/virtualplatform/SDN/Controller/Distribute/test/unit/buffer.cxx
CMakeFiles/RaftTest.dir/unit/buffer.cxx.o: CMakeFiles/RaftTest.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/virtualplatform/SDN/Controller/Distribute/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/RaftTest.dir/unit/buffer.cxx.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/RaftTest.dir/unit/buffer.cxx.o -MF CMakeFiles/RaftTest.dir/unit/buffer.cxx.o.d -o CMakeFiles/RaftTest.dir/unit/buffer.cxx.o -c /home/virtualplatform/SDN/Controller/Distribute/test/unit/buffer.cxx

CMakeFiles/RaftTest.dir/unit/buffer.cxx.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/RaftTest.dir/unit/buffer.cxx.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/virtualplatform/SDN/Controller/Distribute/test/unit/buffer.cxx > CMakeFiles/RaftTest.dir/unit/buffer.cxx.i

CMakeFiles/RaftTest.dir/unit/buffer.cxx.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/RaftTest.dir/unit/buffer.cxx.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/virtualplatform/SDN/Controller/Distribute/test/unit/buffer.cxx -o CMakeFiles/RaftTest.dir/unit/buffer.cxx.s

# Object files for target RaftTest
RaftTest_OBJECTS = \
"CMakeFiles/RaftTest.dir/unit/buffer.cxx.o"

# External object files for target RaftTest
RaftTest_EXTERNAL_OBJECTS =

RaftTest: CMakeFiles/RaftTest.dir/unit/buffer.cxx.o
RaftTest: CMakeFiles/RaftTest.dir/build.make
RaftTest: /home/virtualplatform/SDN/Controller/Distribute/test/libRaftLib.so
RaftTest: CMakeFiles/RaftTest.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/virtualplatform/SDN/Controller/Distribute/test/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable RaftTest"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/RaftTest.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/RaftTest.dir/build: RaftTest
.PHONY : CMakeFiles/RaftTest.dir/build

CMakeFiles/RaftTest.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/RaftTest.dir/cmake_clean.cmake
.PHONY : CMakeFiles/RaftTest.dir/clean

CMakeFiles/RaftTest.dir/depend:
	cd /home/virtualplatform/SDN/Controller/Distribute/test/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/virtualplatform/SDN/Controller/Distribute/test /home/virtualplatform/SDN/Controller/Distribute/test /home/virtualplatform/SDN/Controller/Distribute/test/build /home/virtualplatform/SDN/Controller/Distribute/test/build /home/virtualplatform/SDN/Controller/Distribute/test/build/CMakeFiles/RaftTest.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/RaftTest.dir/depend


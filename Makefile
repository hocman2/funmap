CC := clang
CXXFLAGS := -std=c++23 -Wall -Wextra -Wno-missing-field-initializers -O0 -g
FRAMEWORKS := -framework Cocoa -framework IOKit -framework OpenGL 
INCLUDE_DIRS := -I./include -I./raylib/build/raylib/include 

SRCS = src/osmraylib.cc src/map_data.cc src/earcut.cc src/tinyxml2.cpp
INCS = include/map_data.hpp include/earcut.hpp
OBJS = obj/osmraylib.o obj/map_data.o obj/earcut.o obj/tinyxml2.o

osmraylib: $(OBJS)
	$(CC) $(CXXFLAGS) -lc++ -lcurl $(FRAMEWORKS) ./raylib/build/raylib/libraylib.a $(OBJS) -o osmraylib

obj/osmraylib.o: $(SRCS) $(INCS)
	$(CC) $(CXXFLAGS) $(INCLUDE_DIRS) -c src/osmraylib.cc -o obj/osmraylib.o

obj/map_data.o: src/map_data.cc include/map_data.hpp
	$(CC) $(CXXFLAGS) $(INCLUDE_DIRS) -c src/map_data.cc -o obj/map_data.o

obj/earcut.o: src/earcut.cc include/earcut.hpp src/map_data.cc include/map_data.hpp
	$(CC) $(CXXFLAGS) $(INCLUDE_DIRS) -c src/earcut.cc -o obj/earcut.o

obj/tinyxml2.o: src/tinyxml2.cpp
	$(CC) $(CXXFLAGS) $(INCLUDE_DIRS) -c src/tinyxml2.cpp -o obj/tinyxml2.o

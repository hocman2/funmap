#!/bin/sh

#Generate a tagfile (for usage in vim for instance) by:
#1. Fetching all source files in relevant places
#2. calling ctags with the generated file list

fd -e cc -e hpp -e cpp -e h . src include > tag_file_list.txt
fd -e c -e h -d 1 . raylib/src >> tag_file_list.txt

ctags --kinds-c++=+p --language-force=C++ -f tags -R -L tag_file_list.txt 

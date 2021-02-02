cd '/opt/SqliteMemory'
/usr/bin/make -f Makefile CONF=Debug clean
"/usr/bin/make" -f nbproject/Makefile-Debug.mk QMAKE= SUBPROJECTS= .clean-conf

rm -f -r build/Debug

cd '/opt/SqliteMemory'
/usr/bin/make -f Makefile CONF=Debug
"/usr/bin/make" -f nbproject/Makefile-Debug.mk QMAKE= SUBPROJECTS= .build-conf

"/usr/bin/make"  -f nbproject/Makefile-Debug.mk dist/Debug/GNU-Linux/sqlitememory

mkdir -p build/Debug/GNU-Linux/include
rm -f "build/Debug/GNU-Linux/include/buf.o.d"
gcc    -c -g -MMD -MP -MF "build/Debug/GNU-Linux/include/buf.o.d" -o build/Debug/GNU-Linux/include/buf.o include/buf.c
mkdir -p build/Debug/GNU-Linux/include
rm -f "build/Debug/GNU-Linux/include/http_parser.o.d"
gcc    -c -g -MMD -MP -MF "build/Debug/GNU-Linux/include/http_parser.o.d" -o build/Debug/GNU-Linux/include/http_parser.o include/http_parser.c
mkdir -p build/Debug/GNU-Linux/include/inih/cpp
rm -f "build/Debug/GNU-Linux/include/inih/cpp/INIReader.o.d"
g++    -c -g -std=c++11 -MMD -MP -MF "build/Debug/GNU-Linux/include/inih/cpp/INIReader.o.d" -o build/Debug/GNU-Linux/include/inih/cpp/INIReader.o include/inih/cpp/INIReader.cpp
mkdir -p build/Debug/GNU-Linux/include/inih
rm -f "build/Debug/GNU-Linux/include/inih/ini.o.d"
gcc    -c -g -MMD -MP -MF "build/Debug/GNU-Linux/include/inih/ini.o.d" -o build/Debug/GNU-Linux/include/inih/ini.o include/inih/ini.c
mkdir -p build/Debug/GNU-Linux/include
rm -f "build/Debug/GNU-Linux/include/jsmn.o.d"
gcc    -c -g -MMD -MP -MF "build/Debug/GNU-Linux/include/jsmn.o.d" -o build/Debug/GNU-Linux/include/jsmn.o include/jsmn.c
mkdir -p build/Debug/GNU-Linux/include
rm -f "build/Debug/GNU-Linux/include/sqlite3.o.d"
gcc    -c -g -MMD -MP -MF "build/Debug/GNU-Linux/include/sqlite3.o.d" -o build/Debug/GNU-Linux/include/sqlite3.o include/sqlite3.c
mkdir -p build/Debug/GNU-Linux
rm -f "build/Debug/GNU-Linux/main.o.d"
g++    -c -g -std=c++11 -MMD -MP -MF "build/Debug/GNU-Linux/main.o.d" -o build/Debug/GNU-Linux/main.o main.cpp

mkdir -p dist/Debug/GNU-Linux
g++     -o dist/Debug/GNU-Linux/sqlitememory build/Debug/GNU-Linux/include/buf.o build/Debug/GNU-Linux/include/http_parser.o build/Debug/GNU-Linux/include/inih/cpp/INIReader.o build/Debug/GNU-Linux/include/inih/ini.o build/Debug/GNU-Linux/include/jsmn.o build/Debug/GNU-Linux/include/sqlite3.o build/Debug/GNU-Linux/main.o  -pthread -ldl -licui18n -licuuc -licudata -licuio


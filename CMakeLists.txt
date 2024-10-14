cmake_minimum_required(VERSION 3.29)
project(brainzip C)

set(CMAKE_C_STANDARD 11)

add_executable(brainzip main.c
        brainfuck.h
        brainfuck.c
        zip.h
        zip.c)

#ifndef BRAINFUCK_H
#define BRAINFUCK_H

char* toBrainfuck(const unsigned char* data, size_t length);
unsigned char* fromBrainfuck(const char* input, size_t* output_length);

#endif //BRAINFUCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "brainfuck.h"

#define CELL_SIZE 30000

char* toBrainfuck(const unsigned char* data, size_t length) {
    size_t max_size = length * 20; // Taille estimée
    char* bf_code = (char*)malloc(max_size * sizeof(char));
    if (!bf_code) {
        fprintf(stderr, "Erreur d'allocation mémoire\n");
        return NULL;
    }
    size_t bf_index = 0;

    unsigned char current_value = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char target_value = data[i];
        int diff = target_value - current_value;

        // Réinitialiser la cellule si nécessaire
        if (abs(diff) > 10) {
            if (bf_index + 3 >= max_size) {
                max_size *= 2;
                bf_code = (char*)realloc(bf_code, max_size);
                if (!bf_code) {
                    fprintf(stderr, "Erreur de réallocation mémoire\n");
                    return NULL;
                }
            }
            bf_code[bf_index++] = '[';
            bf_code[bf_index++] = '-';
            bf_code[bf_index++] = ']';
            current_value = 0;
            diff = target_value;
        }

        // Générer les '+' ou '-'
        char op = (diff > 0) ? '+' : '-';
        int count = abs(diff);

        if (bf_index + count >= max_size) {
            max_size += count * 2;
            bf_code = (char*)realloc(bf_code, max_size);
            if (!bf_code) {
                fprintf(stderr, "Erreur de réallocation mémoire\n");
                return NULL;
            }
        }

        for (int j = 0; j < count; j++) {
            bf_code[bf_index++] = op;
        }

        // Ajouter '.'
        if (bf_index + 1 >= max_size) {
            max_size *= 2;
            bf_code = (char*)realloc(bf_code, max_size);
            if (!bf_code) {
                fprintf(stderr, "Erreur de réallocation mémoire\n");
                return NULL;
            }
        }
        bf_code[bf_index++] = '.';
        current_value = target_value;
    }

    // Terminer la chaîne
    if (bf_index >= max_size) {
        max_size += 1;
        bf_code = (char*)realloc(bf_code, max_size);
        if (!bf_code) {
            fprintf(stderr, "Erreur de réallocation mémoire\n");
            return NULL;
        }
    }
    bf_code[bf_index] = '\0';

    return bf_code;
}

unsigned char* fromBrainfuck(const char* input, size_t* output_length) {
    size_t size = strlen(input);
    unsigned char cells[CELL_SIZE] = {0};
    size_t index = 0;
    size_t code_ptr = 0;

    size_t output_size = size;
    unsigned char* output = (unsigned char*)malloc(output_size * sizeof(unsigned char));
    if (!output) {
        fprintf(stderr, "Erreur d'allocation mémoire pour le tampon de sortie\n");
        return NULL;
    }
    size_t output_index = 0;

    while (code_ptr < size) {
        char c = input[code_ptr];

        switch (c) {
            case '>':
                index++;
                if (index >= CELL_SIZE) {
                    fprintf(stderr, "Erreur : Dépassement de la mémoire à droite\n");
                    free(output);
                    return NULL;
                }
                break;
            case '<':
                if (index == 0) {
                    fprintf(stderr, "Erreur : Dépassement de la mémoire à gauche\n");
                    free(output);
                    return NULL;
                }
                index--;
                break;
            case '+':
                cells[index]++;
                break;
            case '-':
                cells[index]--;
                break;
            case '.':
                if (output_index >= output_size) {
                    output_size *= 2;
                    unsigned char* temp = (unsigned char*)realloc(output, output_size * sizeof(unsigned char));
                    if (!temp) {
                        fprintf(stderr, "Erreur de réallocation de mémoire pour le tampon de sortie\n");
                        free(output);
                        return NULL;
                    }
                    output = temp;
                }
                output[output_index++] = cells[index];
                break;
            case ',':
                cells[index] = 0;
                break;
            case '[':
                if (cells[index] == 0) {
                    int loop = 1;
                    while (loop > 0) {
                        code_ptr++;
                        if (code_ptr >= size) {
                            fprintf(stderr, "Erreur : '[' non apparié\n");
                            free(output);
                            return NULL;
                        }
                        if (input[code_ptr] == '[') loop++;
                        else if (input[code_ptr] == ']') loop--;
                    }
                }
                break;
            case ']':
                if (cells[index] != 0) {
                    int loop = 1;
                    code_ptr--;
                    while (loop > 0) {
                        if (code_ptr == 0) {
                            fprintf(stderr, "Erreur : ']' non apparié\n");
                            free(output);
                            return NULL;
                        }
                        if (input[code_ptr] == ']') loop++;
                        else if (input[code_ptr] == '[') loop--;
                        code_ptr--;
                    }
                    code_ptr++;
                }
                break;
            default:
                break;
        }
        code_ptr++;
    }

    if (output_length) {
        *output_length = output_index;
    }

    unsigned char* final_output = (unsigned char*)realloc(output, output_index * sizeof(unsigned char));
    if (!final_output) {
        return output;
    }

    return final_output;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "zip.h"

#include "brainfuck.h"

#define BUFFER_SIZE 1024

typedef struct {
    char* path;
    int is_directory;
} FileInfo;

FileInfo* files = NULL;
size_t file_count = 0;
size_t file_capacity = 0;

void collectFiles(const char* base_path, const char* relative_path) {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, relative_path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        fprintf(stderr, "Erreur : Impossible d'accéder à %s\n", full_path);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        if (file_count >= file_capacity) {
            file_capacity = (file_capacity == 0) ? 10 : file_capacity * 2;
            files = realloc(files, file_capacity * sizeof(FileInfo));
        }
        files[file_count].path = strdup(relative_path);
        files[file_count].is_directory = 1;
        file_count++;

        DIR* dir = opendir(full_path);
        if (!dir) {
            fprintf(stderr, "Erreur : Impossible d'ouvrir le dossier %s\n", full_path);
            return;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char child_relative_path[PATH_MAX];
            snprintf(child_relative_path, sizeof(child_relative_path), "%s/%s", relative_path, entry->d_name);

            collectFiles(base_path, child_relative_path);
        }
        closedir(dir);
    } else if (S_ISREG(st.st_mode)) {
        if (file_count >= file_capacity) {
            file_capacity = (file_capacity == 0) ? 10 : file_capacity * 2;
            files = realloc(files, file_capacity * sizeof(FileInfo));
        }
        files[file_count].path = strdup(relative_path);
        files[file_count].is_directory = 0;
        file_count++;
    }
}

int compressFiles(const char* output_filename, const char** input_paths, int path_count) {
    files = NULL;
    file_count = 0;
    file_capacity = 0;

    for (int i = 0; i < path_count; i++) {
        const char* path = input_paths[i];
        struct stat st;
        if (stat(path, &st) != 0) {
            fprintf(stderr, "Erreur : Impossible d'accéder à %s\n", path);
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            collectFiles(path, "");
        } else if (S_ISREG(st.st_mode)) {
            if (file_count >= file_capacity) {
                file_capacity = (file_capacity == 0) ? 10 : file_capacity * 2;
                files = realloc(files, file_capacity * sizeof(FileInfo));
            }
            files[file_count].path = strdup(path);
            files[file_count].is_directory = 0;
            file_count++;
        }
    }

    FILE* output_file = fopen(output_filename, "wb");
    if (!output_file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier de sortie %s\n", output_filename);
        return -1;
    }

    fprintf(output_file, "BrainZip Archive\n");

    fprintf(output_file, "FileCount:%zu\n", file_count);
    for (size_t i = 0; i < file_count; i++) {
        FileInfo* fi = &files[i];
        fprintf(output_file, "Entry:%s;Type:%s\n", fi->path, fi->is_directory ? "DIR" : "FILE");
    }
    fprintf(output_file, "EndMetadata\n");

    for (size_t i = 0; i < file_count; i++) {
        FileInfo* fi = &files[i];
        if (fi->is_directory) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s", fi->path);

        FILE* input_file = fopen(full_path, "rb");
        if (!input_file) {
            fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", full_path);
            fclose(output_file);
            return -1;
        }

        fseek(input_file, 0, SEEK_END);
        size_t filesize = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);

        unsigned char* data = (unsigned char*)malloc(filesize);
        if (!data) {
            fprintf(stderr, "Erreur d'allocation mémoire pour le fichier %s\n", full_path);
            fclose(input_file);
            fclose(output_file);
            return -1;
        }

        fread(data, 1, filesize, input_file);
        fclose(input_file);

        char* bf_code = toBrainfuck(data, filesize);
        free(data);

        if (!bf_code) {
            fprintf(stderr, "Erreur lors de la conversion en Brainfuck du fichier %s\n", full_path);
            fclose(output_file);
            return -1;
        }

        fprintf(output_file, "StartFile:%s\n", fi->path);
        fprintf(output_file, "%s\n", bf_code);
        fprintf(output_file, "EndFile\n");

        free(bf_code);
    }

    fclose(output_file);

    for (size_t i = 0; i < file_count; i++) {
        free(files[i].path);
    }
    free(files);

    return 0;
}

int decompressFile(const char* input_filename) {
    FILE* input_file = fopen(input_filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", input_filename);
        return -1;
    }

    char line[BUFFER_SIZE];
    size_t entry_count = 0;

    if (!fgets(line, BUFFER_SIZE, input_file) || strcmp(line, "BrainZip Archive\n") != 0) {
        fprintf(stderr, "Erreur : Fichier d'archive invalide\n");
        fclose(input_file);
        return -1;
    }

    if (fgets(line, BUFFER_SIZE, input_file) && sscanf(line, "FileCount:%zu", &entry_count) != 1) {
        fprintf(stderr, "Erreur : Nombre d'entrées invalide\n");
        fclose(input_file);
        return -1;
    }

    FileInfo* entries = (FileInfo*)malloc(entry_count * sizeof(FileInfo));
    if (!entries) {
        fprintf(stderr, "Erreur d'allocation mémoire\n");
        fclose(input_file);
        return -1;
    }

    for (size_t i = 0; i < entry_count; i++) {
        if (!fgets(line, BUFFER_SIZE, input_file)) {
            fprintf(stderr, "Erreur : Lecture des métadonnées échouée\n");
            fclose(input_file);
            return -1;
        }

        char path[BUFFER_SIZE];
        char type[10];
        if (sscanf(line, "Entry:%[^;];Type:%s", path, type) != 2) {
            fprintf(stderr, "Erreur : Métadonnées de l'entrée invalide\n");
            fclose(input_file);
            return -1;
        }
        entries[i].path = strdup(path);
        entries[i].is_directory = (strcmp(type, "DIR") == 0) ? 1 : 0;
    }

    if (!fgets(line, BUFFER_SIZE, input_file) || strcmp(line, "EndMetadata\n") != 0) {
        fprintf(stderr, "Erreur : Fin des métadonnées non trouvée\n");
        fclose(input_file);
        return -1;
    }

    for (size_t i = 0; i < entry_count; i++) {
        FileInfo* fi = &entries[i];
        if (fi->is_directory) {
            // Créer le dossier
            if (mkdir(fi->path) != 0) {
            }
        }
    }

    for (size_t i = 0; i < entry_count; i++) {
        FileInfo* fi = &entries[i];
        if (fi->is_directory) {
            continue;
        }

        while (fgets(line, BUFFER_SIZE, input_file) && strncmp(line, "StartFile:", 10) != 0);

        if (feof(input_file)) {
            fprintf(stderr, "Erreur : StartFile non trouvé pour %s\n", fi->path);
            fclose(input_file);
            return -1;
        }

        char start_filename[BUFFER_SIZE];
        sscanf(line, "StartFile:%[^\n]", start_filename);

        if (strcmp(start_filename, fi->path) != 0) {
            fprintf(stderr, "Erreur : Nom de fichier non correspondant (%s vs %s)\n", start_filename, fi->path);
            fclose(input_file);
            return -1;
        }

        char* bf_code = NULL;
        size_t bf_code_size = 0;
        size_t bf_code_capacity = BUFFER_SIZE;
        bf_code = (char*)malloc(bf_code_capacity * sizeof(char));
        if (!bf_code) {
            fprintf(stderr, "Erreur d'allocation mémoire pour le code Brainfuck\n");
            fclose(input_file);
            return -1;
        }
        bf_code[0] = '\0';

        while (fgets(line, BUFFER_SIZE, input_file) && strcmp(line, "EndFile\n") != 0) {
            size_t line_length = strlen(line);
            if (bf_code_size + line_length >= bf_code_capacity) {
                bf_code_capacity *= 2;
                char* temp = (char*)realloc(bf_code, bf_code_capacity * sizeof(char));
                if (!temp) {
                    fprintf(stderr, "Erreur de réallocation mémoire pour le code Brainfuck\n");
                    free(bf_code);
                    fclose(input_file);
                    return -1;
                }
                bf_code = temp;
            }
            strcat(bf_code, line);
            bf_code_size += line_length;
        }

        if (feof(input_file)) {
            fprintf(stderr, "Erreur : EndFile non trouvé pour %s\n", fi->path);
            free(bf_code);
            fclose(input_file);
            return -1;
        }

        size_t output_length = 0;
        unsigned char* data = fromBrainfuck(bf_code, &output_length);
        free(bf_code);

        if (!data) {
            fprintf(stderr, "Erreur lors de l'interprétation du code Brainfuck pour %s\n", fi->path);
            fclose(input_file);
            return -1;
        }

        char* dir_path = strdup(fi->path);
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            mkdir(dir_path);
        }
        free(dir_path);

        FILE* output_file = fopen(fi->path, "wb");
        if (!output_file) {
            fprintf(stderr, "Erreur : Impossible de créer le fichier %s\n", fi->path);
            free(data);
            fclose(input_file);
            return -1;
        }

        fwrite(data, 1, output_length, output_file);
        fclose(output_file);
        free(data);
    }

    fclose(input_file);

    for (size_t i = 0; i < entry_count; i++) {
        free(entries[i].path);
    }
    free(entries);

    return 0;
}
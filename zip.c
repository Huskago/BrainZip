#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>   // Pour les mesures de performance
#include "zip.h"

#include "brainfuck.h"

#define BUFFER_SIZE 8192  // Augmenté pour améliorer les performances d'I/O
#define PROGRESS_BAR_WIDTH 50

// Cross-platform mkdir
#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#define PATH_SEPARATOR "\\"
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#define PATH_SEPARATOR "/"
#endif

typedef struct {
    char* path;
    int is_directory;
    size_t size;  // Ajouté pour suivre la taille totale pour la barre de progression
} FileInfo;

FileInfo* files = NULL;
size_t file_count = 0;
size_t file_capacity = 0;
size_t total_bytes = 0;  // Pour suivre la taille totale des fichiers

// Affiche une barre de progression
void print_progress_bar(size_t current, size_t total) {
    float percentage = (float)current / total;
    int pos = PROGRESS_BAR_WIDTH * percentage;
    
    printf("\r[");
    for (int i = 0; i < PROGRESS_BAR_WIDTH; i++) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %.1f%% (%zu/%zu)", percentage * 100, current, total);
    fflush(stdout);
}

void collectFiles(const char* base_path, const char* relative_path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, relative_path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        fprintf(stderr, "Erreur : Impossible d'accéder à %s\n", full_path);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        if (file_count >= file_capacity) {
            file_capacity = (file_capacity == 0) ? 16 : file_capacity * 2;
            files = realloc(files, file_capacity * sizeof(FileInfo));
            if (!files) {
                fprintf(stderr, "Erreur d'allocation mémoire\n");
                exit(1);
            }
        }
        files[file_count].path = strdup(relative_path);
        files[file_count].is_directory = 1;
        files[file_count].size = 0;
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

            char child_relative_path[BUFFER_SIZE];
            snprintf(child_relative_path, sizeof(child_relative_path), "%s/%s", 
                     relative_path[0] ? relative_path : "", entry->d_name);

            collectFiles(base_path, child_relative_path);
        }
        closedir(dir);
    } else if (S_ISREG(st.st_mode)) {
        if (file_count >= file_capacity) {
            file_capacity = (file_capacity == 0) ? 16 : file_capacity * 2;
            files = realloc(files, file_capacity * sizeof(FileInfo));
            if (!files) {
                fprintf(stderr, "Erreur d'allocation mémoire\n");
                exit(1);
            }
        }
        files[file_count].path = strdup(relative_path);
        files[file_count].is_directory = 0;
        files[file_count].size = st.st_size;
        total_bytes += st.st_size;
        file_count++;
    }
}

int compressFiles(const char* output_filename, const char** input_paths, int path_count) {
    clock_t start = clock();
    files = NULL;
    file_count = 0;
    file_capacity = 0;
    total_bytes = 0;
    
    printf("Analyse des fichiers...\n");
    
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
                file_capacity = (file_capacity == 0) ? 16 : file_capacity * 2;
                files = realloc(files, file_capacity * sizeof(FileInfo));
                if (!files) {
                    fprintf(stderr, "Erreur d'allocation mémoire\n");
                    return -1;
                }
            }
            files[file_count].path = strdup(path);
            files[file_count].is_directory = 0;
            files[file_count].size = st.st_size;
            total_bytes += st.st_size;
            file_count++;
        }
    }

    printf("Compression de %zu fichiers (%zu octets)...\n", file_count, total_bytes);

    FILE* output_file = fopen(output_filename, "wb");
    if (!output_file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier de sortie %s\n", output_filename);
        return -1;
    }

    fprintf(output_file, "BrainZip Archive\n");
    fprintf(output_file, "FileCount:%zu\n", file_count);
    
    // Écrire les métadonnées
    for (size_t i = 0; i < file_count; i++) {
        FileInfo* fi = &files[i];
        fprintf(output_file, "Entry:%s;Type:%s;Size:%zu\n", 
                fi->path, fi->is_directory ? "DIR" : "FILE", fi->size);
    }
    fprintf(output_file, "EndMetadata\n");

    // Compression des fichiers
    size_t processed_bytes = 0;
    for (size_t i = 0; i < file_count; i++) {
        FileInfo* fi = &files[i];
        if (fi->is_directory) {
            continue;
        }

        print_progress_bar(processed_bytes, total_bytes);

        char full_path[BUFFER_SIZE];
        snprintf(full_path, sizeof(full_path), "%s", fi->path);

        FILE* input_file = fopen(full_path, "rb");
        if (!input_file) {
            fprintf(stderr, "\nErreur : Impossible d'ouvrir le fichier %s\n", full_path);
            fclose(output_file);
            return -1;
        }

        // Utiliser la taille déjà connue du fichier au lieu de ftell
        size_t filesize = fi->size;
        unsigned char* data = (unsigned char*)malloc(filesize);
        if (!data) {
            fprintf(stderr, "\nErreur d'allocation mémoire pour le fichier %s\n", full_path);
            fclose(input_file);
            fclose(output_file);
            return -1;
        }

        size_t bytesRead = fread(data, 1, filesize, input_file);
        fclose(input_file);
        
        if (bytesRead != filesize) {
            fprintf(stderr, "\nErreur de lecture du fichier %s\n", full_path);
            free(data);
            fclose(output_file);
            return -1;
        }

        char* bf_code = toBrainfuck(data, filesize);
        free(data);

        if (!bf_code) {
            fprintf(stderr, "\nErreur lors de la conversion en Brainfuck du fichier %s\n", full_path);
            fclose(output_file);
            return -1;
        }

        fprintf(output_file, "StartFile:%s\n", fi->path);
        fprintf(output_file, "%s\n", bf_code);
        fprintf(output_file, "EndFile\n");

        free(bf_code);
        processed_bytes += filesize;
    }
    
    print_progress_bar(total_bytes, total_bytes);
    printf("\nCompression terminée!\n");

    fclose(output_file);

    for (size_t i = 0; i < file_count; i++) {
        free(files[i].path);
    }
    free(files);
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Temps total: %.2f secondes\n", elapsed);
    
    if (total_bytes > 0) {
        printf("Vitesse moyenne: %.2f MiB/s\n", (total_bytes / (1024.0 * 1024.0)) / elapsed);
    }

    return 0;
}

// Create directory recursively
void create_directory(const char* path) {
    char temp[BUFFER_SIZE];
    char* p = NULL;
    size_t len;
    
    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    
    // Remove trailing slash if exists
    if (len > 0 && (temp[len - 1] == '/' || temp[len - 1] == '\\')) {
        temp[len - 1] = 0;
    }
    
    // Create parent directories
    for (p = temp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            MKDIR(temp);
            *p = PATH_SEPARATOR[0];
        }
    }
    
    // Create the final directory
    MKDIR(temp);
}

// Déterminer la taille totale du contenu Brainfuck dans le fichier
size_t calculate_total_brainfuck_size(FILE* file) {
    size_t total_size = 0;
    size_t current_pos = ftell(file);
    char line[BUFFER_SIZE];
    
    while (fgets(line, BUFFER_SIZE, file)) {
        if (strncmp(line, "StartFile:", 10) == 0) {
            // Ignorer la ligne StartFile
            continue;
        } else if (strncmp(line, "EndFile", 7) == 0) {
            // Fin d'un fichier
            continue;
        } else if (strncmp(line, "EndMetadata", 11) == 0) {
            // Fin des métadonnées, commencer à compter
            continue;
        } else {
            total_size += strlen(line);
        }
    }
    
    // Retourner à la position originale
    fseek(file, current_pos, SEEK_SET);
    return total_size;
}

int decompressFile(const char* input_filename) {
    clock_t start = clock();
    FILE* input_file = fopen(input_filename, "rb");
    if (!input_file) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s\n", input_filename);
        return -1;
    }

    char line[BUFFER_SIZE];
    size_t entry_count = 0;
    size_t total_processed = 0;

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

    printf("Archive contenant %zu entrées\n", entry_count);
    
    FileInfo* entries = (FileInfo*)malloc(entry_count * sizeof(FileInfo));
    if (!entries) {
        fprintf(stderr, "Erreur d'allocation mémoire\n");
        fclose(input_file);
        return -1;
    }

    // Lire les métadonnées
    size_t total_size = 0;
    for (size_t i = 0; i < entry_count; i++) {
        if (!fgets(line, BUFFER_SIZE, input_file)) {
            fprintf(stderr, "Erreur : Lecture des métadonnées échouée\n");
            fclose(input_file);
            return -1;
        }

        char path[BUFFER_SIZE];
        char type[10];
        size_t file_size = 0;
        
        // Format mis à jour pour inclure la taille
        if (sscanf(line, "Entry:%[^;];Type:%[^;];Size:%zu", path, type, &file_size) != 3) {
            // Fallback pour la compatibilité avec l'ancien format
            if (sscanf(line, "Entry:%[^;];Type:%s", path, type) != 2) {
                fprintf(stderr, "Erreur : Métadonnées de l'entrée invalide\n");
                fclose(input_file);
                return -1;
            }
        }
        
        entries[i].path = strdup(path);
        entries[i].is_directory = (strcmp(type, "DIR") == 0) ? 1 : 0;
        entries[i].size = file_size;
        
        if (!entries[i].is_directory) {
            total_size += file_size;
        }
    }

    if (!fgets(line, BUFFER_SIZE, input_file) || strcmp(line, "EndMetadata\n") != 0) {
        fprintf(stderr, "Erreur : Fin des métadonnées non trouvée\n");
        fclose(input_file);
        return -1;
    }

    printf("Taille totale des données: %zu octets\n", total_size);
    
    // Si la taille totale n'est pas disponible dans les métadonnées
    if (total_size == 0) {
        printf("Calcul de la taille des données...\n");
        total_size = calculate_total_brainfuck_size(input_file);
        // Retourner après EndMetadata
        fseek(input_file, 0, SEEK_SET);
        while (fgets(line, BUFFER_SIZE, input_file) && strcmp(line, "EndMetadata\n") != 0);
    }
    
    printf("Création des dossiers...\n");
    // Créer d'abord tous les dossiers
    for (size_t i = 0; i < entry_count; i++) {
        FileInfo* fi = &entries[i];
        if (fi->is_directory) {
            create_directory(fi->path);
        }
    }

    printf("Décompression des fichiers...\n");
    
    // Ensuite extraire les fichiers
    for (size_t i = 0; i < entry_count; i++) {
        FileInfo* fi = &entries[i];
        if (fi->is_directory) {
            continue;
        }

        // Rechercher le début du fichier
        while (fgets(line, BUFFER_SIZE, input_file) && strncmp(line, "StartFile:", 10) != 0);

        if (feof(input_file)) {
            fprintf(stderr, "\nErreur : StartFile non trouvé pour %s\n", fi->path);
            fclose(input_file);
            return -1;
        }

        char start_filename[BUFFER_SIZE];
        sscanf(line, "StartFile:%[^\n]", start_filename);

        if (strcmp(start_filename, fi->path) != 0) {
            fprintf(stderr, "\nErreur : Nom de fichier non correspondant (%s vs %s)\n", start_filename, fi->path);
            fclose(input_file);
            return -1;
        }

        // Lire le code Brainfuck
        char* bf_code = NULL;
        size_t bf_code_size = 0;
        size_t bf_code_capacity = BUFFER_SIZE * 4;  // Plus grand buffer initial
        bf_code = (char*)malloc(bf_code_capacity * sizeof(char));
        if (!bf_code) {
            fprintf(stderr, "\nErreur d'allocation mémoire pour le code Brainfuck\n");
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
                    fprintf(stderr, "\nErreur de réallocation mémoire pour le code Brainfuck\n");
                    free(bf_code);
                    fclose(input_file);
                    return -1;
                }
                bf_code = temp;
            }
            strcat(bf_code, line);
            bf_code_size += line_length;
            
            // Mise à jour de la barre de progression
            total_processed += line_length;
            print_progress_bar(total_processed, total_size);
        }

        if (feof(input_file)) {
            fprintf(stderr, "\nErreur : EndFile non trouvé pour %s\n", fi->path);
            free(bf_code);
            fclose(input_file);
            return -1;
        }

        // Interpréter le code Brainfuck
        size_t output_length = 0;
        unsigned char* data = fromBrainfuck(bf_code, &output_length);
        free(bf_code);

        if (!data) {
            fprintf(stderr, "\nErreur lors de l'interprétation du code Brainfuck pour %s\n", fi->path);
            fclose(input_file);
            return -1;
        }

        // Créer le dossier parent si nécessaire
        char* dir_path = strdup(fi->path);
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            create_directory(dir_path);
        }
        free(dir_path);

        // Écrire le fichier extrait
        FILE* output_file = fopen(fi->path, "wb");
        if (!output_file) {
            fprintf(stderr, "\nErreur : Impossible de créer le fichier %s\n", fi->path);
            free(data);
            fclose(input_file);
            return -1;
        }

        fwrite(data, 1, output_length, output_file);
        fclose(output_file);
        free(data);
    }

    print_progress_bar(total_size, total_size);
    printf("\nDécompression terminée!\n");
    
    fclose(input_file);

    for (size_t i = 0; i < entry_count; i++) {
        free(entries[i].path);
    }
    free(entries);
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Temps total: %.2f secondes\n", elapsed);
    
    if (total_size > 0) {
        printf("Vitesse moyenne: %.2f MiB/s\n", (total_size / (1024.0 * 1024.0)) / elapsed);
    }

    return 0;
}

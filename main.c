#include <stdio.h>
#include <string.h>
#include "zip.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Utilisation :\n");
        printf("Pour compresser : %s compress archive.bfz chemin1 [chemin2 ...]\n", argv[0]);
        printf("Pour décompresser : %s decompress archive.bfz\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "compress") == 0) {
        const char* output_filename = argv[2];
        const char** input_paths = (const char**)&argv[3];
        int path_count = argc - 3;
        if (path_count <= 0) {
            fprintf(stderr, "Erreur : Aucun fichier ou dossier spécifié pour la compression\n");
            return 1;
        }
        return compressFiles(output_filename, input_paths, path_count);
    } else if (strcmp(argv[1], "decompress") == 0) {
        const char* input_filename = argv[2];
        return decompressFile(input_filename);
    } else {
        fprintf(stderr, "Erreur : Commande inconnue %s\n", argv[1]);
        return 1;
    }

    return 0;
}

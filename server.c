/**
 * ftp_curl_final.c
 * Baixa os 10 maiores PDFs recursivamente do FTP.
 * Compilar: gcc -o ftp_top10.exe ftp_curl_final.c -lcurl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#define MKDIR(path) CreateDirectoryA(path, NULL)
#define strcasecmp _stricmp
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/* Estruturas */
typedef struct {
    char *full_path;   /* caminho completo no FTP (com barra no início) */
    char *name;
    double size;
} FileInfo;

FileInfo *pdf_list = NULL;
size_t pdf_count = 0;

typedef struct {
    char host[256];
    int  port;
    char user[128];
    char pass[128];
    char destino[512];
} Config;

Config cfg;

/* Callback para escrita em memória */
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

/* Callback para salvar arquivo */
static size_t WriteFileCallback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

/* Leitura do .env */
bool parse_env(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        char key[128], value[512];
        if (sscanf(line, "%127[^=]=%511[^\n]", key, value) == 2) {
            if (strcmp(key, "host") == 0) strcpy(cfg.host, value);
            else if (strcmp(key, "port") == 0) cfg.port = atoi(value);
            else if (strcmp(key, "usr") == 0) strcpy(cfg.user, value);
            else if (strcmp(key, "pws") == 0) strcpy(cfg.pass, value);
            else if (strcmp(key, "destino") == 0) strcpy(cfg.destino, value);
        }
    }
    fclose(f);
    return true;
}

bool is_pdf(const char *name) {
    size_t len = strlen(name);
    return (len > 4 && strcasecmp(name + len - 4, ".pdf") == 0);
}

void add_pdf(const char *full_path, const char *name, double size) {
    pdf_list = realloc(pdf_list, (pdf_count + 1) * sizeof(FileInfo));
    pdf_list[pdf_count].full_path = strdup(full_path);
    pdf_list[pdf_count].name = strdup(name);
    pdf_list[pdf_count].size = size;
    pdf_count++;
}

int compare_size_desc(const void *a, const void *b) {
    const FileInfo *fa = (const FileInfo *)a;
    const FileInfo *fb = (const FileInfo *)b;
    if (fa->size < fb->size) return 1;
    if (fa->size > fb->size) return -1;
    return 0;
}

/* Garante que o caminho termine com '/' se for diretório */
void ensure_trailing_slash(char *path, size_t size) {
    size_t len = strlen(path);
    if (len > 0 && path[len-1] != '/' && len < size-1) {
        path[len] = '/';
        path[len+1] = '\0';
    }
}

/* Processa a resposta do comando LIST */
void process_list(const char *data, const char *current_dir, CURL *curl) {
    char *line = strtok((char *)data, "\r\n");
    while (line) {
        // Ignora linhas que começam com "total"
        if (strncmp(line, "total", 5) == 0) {
            line = strtok(NULL, "\r\n");
            continue;
        }

        // Formato Unix: drwxr-xr-x ... ou -rw-r--r-- ...
        if (strlen(line) > 10 && (line[0] == 'd' || line[0] == '-')) {
            char perm[11], user[64], group[64], month[4], day[3], time_or_year[6], name[1024];
            int links;
            double size;
            int n = sscanf(line, "%10s %d %63s %63s %lf %3s %2s %5s %1023s",
                           perm, &links, user, group, &size, month, day, time_or_year, name);
            if (n >= 9) {
                if (line[0] == 'd' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
                    char new_dir[2048];
                    snprintf(new_dir, sizeof(new_dir), "%s%s/", current_dir, name);
                    // Recursão
                    extern void list_ftp_directory(CURL *curl, const char *path);
                    list_ftp_directory(curl, new_dir);
                } else if (line[0] == '-' && is_pdf(name)) {
                    char full_path[2048];
                    snprintf(full_path, sizeof(full_path), "%s%s", current_dir, name);
                    add_pdf(full_path, name, size);
                }
            }
        }
        line = strtok(NULL, "\r\n");
    }
}

/* Função recursiva que lista um diretório */
void list_ftp_directory(CURL *curl, const char *remote_path) {
    CURLcode res;
    struct MemoryStruct chunk = {0};
    char url[2048];
    char path_with_slash[2048];

    // Garante que o caminho termine com '/' para indicar diretório
    strncpy(path_with_slash, remote_path, sizeof(path_with_slash)-1);
    ensure_trailing_slash(path_with_slash, sizeof(path_with_slash));

    snprintf(url, sizeof(url), "ftp://%s:%d%s", cfg.host, cfg.port, path_with_slash);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "LIST");  // força LIST

    printf("Listando: %s\n", path_with_slash);
    res = curl_easy_perform(curl);

    if (res == CURLE_OK && chunk.size > 0) {
        process_list(chunk.memory, path_with_slash, curl);
    } else {
        fprintf(stderr, "Erro ao listar %s: %s\n", path_with_slash, curl_easy_strerror(res));
    }

    free(chunk.memory);
}

int main(void) {
    if (!parse_env(".env")) {
        fprintf(stderr, "Erro ao ler .env\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) return 1;

    curl_easy_setopt(curl, CURLOPT_USERNAME, cfg.user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg.pass);
    curl_easy_setopt(curl, CURLOPT_PORT, cfg.port);
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 1L);
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // descomente para debug

    printf("Conectado ao FTP %s:%d\n", cfg.host, cfg.port);
    printf("Varrendo recursivamente /FICHATECNICA/ ...\n");

    // Inicia a varredura com barra no final
    list_ftp_directory(curl, "/FICHATECNICA/");

    printf("Total de PDFs encontrados: %zu\n", pdf_count);

    if (pdf_count == 0) {
        printf("Nenhum PDF encontrado.\n");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    }

    qsort(pdf_list, pdf_count, sizeof(FileInfo), compare_size_desc);
    int top_n = (pdf_count < 10) ? (int)pdf_count : 10;
    printf("\n--- Top %d maiores PDFs ---\n", top_n);
    for (int i = 0; i < top_n; i++) {
        printf("%2d. %s (%.0f bytes)\n", i+1, pdf_list[i].name, pdf_list[i].size);
    }

    MKDIR(cfg.destino);

    printf("\nBaixando os %d maiores...\n", top_n);
    for (int i = 0; i < top_n; i++) {
        char local_path[MAX_PATH];
        snprintf(local_path, sizeof(local_path), "%s\\%s", cfg.destino, pdf_list[i].name);
        FILE *out = fopen(local_path, "wb");
        if (!out) {
            fprintf(stderr, "Erro ao criar %s\n", local_path);
            continue;
        }

        // URL do arquivo (sem barra final)
        char url[2048];
        snprintf(url, sizeof(url), "ftp://%s:%d%s", cfg.host, cfg.port, pdf_list[i].full_path);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);

        printf("(%d/%d) %s ... ", i+1, top_n, pdf_list[i].name);
        fflush(stdout);
        CURLcode res = curl_easy_perform(curl);
        fclose(out);
        if (res == CURLE_OK) printf("OK\n");
        else printf("FALHA (%s)\n", curl_easy_strerror(res));
    }

    for (size_t i = 0; i < pdf_count; i++) {
        free(pdf_list[i].full_path);
        free(pdf_list[i].name);
    }
    free(pdf_list);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    printf("Processo concluído.\n");
    return 0;
}
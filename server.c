/**
 * ftp_sync_dirs_fixed.c
 * Sincroniza PDFs do FTP mantendo a estrutura de diretórios.
 * Corrige o problema de recursão com strtok.
 * Compilar: gcc -o ftp_sync_dirs_fixed.exe ftp_sync_dirs_fixed.c -lcurl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>

// Descomente para ver a listagem bruta do FTP
// #define DEBUG 1

#ifdef _WIN32
#include <windows.h>
#include <sys/stat.h>
#define MKDIR(path) CreateDirectoryA(path, NULL)
#define strcasecmp _stricmp
#define stat _stat
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

/* Estruturas */
typedef struct {
    char *full_path;      /* caminho completo no FTP (ex: /FICHATECNICA/214/arq.pdf) */
    char *relative_path;  /* caminho relativo a partir de /FICHATECNICA/ (ex: 214/arq.pdf) */
    char *name;           /* apenas o nome do arquivo */
    double size;
    time_t mtime;
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

/* Gera o caminho relativo a partir do full_path, removendo o prefixo "/FICHATECNICA/" */
void make_relative_path(const char *full_path, char *rel_path, size_t size) {
    const char *prefix = "/FICHATECNICA/";
    if (strncmp(full_path, prefix, strlen(prefix)) == 0) {
        strncpy(rel_path, full_path + strlen(prefix), size);
    } else {
        while (*full_path == '/') full_path++;
        strncpy(rel_path, full_path, size);
    }
    rel_path[size-1] = '\0';
}

void add_pdf(const char *full_path, const char *name, double size, time_t mtime) {
    pdf_list = realloc(pdf_list, (pdf_count + 1) * sizeof(FileInfo));
    pdf_list[pdf_count].full_path = strdup(full_path);
    pdf_list[pdf_count].name = strdup(name);
    pdf_list[pdf_count].size = size;
    pdf_list[pdf_count].mtime = mtime;
    char rel[2048];
    make_relative_path(full_path, rel, sizeof(rel));
    pdf_list[pdf_count].relative_path = strdup(rel);
    pdf_count++;
}

/* Converte string de data do FTP para time_t */
time_t parse_ftp_date(const char *month_str, const char *day_str, const char *time_or_year) {
    struct tm tm = {0};
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = -1;
    for (int i = 0; i < 12; i++) {
        if (strcasecmp(month_str, months[i]) == 0) {
            month = i;
            break;
        }
    }
    if (month == -1) return 0;
    tm.tm_mon = month;
    tm.tm_mday = atoi(day_str);

    if (strchr(time_or_year, ':')) {
        int hour, min;
        sscanf(time_or_year, "%d:%d", &hour, &min);
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = 0;
        time_t now = time(NULL);
        struct tm *now_tm = localtime(&now);
        tm.tm_year = now_tm->tm_year;
        tm.tm_isdst = -1;
    } else {
        int year = atoi(time_or_year);
        tm.tm_year = year - 1900;
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = -1;
    }
    return mktime(&tm);
}

void ensure_trailing_slash(char *path, size_t size) {
    size_t len = strlen(path);
    if (len > 0 && path[len-1] != '/' && len < size-1) {
        path[len] = '/';
        path[len+1] = '\0';
    }
}

/* Cria diretórios recursivamente para um caminho de arquivo (excluindo o nome do arquivo) */
void create_directories_for_file(const char *filepath) {
    char path_copy[2048];
    strncpy(path_copy, filepath, sizeof(path_copy)-1);
    path_copy[sizeof(path_copy)-1] = '\0';
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash == NULL) return;
    *last_slash = '\0';
    char *p = path_copy;
    char current[2048] = "";
    while (*p) {
        char *next = strchr(p, '/');
        if (next) {
            size_t len = next - p;
            strncat(current, p, len);
            MKDIR(current);
            strcat(current, "/");
            p = next + 1;
        } else {
            strcat(current, p);
            MKDIR(current);
            break;
        }
    }
}

/* Divide uma string em linhas (separador \r\n) e retorna um array de ponteiros.
   O array termina com NULL. A memória alocada deve ser liberada com free_lines(). */
static char **split_lines(const char *data, size_t *num_lines) {
    if (!data || !num_lines) return NULL;
    size_t count = 0;
    // Primeira passagem: contar linhas
    const char *ptr = data;
    while (*ptr) {
        if (*ptr == '\r' && *(ptr+1) == '\n') {
            count++;
            ptr += 2;
        } else if (*ptr == '\n') {
            count++;
            ptr++;
        } else {
            ptr++;
        }
    }
    // Aloca array com count+1 (último NULL)
    char **lines = malloc((count + 1) * sizeof(char *));
    if (!lines) return NULL;
    // Segunda passagem: copiar cada linha
    ptr = data;
    size_t idx = 0;
    while (*ptr && idx < count) {
        const char *start = ptr;
        while (*ptr && !(*ptr == '\r' && *(ptr+1) == '\n') && *ptr != '\n') ptr++;
        size_t len = ptr - start;
        char *line = malloc(len + 1);
        if (line) {
            strncpy(line, start, len);
            line[len] = '\0';
            lines[idx++] = line;
        }
        if (*ptr == '\r' && *(ptr+1) == '\n') ptr += 2;
        else if (*ptr == '\n') ptr++;
    }
    lines[idx] = NULL;
    *num_lines = idx;
    return lines;
}

void free_lines(char **lines) {
    if (!lines) return;
    for (size_t i = 0; lines[i]; i++) free(lines[i]);
    free(lines);
}

/* Protótipo para evitar warnings */
void list_ftp_directory(CURL *curl, const char *remote_path);

/* Processa a listagem sem usar strtok recursivamente */
void process_list(const char *data, const char *current_dir, CURL *curl) {
    size_t num_lines = 0;
    char **lines = split_lines(data, &num_lines);
    if (!lines) return;

    for (size_t i = 0; i < num_lines; i++) {
        const char *line = lines[i];
        if (strncmp(line, "total", 5) == 0) continue;
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
                    // Recursão segura (não usa strtok)
                    list_ftp_directory(curl, new_dir);
                } else if (line[0] == '-' && is_pdf(name)) {
                    char full_path[2048];
                    snprintf(full_path, sizeof(full_path), "%s%s", current_dir, name);
                    time_t mtime = parse_ftp_date(month, day, time_or_year);
                    add_pdf(full_path, name, size, mtime);
                }
            }
        }
    }
    free_lines(lines);
}

/* Função recursiva que lista um diretório */
void list_ftp_directory(CURL *curl, const char *remote_path) {
    CURLcode res;
    struct MemoryStruct chunk = {0};
    char url[2048];
    char path_with_slash[2048];

    strncpy(path_with_slash, remote_path, sizeof(path_with_slash)-1);
    ensure_trailing_slash(path_with_slash, sizeof(path_with_slash));

    snprintf(url, sizeof(url), "ftp://%s:%d%s", cfg.host, cfg.port, path_with_slash);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "LIST");

#ifdef DEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

    printf("Listando: %s\n", path_with_slash);
    res = curl_easy_perform(curl);

    if (res == CURLE_OK && chunk.size > 0) {
#ifdef DEBUG
        printf("--- LISTAGEM BRUTA (%s) ---\n%s\n--- FIM ---\n", path_with_slash, chunk.memory);
#endif
        process_list(chunk.memory, path_with_slash, curl);
    } else {
        fprintf(stderr, "Erro ao listar %s: %s\n", path_with_slash, curl_easy_strerror(res));
    }

    free(chunk.memory);
}

/* Retorna a data de modificação de um arquivo local, ou -1 se não existir */
time_t get_local_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_mtime;
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

    printf("Conectado ao FTP %s:%d\n", cfg.host, cfg.port);
    printf("Varrendo recursivamente /FICHATECNICA/ ...\n");

    list_ftp_directory(curl, "/FICHATECNICA/");

    printf("Total de PDFs encontrados no FTP: %zu\n", pdf_count);
    if (pdf_count == 0) {
        printf("Nenhum PDF encontrado.\n");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 0;
    }

    MKDIR(cfg.destino);

    int baixados = 0;
    printf("\nVerificando arquivos locais e baixando novidades...\n");
    for (size_t i = 0; i < pdf_count; i++) {
        char local_path[2048];
        snprintf(local_path, sizeof(local_path), "%s/%s", cfg.destino, pdf_list[i].relative_path);
        for (char *p = local_path; *p; p++) if (*p == '\\') *p = '/';

        time_t local_mtime = get_local_mtime(local_path);
        bool precisa_baixar = false;

        if (local_mtime == -1) {
            printf("  [NOVO] %s\n", pdf_list[i].relative_path);
            precisa_baixar = true;
        } else if (local_mtime < pdf_list[i].mtime) {
            printf("  [ATUALIZADO] %s (local: %s, remoto: %s)\n",
                   pdf_list[i].relative_path, ctime(&local_mtime), ctime(&pdf_list[i].mtime));
            precisa_baixar = true;
        } else {
            printf("  [OK] %s (já atualizado)\n", pdf_list[i].relative_path);
        }

        if (precisa_baixar) {
            create_directories_for_file(local_path);
            FILE *out = fopen(local_path, "wb");
            if (!out) {
                fprintf(stderr, "    Erro ao criar %s\n", local_path);
                continue;
            }
            char url[2048];
            snprintf(url, sizeof(url), "ftp://%s:%d%s", cfg.host, cfg.port, pdf_list[i].full_path);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
            CURLcode res = curl_easy_perform(curl);
            fclose(out);
            if (res == CURLE_OK) {
                printf("    -> Download concluído.\n");
                baixados++;
            } else {
                printf("    -> FALHA: %s\n", curl_easy_strerror(res));
            }
        }
    }

    printf("\nSincronização finalizada. Arquivos baixados: %d\n", baixados);

    for (size_t i = 0; i < pdf_count; i++) {
        free(pdf_list[i].full_path);
        free(pdf_list[i].relative_path);
        free(pdf_list[i].name);
    }
    free(pdf_list);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}
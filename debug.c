/**
 * ftp_deep_debug.c
 * Diagnóstico detalhado para listagem FTP com WinINet.
 * Compilar: gcc -o deepdebug.exe ftp_deep_debug.c -lwininet
 */

#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "wininet.lib")

typedef struct {
    char host[256];
    int  port;
    char user[128];
    char pass[128];
} Config;

Config cfg;

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
        }
    }
    fclose(f);
    return true;
}

// Função para enviar um comando FTP e capturar a resposta
void ftp_command_debug(HINTERNET hFtp, const char *cmd) {
    char buffer[4096] = {0};
    DWORD bytesRead = 0;
    HINTERNET hData = NULL;

    printf("\n--- Enviando comando: %s ---\n", cmd);

    // Para comandos que abrem conexão de dados (LIST, MLSD, etc.)
    if (strncmp(cmd, "LIST", 4) == 0 || strncmp(cmd, "MLSD", 4) == 0) {
        hData = FtpCommandA(hFtp, FALSE, FTP_TRANSFER_TYPE_ASCII, cmd, 0, NULL);
        if (!hData) {
            printf("Falha ao executar %s (erro %lu)\n", cmd, GetLastError());
            return;
        }

        // Lê os dados retornados
        while (InternetReadFile(hData, buffer, sizeof(buffer)-1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            printf("%s", buffer);
        }
        InternetCloseHandle(hData);
    } else {
        // Comandos sem dados (CWD, PWD, etc.)
        if (!FtpCommandA(hFtp, FALSE, FTP_TRANSFER_TYPE_ASCII, cmd, 0, NULL)) {
            printf("Falha ao executar %s (erro %lu)\n", cmd, GetLastError());
        } else {
            printf("Comando executado com sucesso.\n");
        }
    }
}

void test_ftp_listing(HINTERNET hConnect) {
    WIN32_FIND_DATAA findData;
    HINTERNET hFind = NULL;

    // 1. Navegação incremental
    printf("\n=== Teste 1: Navegação incremental ===\n");
    if (!FtpSetCurrentDirectoryA(hConnect, "/FICHATECNICA")) {
        printf("Falha ao acessar /FICHATECNICA (erro %lu)\n", GetLastError());
        return;
    }
    printf("Diretório atual: /FICHATECNICA\n");

    if (!FtpSetCurrentDirectoryA(hConnect, "214")) {
        printf("Falha ao acessar 214 (erro %lu)\n", GetLastError());
        return;
    }
    printf("Diretório atual: /FICHATECNICA/214\n");

    // Tenta listar com FtpFindFirstFile
    hFind = FtpFindFirstFileA(hConnect, "*", &findData, INTERNET_FLAG_RELOAD, 0);
    if (hFind) {
        printf("Itens encontrados com FtpFindFirstFile:\n");
        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
                continue;
            printf("  - %s (%s)\n", findData.cFileName,
                   (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "DIR" : "FILE");
        } while (InternetFindNextFileA(hFind, &findData));
        InternetCloseHandle(hFind);
    } else {
        printf("FtpFindFirstFile falhou (erro %lu)\n", GetLastError());
    }

    // Volta à raiz para testes manuais
    FtpSetCurrentDirectoryA(hConnect, "/FICHATECNICA");

    // 2. Teste com comando LIST manual no diretório /214
    printf("\n=== Teste 2: Comando LIST manual ===\n");
    ftp_command_debug(hConnect, "CWD /FICHATECNICA/214");
    ftp_command_debug(hConnect, "LIST");

    // 3. Teste com MLSD (se suportado)
    printf("\n=== Teste 3: Comando MLSD (se suportado) ===\n");
    ftp_command_debug(hConnect, "MLSD");

    // Volta à raiz
    FtpSetCurrentDirectoryA(hConnect, "/FICHATECNICA");
}

int main(void) {
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;

    if (!parse_env(".env")) {
        fprintf(stderr, "Erro no .env\n");
        return 1;
    }

    hInternet = InternetOpenA("FTPDebug/2.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return 1;

    printf("Conectando a %s:%d ...\n", cfg.host, cfg.port);
    hConnect = InternetConnectA(
        hInternet,
        cfg.host,
        (INTERNET_PORT)cfg.port,
        cfg.user,
        cfg.pass,
        INTERNET_SERVICE_FTP,
        INTERNET_FLAG_PASSIVE,  // tente remover esta flag para modo ativo se falhar
        0
    );
    if (!hConnect) {
        fprintf(stderr, "Falha na conexão: %lu\n", GetLastError());
        InternetCloseHandle(hInternet);
        return 1;
    }
    printf("Conectado!\n");

    test_ftp_listing(hConnect);

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return 0;
}
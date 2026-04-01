#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

#define MAX_SENSORS   1000
#define TOP_TEMP      10

typedef struct {
    int      ativo;
    int      sensor_id;

    char     ultima_data[12];
    char     ultima_hora[10];
    char     ultimo_status[9];

    int64_t  contagem;
    double   media;
    double   soma_quadrados;

    double   energia_soma;
} SensorStat;

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "sensores.log";

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'\n", filename);
        return 1;
    }

    SensorStat *stats = calloc(MAX_SENSORS, sizeof(SensorStat));
    if (!stats) {
        fprintf(stderr, "Erro: falha ao alocar memoria para os sensores\n");
        fclose(fp);
        return 1;
    }

    int64_t total_alertas = 0;
    double  energia_total = 0.0;
    int64_t total_linhas  = 0;
    char    line[256];

    clock_t inicio = clock();

   while (fgets(line, sizeof(line), fp)) {
        total_linhas++;

        char *ptr = line + 7; 
        char *endptr;
        int sensor_id = (int)strtol(ptr, &endptr, 10);
        
        if (sensor_id < 0 || sensor_id >= MAX_SENSORS) continue;

        char *data_ptr = endptr + 1;
        char *hora_ptr = strchr(data_ptr, ' ') + 1;
        char *tipo_ptr = strchr(hora_ptr, ' ') + 1;

        double val = strtod(tipo_ptr + 12, &endptr);

        char *status_ptr = strstr(endptr, "status ") + 7;

        SensorStat *s = &stats[sensor_id];
        s->ativo = 1;
        s->sensor_id = sensor_id;

        memcpy(s->ultima_data, data_ptr, 10); s->ultima_data[10] = '\0';
        memcpy(s->ultima_hora, hora_ptr, 8);  s->ultima_hora[8] = '\0';
        
        if (status_ptr[0] == 'A' || status_ptr[0] == 'C') {
            total_alertas++;
            strncpy(s->ultimo_status, status_ptr, 8);
            s->ultimo_status[8] = '\0';
        }

        if (tipo_ptr[0] == 't') { 
            s->contagem++;
            double delta = val - s->media;
            s->media += delta / (double)s->contagem;
            s->soma_quadrados += delta * (val - s->media);
        } else if (tipo_ptr[0] == 'e') {
            energia_total += val;
            s->energia_soma += val;
        }
    }
    
    fclose(fp);

    clock_t fim = clock();

    printf("=== Estatisticas do Log de Sensores ===\n\n");

    printf("1. Media de temperatura por sensor (primeiros %d):\n", TOP_TEMP);
    int exibidos = 0;
    for (int i = 0; i < MAX_SENSORS && exibidos < TOP_TEMP; i++) {
        SensorStat *s = &stats[i];
        if (!s->ativo || s->contagem == 0) continue;

        printf("   sensor_%d -> %.2f C  (%" PRId64 " leituras)\n", s->sensor_id, s->media, s->contagem);
        exibidos++;
    }
    if (exibidos == 0)
        printf("Nenhum dado de temperatura encontrado.\n");

    printf("\n2. Sensor mais instavel (maior desvio padrao de temperatura):\n");
    double maior_dp = -1.0;
    int    idx_inst = -1;

    for (int i = 0; i < MAX_SENSORS; i++) {
        SensorStat *s = &stats[i];
        if (!s->ativo || s->contagem < 2) continue;

        double dp = sqrt(s->soma_quadrados / (double)s->contagem);

        if (dp > maior_dp) {
            maior_dp = dp;
            idx_inst = i;
        }
    }

    if (idx_inst >= 0)
        printf("sensor_%d (desvio padrao = %.4f C)\n", stats[idx_inst].sensor_id, maior_dp);
    else
        printf("Dados insuficientes para calcular.\n");

    printf("\n3. Total de alertas (ALERTA + CRITICO): %" PRId64 "\n", total_alertas);

    printf("\n4. Consumo total de energia: %.2f W\n", energia_total);

    double elapsed = ((double)(fim - inicio)) / CLOCKS_PER_SEC;
    printf("\n5. Tempo de execucao: %.4f segundos\n", elapsed);

    int ativos = 0;
    for (int i = 0; i < MAX_SENSORS; i++) ativos += stats[i].ativo;
    printf("\n   (Linhas processadas: %" PRId64 " | Sensores ativos: %d)\n", total_linhas, ativos);

    free(stats);
    return 0;
}
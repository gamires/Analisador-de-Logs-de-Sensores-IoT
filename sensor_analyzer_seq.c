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
        int    sensor_id;
        char   data[12], hora[10], tipo[12], status[9];
        double val;

        int n = sscanf(line,
            "sensor_%d %11s %9s %11s %lf status %8s",
            &sensor_id, data, hora, tipo, &val, status);

        if (n != 6) continue;

        if (sensor_id < 0 || sensor_id >= MAX_SENSORS) {
            fprintf(stderr, "Aviso: sensor_id %d fora do intervalo, ignorado\n", sensor_id);
            continue;
        }

        total_linhas++;

        SensorStat *s = &stats[sensor_id];
        s->ativo     = 1;
        s->sensor_id = sensor_id;
        snprintf(s->ultima_data,   sizeof(s->ultima_data),   "%s", data);
        snprintf(s->ultima_hora,   sizeof(s->ultima_hora),   "%s", hora);
        snprintf(s->ultimo_status, sizeof(s->ultimo_status), "%s", status);

        if (strcmp(status, "ALERTA") == 0 || strcmp(status, "CRITICO") == 0)
            total_alertas++;

        if (strcmp(tipo, "temperatura") == 0) {
            s->contagem++;
            double delta  = val - s->media;
            s->media += delta / (double)s->contagem;
            double delta2 = val - s->media;
            s->soma_quadrados   += delta * delta2;
        }

        if (strcmp(tipo, "energia") == 0) {
            energia_total   += val;
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
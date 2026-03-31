#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_SENSORS  1000
#define MAX_READINGS 100000

typedef struct {
    double *valores;
    int     quant_leituras;
} Leituras;

typedef struct {
    int      sensor_id;
    char     data[12];
    char     hora[10];
    Leituras temp;
    char     status[9];
} SensorTemp;

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "sensores.log";

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", filename);
        return 1;
    }

    SensorTemp *temps = calloc(MAX_SENSORS, sizeof(SensorTemp));
    if (!temps) {
        fprintf(stderr, "Erro: falha ao alocar memoria para os sensores\n");
        fclose(fp);
        return 1;
    }

    for (int i = 0; i < MAX_SENSORS; i++) {
        temps[i].sensor_id = -1;
        temps[i].temp.valores = NULL;
        temps[i].temp.quant_leituras = 0;
    }

    int    total_alertas  = 0;
    double energia_total  = 0.0;
    char   line[256];

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
            fprintf(stderr, "Aviso: sensor_id %d fora do intervalo, linha ignorada\n", sensor_id);
            continue;
        }

        temps[sensor_id].sensor_id = sensor_id;
        snprintf(temps[sensor_id].data,   sizeof(temps[sensor_id].data),   "%s", data);
        snprintf(temps[sensor_id].hora,   sizeof(temps[sensor_id].hora),   "%s", hora);
        snprintf(temps[sensor_id].status, sizeof(temps[sensor_id].status), "%s", status);

        if (strcmp(status, "ALERTA") == 0 || strcmp(status, "CRITICO") == 0)
            total_alertas++;

        if (strcmp(tipo, "temperatura") == 0) {
            Leituras *l = &temps[sensor_id].temp;

            if (l->valores == NULL) {
                l->valores = malloc(MAX_READINGS * sizeof(double));
                if (!l->valores) {
                    fprintf(stderr, "Erro: falha ao alocar leituras do sensor %d\n", sensor_id);
                    continue;
                }
            }

            if (l->quant_leituras < MAX_READINGS)
                l->valores[l->quant_leituras++] = val;
        }

        if (strcmp(tipo, "energia") == 0)
            energia_total += val;
    }

    fclose(fp);
     /* ========================================================
       RESULTADOS
       ======================================================== */
    printf("=== Estatisticas do Log de Sensores ===\n\n");

    printf("1. Media de temperatura por sensor:\n");
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (temps[i].sensor_id < 0 || temps[i].temp.quant_leituras == 0) continue;

        double soma = 0.0;
        for (int j = 0; j < temps[i].temp.quant_leituras; j++)
            soma += temps[i].temp.valores[j];
        double media = soma / temps[i].temp.quant_leituras;

        printf("sensor_%d -> %.2f  (%d leituras)\n",
               temps[i].sensor_id, media, temps[i].temp.quant_leituras);
    }

    printf("\n2. Sensor mais instavel (maior desvio padrao):\n");
    double maior_dp = -1.0;
    int    idx_inst = -1;

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (temps[i].temp.quant_leituras < 2) continue;

        double soma = 0.0;
        for (int j = 0; j < temps[i].temp.quant_leituras; j++)
            soma += temps[i].temp.valores[j];
        double media = soma / temps[i].temp.quant_leituras;

        double var = 0.0;
        for (int j = 0; j < temps[i].temp.quant_leituras; j++) {
            double d = temps[i].temp.valores[j] - media;
            var += d * d;
        }
        double dp = sqrt(var / temps[i].temp.quant_leituras);

        if (dp > maior_dp) { maior_dp = dp; idx_inst = i; }
    }

    if (idx_inst >= 0)
        printf("sensor_%d (desvio padrao = %.4f)\n", temps[idx_inst].sensor_id, maior_dp);
    else
        printf("Dados insuficientes para calcular.\n");

    printf("\n3. Total de alertas (ALERTA + CRITICO): %d\n", total_alertas);
    printf("\n4. Consumo total de energia: %.2f\n", energia_total);

    clock_t fim = clock();
    printf("\nTempo de execucao: %f segundos\n", ((double)(fim - inicio)) / CLOCKS_PER_SEC);

    for (int i = 0; i < MAX_SENSORS; i++)
        free(temps[i].temp.valores);
    free(temps);

    return 0;
}

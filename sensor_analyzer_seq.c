#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_SENSORS 1000
#define MAX_READINGS 100000

typedef struct {
    char type[2][12];
    float valor[2];
    float media[2];
    float dp[2];
} type;

typedef struct {
    char sensor_id[4];
    char data[10];
    char hora[8];
    type tipo;
    char status[8];
} SensorTemp;

int find_or_add_sensor(SensorTemp sensors[], int *n, const char *id) {
    for (int i = 0; i < *n; i++) {
        if (strcmp(sensors[i].sensor_id, id) == 0)
            return i;
    }
    strncpy(sensors[*n].sensor_id, id, 15);
    sensors[*n].count = 0;
    return (*n)++;
}

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "sensor.log";

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", filename);
        return 1;
    }

    SensorTemp *temps = calloc(MAX_SENSORS, sizeof(SensorTemp));
    if (!temps) { fclose(fp); return 1; }

    int n_temp_sensors = 0;
    int total_alertas   = 0;
    double energia_total = 0.0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char sensor_id[3], data[11], hora[9];
        double val1, val2;
        char tipo1[12], tipo2[12];  
        char status[8];

        /* formato: sensor_XXX AAAA-MM-DD HH:MM:SS tipo1 val1 [tipo2 val2] status STATUS */
        /* tenta ler com dois pares tipo/valor */
        int n = sscanf(line,
            "%3s %11s %9s %12s %f %12s %f %8s",
            sensor_id, data, hora,
            tipo1, &val1, tipo2, &val2, status);

        if (n != 8) continue;

        if (strcmp(status, "ALERTA") == 0 || strcmp(status, "CRITICO") == 0)
            total_alertas++;

        /* --- estatística 1 e 2: temperatura por sensor --- */
        if (strcmp(tipo1, "temperatura") == 0) {
            int idx = find_or_add_sensor(temps, &n_temp_sensors, sensor_id);
            temps[idx].tipo.media[0] += val1;
        }

        /* --- estatística 4: consumo de energia --- */
        if (strcmp(tipo1, "energia") == 0)
            energia_total += val1;
        if (strcmp(tipo2, "energia") == 0)
            energia_total += val2;
    }

    fclose(fp);

    /* ========================================================
       RESULTADOS
       ======================================================== */

    printf("=== Estatisticas do Log de Sensores ===\n\n");

    /* 1. Media de temperatura por sensor */
    printf("1. Media de temperatura por sensor:\n");
    for (int i = 0; i < n_temp_sensors; i++) {
        double soma = 0.0;
        for (int j = 0; j < temps[i].count; j++)
            soma += temps[i].values[j];
        double media = soma / temps[i].count;
        printf("   %-12s -> %.2f  (%d leituras)\n",
               temps[i].sensor_id, media, temps[i].count);
    }

    /* 2. Sensor mais instavel (maior desvio padrao) */
    printf("\n2. Sensor mais instavel (maior desvio padrao):\n");
    double maior_dp  = -1.0;
    int    idx_inst  = -1;

    for (int i = 0; i < n_temp_sensors; i++) {
        if (temps[i].count < 2) continue;

        double soma = 0.0;
        for (int j = 0; j < temps[i].count; j++)
            soma += temps[i].values[j];
        double media = soma / temps[i].count;

        double var = 0.0;
        for (int j = 0; j < temps[i].count; j++) {
            double d = temps[i].values[j] - media;
            var += d * d;
        }
        double dp = sqrt(var / temps[i].count);

        if (dp > maior_dp) { maior_dp = dp; idx_inst = i; }
    }

    if (idx_inst >= 0)
        printf("   %s  (desvio padrao = %.4f)\n",
               temps[idx_inst].sensor_id, maior_dp);
    else
        printf("   Dados insuficientes para calcular.\n");

    /* 3. Total de alertas */
    printf("\n3. Total de alertas (ALERTA + CRITICO): %d\n", total_alertas);

    /* 4. Consumo total de energia */
    printf("\n4. Consumo total de energia: %.2f\n", energia_total);

    free(temps);
    return 0;
}   
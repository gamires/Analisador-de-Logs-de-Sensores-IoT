#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_SENSORS 1000
#define MAX_READINGS 100000

typedef struct {
    char type[12];
    double valor[MAX_READINGS];
    int quant_leituras;
} type;

typedef struct {
    int sensor_id;
    char data[10];
    char hora[8];
    type tipo;
    char status[8];
} SensorTemp;

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "sensor.log";

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", filename);
        return 1;
    }

    SensorTemp *temps = calloc(MAX_SENSORS, sizeof(SensorTemp));
    int total_alertas   = 0;
    double energia_total = 0.0;

    char line[256];

    clock_t inicio = clock();

    while (fgets(line, sizeof(line), fp)) {
        int sensor_id; 
        char data[11], hora[9], tipo[12], status[8];
        double val;

        int n = sscanf(line,
            "sensor_%d %10s %8s %11s %lf status %7s",
            &sensor_id, data, hora, tipo, &val, status);
            
        if (n != 6) continue;

        if (strcmp(status, "ALERTA") == 0 || strcmp(status, "CRITICO") == 0)
            total_alertas++;

        if (strcmp(tipo, "temperatura") == 0) {
            if (temps[sensor_id].tipo.quant_leituras < MAX_READINGS) {
                temps[sensor_id].tipo.valor[temps[sensor_id].tipo.quant_leituras] = val;
                temps[sensor_id].tipo.quant_leituras++;
            }
        }

        if (strcmp(tipo, "energia") == 0) energia_total += val;
    }

    fclose(fp);

    /* ========================================================
       RESULTADOS
       ======================================================== */
    printf("=== Estatisticas do Log de Sensores ===\n\n");

    printf("1. Media de temperatura por sensor:\n");
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (temps[i].tipo.quant_leituras > 0) {
            double soma = 0.0;

            for (int j = 0; j < temps[i].tipo.quant_leituras; j++)
                soma += temps[i].tipo.valor[j];
            double media = soma / temps[i].tipo.quant_leituras;
            printf("%d -> %.2f  (%d leituras)\n", temps[i].sensor_id, media, temps[i].tipo.quant_leituras);
        }
        
    }

    printf("\n2. Sensor mais instavel (maior desvio padrao):\n");
    double maior_dp  = -1.0;
    int    idx_inst  = -1;

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (temps[i].tipo.quant_leituras < 2) continue;

        double soma = 0.0;
        for (int j = 0; j < temps[i].tipo.quant_leituras; j++)
            soma += temps[i].tipo.valor[j];
        double media = soma / temps[i].tipo.quant_leituras;

        double var = 0.0;
        for (int j = 0; j < temps[i].tipo.quant_leituras; j++) {
            double d = temps[i].tipo.valor[j] - media;
            var += d * d;
        }
        double dp = sqrt(var / temps[i].tipo.quant_leituras);

        if (dp > maior_dp) { maior_dp = dp; idx_inst = i; }
    }

    if (idx_inst >= 0)
        printf("%d (desvio padrao = %.4f)\n",
               temps[idx_inst].sensor_id, maior_dp);
    else
        printf("Dados insuficientes para calcular.\n");

    printf("\n3. Total de alertas (ALERTA + CRITICO): %d\n", total_alertas);

    printf("\n4. Consumo total de energia: %.2f\n", energia_total);
    
    clock_t fim = clock();

    double tempo_execucao = ((double)(fim - inicio)) / CLOCKS_PER_SEC;

    printf("Tempo de execucao: %f segundos\n", tempo_execucao);

    free(temps);
    return 0;
}   
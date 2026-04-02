/*
Aluno: Pedro Henrique Carvalho Pereira — RA: 10418861
Aluno: Mateus Ribeiro Cerqueira — RA: 10443901
Aluno: Gabriel Mires Camargo — RA: 10436741
Turma: 05D - CC
Fontes:
- https://www.ibm.com/docs/pt-br/i/7.6.0?topic=functions-atoi-convert-character-string-integer
- https://linguagemc.com.br/argumentos-em-linha-de-comando/ 
- https://linguagemc.com.br/arquivos-em-c-categoria-usando-arquivos/ 
- https://www.ibm.com/docs/pt-br/i/7.6.0?topic=functions-strdup-duplicate-string
- https://www.ibm.com/docs/pt-br/aix/7.3.0?topic=m-malloc-free-realloc-calloc-mallopt-mallinfo-mallinfo-heap-alloca-valloc-posix-memalign-subroutine 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <time.h>



#define MAX_LINHA_BUFFER 256 
#define MAX_SENSORS 1000 




// struct para os dados da média e desvio padrão de cada sensor
typedef struct {
    double soma; // soma de todos os valores de temp
    double soma_quadrados; // soma dos quadrados dos valores (variancia)
    int contagem; // cont para as leituras válidas do sensor
} ArgsSensor;



// VAL GLOBAIS
ArgsSensor sensores[MAX_SENSORS];
double energia_total = 0.0;
int total_alertas = 0;

// MUTEX GLOBAL
pthread_mutex_t mutex;



// struct para as threads
typedef struct {
    char **linhas; // ponteiro pro conteúdo do arquivo
    long inicio; // inicio da leitura da thread
    long fim; // final da leitura da thread
} ThreadArgs;

//
int extrair_id_sensor(const char *sensor_str) {
    
    int id = 0;
    
    sscanf(sensor_str, "sensor_%d", &id);
    return id;
}

// cada thread executa
void *processar_linhas(void *arg) {
    
    // coerão (Luba, essa é pra você)
    ThreadArgs *args = (ThreadArgs *)arg;


    // processar o bloco específico da thread 
    //printf("\nInicio da Thread: %ld\nFim da Thread: %ld\n");
    for (long i = args->inicio; i < args->fim; i++) {

        char sensor[32], data[16], hora[16];
        char tipo[32], status[32];
        double valor;

        // parsing com sscanf pra separar a linha de log
        int lidos = sscanf(args->linhas[i], "%s %s %s %s %lf status %s",sensor, data, hora, tipo, &valor, status);
        //printf("======= TESTE PARSING: =======\n sensor:%s\n data: %s\n hora: %s\n tipo: %s\n valor: %lf\n status %s\n==================================",sensor, data, hora, tipo, &valor, status)

        // caso de mais parametros (erro)
        if (lidos != 6) continue;


        int id = extrair_id_sensor(sensor);

        // XXXXXXXXXXSEGMENTATION FAULT
        if (id < 0 || id >= MAX_SENSORS) continue;

        pthread_mutex_lock(&mutex);




        // temperatura (atualiza struct pra media e dp)
        if (strcmp(tipo, "temperatura") == 0) {
            sensores[id].soma += valor;
            sensores[id].soma_quadrados += valor * valor;
            sensores[id].contagem++;
        }

        // energia (atualiza VG)
        if (strcmp(tipo, "energia") == 0) {
            energia_total += valor;
        }

        // alertas (atualiza VG)
        if (strcmp(status, "ALERTA") == 0 || strcmp(status, "CRITICO") == 0) {
            total_alertas++;
        }

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

// 
double calcular_desvio(ArgsSensor *s) {
    if (s->contagem == 0) return 0.0; //div zero

    double media = s->soma / s->contagem;
    double variancia = (s->soma_quadrados / s->contagem) - (media * media);

    // Evitar Not a Number por srt de num neg
    if (variancia < 0) variancia = 0;

    return sqrt(variancia);
}





int main(int argc, char *argv[]) {
    //verifica args
    if (argc < 3) {
        printf("Uso: %s <num_threads> <arquivo>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]); //atoi converter string p int
    char *arquivo_nome = argv[2];

    FILE *fp = fopen(arquivo_nome, "r");
    if (!fp) {
        perror("Erro ao abrir arquivo");
        return 1;
    }


    char **linhas = NULL; //array de strings
    long capacidade = 100000;
    long total_linhas = 0;

    linhas = malloc(capacidade * sizeof(char*));

    char buffer[MAX_LINHA_BUFFER]; // ta usando 256 msm


    // linha por linha
    while (fgets(buffer, MAX_LINHA_BUFFER, fp)) {

        // loop pra realocar conforme necessário
        if (total_linhas >= capacidade) {
            capacidade *= 2;
            linhas = realloc(linhas, capacidade * sizeof(char*));
        }


        // duplica string, aloca memoria e retorna o ponteiro
        linhas[total_linhas] = strdup(buffer);
        total_linhas++;
    }




    
    fclose(fp);


    // LINHAS PROCESSADAS
    printf("Total de linhas: %ld\n", total_linhas);

    pthread_mutex_init(&mutex, NULL);

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
    // INICIO DO TIMER
    clock_t inicio = clock();


    // ids das threads
    pthread_t threads[num_threads];
    ThreadArgs args[num_threads];


    // calculo dos blocos pras threads
    long bloco = total_linhas / num_threads;


    // execução das threads para cada bloco
    for (int i = 0; i < num_threads; i++) {

        args[i].linhas = linhas;//array de strings-linahs (p duplo no ini da main)
        args[i].inicio = i * bloco; //onde a thread começa

        // fim e ultima thread
        if (i == num_threads - 1) {
            args[i].fim = total_linhas;
        } else {
            args[i].fim = (i + 1) * bloco;
        }

        // criar e pedir pra executar processar_linhas com args[i]
        pthread_create(&threads[i], NULL, processar_linhas, &args[i]);
    }

    // ESPERAR AS THREADS
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // FINAL DO TIMER
    clock_t fim = clock();
    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------


    printf("\nRESULTADOS: \n");


    // MÉDIA
    printf("\nMédia de temperatura por sensor (apenas os 10 primeiros):\n");
    int cont = 0;
    for (int i = 0; i < MAX_SENSORS && cont < 10; i++) {
        if (sensores[i].contagem > 0) {
            double media = sensores[i].soma / sensores[i].contagem;
            printf("sensor_%03d - Med.:%.2f\n", i, media);
            cont++;
        }
    }



    // MAIS ISNTÁVEL
    double max_desvio = 0.0;
    int sensor_instavel = -1;
    for (int i = 0; i < MAX_SENSORS; i++) {
        if (sensores[i].contagem > 0) {
            double d = calcular_desvio(&sensores[i]);
            if (d > max_desvio) {
                max_desvio = d;
                sensor_instavel = i;
            }
        }
    }
    printf("\nSensor mais instável: sensor_%03d - Desvio padrão = %.4f\n", sensor_instavel, max_desvio);

    printf("Total de alertas: %d\n", total_alertas);
    printf("Consumo total de energia: %.2f\n", energia_total);
    printf("Tempo de execução: %.4f s\n", tempo);





    // libera o que foi alocado no strdup
    for (long i = 0; i < total_linhas; i++) {
        free(linhas[i]);
    }

    // libera o array principal
    free(linhas);

    // finaliza o mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}

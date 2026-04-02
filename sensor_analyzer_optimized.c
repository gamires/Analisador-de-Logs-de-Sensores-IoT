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

ESTUDOS QUE O GEMINI INDICOU PARA FAZER O PARSING MANUAL E ECONOMIZAR TEMPO NA PARALELIZAÇÃO:
- https://gcc.gnu.org/onlinedocs/gcc-14.1.0/gcc/Static-Analyzer-Options.html
- https://medium.com/@arkarthick/using-static-branch-prediction-with-gccrecently-i-extended-my-heap-manager-for-our-product-to-have-7cadeb9a2208 
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
    char *buffer;
    long  inicio;
    long  fim;
} ThreadArgs;



// parsing rápido do id
//https://gcc.gnu.org/onlinedocs/gcc-14.1.0/gcc/Static-Analyzer-Options.html
//https://medium.com/@arkarthick/using-static-branch-prediction-with-gccrecently-i-extended-my-heap-manager-for-our-product-to-have-7cadeb9a2208 
static inline int extrair_id_sensor_fast(const char *s) {
    return atoi(s + 7); // pula "sensor_"
}



// cada thread executa
void *processar_linhas(void *arg) {

    // coerão (Luba, essa é pra você)
    ThreadArgs *args = (ThreadArgs *)arg;

    // ----------------------------------------------------------
    // DADOS LOCAIS
    ArgsSensor sensores_locais[MAX_SENSORS] = {0};
    double energia_local = 0.0;
    int    alertas_local = 0;
    // ----------------------------------------------------------

    char *ptr = args->buffer + args->inicio;
    char *fim = args->buffer + args->fim;


    // PARSING MANUAL - 
    //https://gcc.gnu.org/onlinedocs/gcc-14.1.0/gcc/Static-Analyzer-Options.html
    //https://medium.com/@arkarthick/using-static-branch-prediction-with-gccrecently-i-extended-my-heap-manager-for-our-product-to-have-7cadeb9a2208 
    while (__builtin_expect(ptr < fim, 1)) {

        
        // SENSOR ID
        int id = atoi(ptr + 7);

        // pula "sensor_XXX "
        ptr = strchr(ptr, ' ') + 1;

        // pula data e hora (2 tokens)
        ptr = strchr(ptr, ' ') + 1;
        ptr = strchr(ptr, ' ') + 1;

        
        // TIPO
        char tipo = *ptr; // primeira letra

        ptr = strchr(ptr, ' ') + 1;

        
        // VALOR
        double valor = atof(ptr);

        ptr = strchr(ptr, ' ') + 1;

        
        // STATUS (vai direto na primeira letra)
        ptr = strstr(ptr, "status ") + 7;
        char status = *ptr;

        
        // PROCESSAMENTO — todos os sensores do bloco são processados
        if (tipo == 't') {
            sensores_locais[id].soma           += valor;
            sensores_locais[id].soma_quadrados += valor * valor;
            sensores_locais[id].contagem++;
        }
        else if (tipo == 'e') {
            energia_local += valor;
        }

        if (status == 'A' || status == 'C') {
            alertas_local++;
        }

        // vai para próxima linha
        ptr = strchr(ptr, '\n');
        if (!ptr) break;
        ptr++;
    }

    
    // mutex travaNdo apenas UMA vez por thread
    pthread_mutex_lock(&mutex);

    for (int i = 0; i < MAX_SENSORS; i++) {
        if (sensores_locais[i].contagem > 0) {
            sensores[i].soma           += sensores_locais[i].soma;
            sensores[i].soma_quadrados += sensores_locais[i].soma_quadrados;
            sensores[i].contagem       += sensores_locais[i].contagem;
        }
    }

    energia_total += energia_local;
    total_alertas += alertas_local;

    pthread_mutex_unlock(&mutex);

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


    // leitura em buffer
    fseek(fp, 0, SEEK_END);
    long tamanho = ftell(fp);
    rewind(fp);

    char *buffer = malloc(tamanho + 1);
    size_t lidos = fread(buffer, 1, tamanho, fp);

    if (lidos != (size_t)tamanho) {
        perror("Erro ao ler arquivo");
        free(buffer);
        fclose(fp);
        return 1;
    }
    // tamanho em bytes
    buffer[tamanho] = '\0';
    //printf("Tamanho do arquivo: %ld bytes\n", tamanho);
    
    fclose(fp);


    pthread_mutex_init(&mutex, NULL);

// -------------------------------------------------------------------------
    // clock_gettime mede tempo de parede real (wall time)
    struct timespec ts_inicio, ts_fim;
    clock_gettime(CLOCK_MONOTONIC, &ts_inicio);

    // ids das threads
    pthread_t threads[num_threads];
    ThreadArgs args[num_threads];


    // calculo dos blocos pras threads
    long bloco = tamanho / num_threads;

    // execução das threads para cada bloco
    for (int i = 0; i < num_threads; i++) {

        args[i].buffer = buffer;
        args[i].inicio = i * bloco; //onde a thread começa
        args[i].fim    = (i == num_threads - 1) ? tamanho : (i + 1) * bloco;

        // ajusta inicio
        if (i != 0) {
            while (args[i].inicio < tamanho && buffer[args[i].inicio] != '\n') {
                args[i].inicio++;
            }
            args[i].inicio++;
        }

        pthread_create(&threads[i], NULL, processar_linhas, &args[i]);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_fim);
    double tempo = (ts_fim.tv_sec  - ts_inicio.tv_sec) + (ts_fim.tv_nsec - ts_inicio.tv_nsec) / 1e9;
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

    free(buffer);

    pthread_mutex_destroy(&mutex);

    return 0;
}

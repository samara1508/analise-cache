#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef struct {
    int valido;
    unsigned int rotulo;
    int dirty;
    int contador_lru;
} BlocoCache;

typedef struct {
    char nome_arquivo[100];
    int pol_escrita; // 0: Write-Through, 1: Write-Back
    int tam_bloco;
    int total_blocos;
    int vias_conjunto;
    double tempo_cache;
    int pol_subst; // 0: LRU, 1: Aleatória
    double tempo_mp_ns;
} Configuracao;

typedef struct {
    int leituras;
    int escritas;
    int hit_leitura;
    int miss_leitura;
    int hit_escrita;
    int miss_escrita;
    int acessos_mp_leitura;
    int acessos_mp_escrita;
} Metricas;

int calcular_bits(int n) {
    int bits = 0;
    while (n >>= 1) bits++;
    return bits;
}

void ler_configuracoes(Configuracao *cfg) {
    printf("Nome do arquivo: ");
    scanf("%s", cfg->nome_arquivo);
    printf("Politica de escrita (0 - Write-Through, 1 - Write-Back): ");
    scanf("%d", &cfg->pol_escrita);
    printf("Tamanho da linha (potencia de 2, em bytes): ");
    scanf("%d", &cfg->tam_bloco);
    printf("Numero de linhas (potencia de 2): ");
    scanf("%d", &cfg->total_blocos);
    printf("Nro de linhas por conjunto (potencia de 2): ");
    scanf("%d", &cfg->vias_conjunto);
    printf("Tempo de acesso do Hit (em ns): ");
    scanf("%lf", &cfg->tempo_cache);
    printf("Politica de substituicao (0 - LRU, 1 - Aleatoria): ");
    scanf("%d", &cfg->pol_subst);
    printf("Tempo de acesso da MP (em ns): ");
    scanf("%lf", &cfg->tempo_mp_ns);
}

int buscar_bloco(BlocoCache *cache, int inicio, int fim, unsigned int rotulo_buscado) {
    for (int i = inicio; i < fim; i++) {
        if (cache[i].valido && cache[i].rotulo == rotulo_buscado) {
            return i;
        }
    }
    return -1;
}

int selecionar_bloco(BlocoCache *cache, int inicio, int fim, int politica, int vias) {
    for (int i = inicio; i < fim; i++) {
        if (!cache[i].valido) return i;
    }
    if (politica == 0) { // LRU
        for (int i = inicio; i < fim; i++) {
            if (cache[i].contador_lru == 0) return i;
        }
    }
    // Aleatório
    return inicio + (rand() % vias);
}

void atualizar_lru_bloco(BlocoCache *cache, int inicio, int fim, int indice_alvo, int vias, int is_hit) {
    int ref_lru = is_hit ? cache[indice_alvo].contador_lru : -1;

    for (int i = inicio; i < fim; i++) {
        if (cache[i].valido && i != indice_alvo && cache[i].contador_lru > ref_lru) {
            cache[i].contador_lru--;
        }
    }
    cache[indice_alvo].contador_lru = vias - 1;
}

void limpar_cache_wb(BlocoCache *cache, Configuracao *cfg, Metricas *met) {
    if (cfg->pol_escrita == 1) {
        for (int i = 0; i < cfg->total_blocos; i++) {
            if (cache[i].valido && cache[i].dirty) {
                met->acessos_mp_escrita++;
                cache[i].dirty = 0;
            }
        }
    }
}

void gerar_resultados(Configuracao *cfg, Metricas *met) {
    int total_acessos = met->leituras + met->escritas;
    int total_hits = met->hit_leitura + met->hit_escrita;

    double tx_hit_leitura = met->leituras > 0 ? (double)met->hit_leitura / met->leituras : 0.0;
    double tx_hit_escrita = met->escritas > 0 ? (double)met->hit_escrita / met->escritas : 0.0;
    double tx_hit_global = total_acessos > 0 ? (double)total_hits / total_acessos : 0.0;

    double tmp_medio_acesso = cfg->tempo_cache + ((1.0 - tx_hit_global) * cfg->tempo_mp_ns);

    printf("\n\n=== PARAMETROS DE ENTRADA ===\n");
    printf("Arquivo: %s\n", cfg->nome_arquivo);
    printf("Politica de substituicao: %s\n", cfg->pol_subst == 0 ? "LRU" : "Aleatoria");
    printf("Politica de escrita: %s\n", cfg->pol_escrita == 0 ? "Write-Through" : "Write-Back");
    printf("Tamanho da linha: %d\n", cfg->tam_bloco);
    printf("Numero de linhas: %d\n", cfg->total_blocos);
    printf("Nro de linhas por conjunto: %d\n", cfg->vias_conjunto);
    printf("Tempo de acesso do Hit: %.4f ns\n", cfg->tempo_cache);
    printf("Tempo de acesso da MP: %.4f ns\n", cfg->tempo_mp_ns);

    printf("\n=== RESULTADO ===");
    printf("\n-> Arquivo de entrada\n");
    printf("Numero de leituras realizadas: %d\n", met->leituras);
    printf("Numero de escritas realizadas: %d\n", met->escritas);
    printf("Soma total de leituras e escritas: %d\n", met->leituras + met->escritas);

    printf("\n-> Memoria principal\n");
    printf("Numero de leituras realizadas: %d\n", met->acessos_mp_leitura);
    printf("Numero de escritas realizadas: %d\n", met->acessos_mp_escrita);
    printf("Soma total de leituras e escritas: %d\n", met->acessos_mp_leitura + met->acessos_mp_escrita);

    printf("\n-> Taxas de acerto\n");
    printf("Leitura: %.2f%% (%d)\n", tx_hit_leitura * 100, met->hit_leitura);
    printf("Escrita: %.2f%% (%d)\n", tx_hit_escrita * 100, met->hit_escrita);
    printf("Global:  %.2f%% (%d)\n", tx_hit_global * 100, total_hits);

    printf("\n-> Desempenho\n");
    printf("Tempo medio de acesso da cache: %.4f ns\n", tmp_medio_acesso);
}

int main() {
    Configuracao cfg;
    Metricas met = {0};

    srand(time(NULL));
    ler_configuracoes(&cfg);

    int qtd_conjuntos = cfg.total_blocos / cfg.vias_conjunto;
    int bits_offset = calcular_bits(cfg.tam_bloco);

    BlocoCache *mem_cache = (BlocoCache *)calloc(cfg.total_blocos, sizeof(BlocoCache));
    if (!mem_cache) return 1;

    FILE *arquivo = fopen(cfg.nome_arquivo, "r");
    if (!arquivo) {
        free(mem_cache);
        return 1;
    }

    unsigned int endereco;
    char op;

    while (fscanf(arquivo, "%x %c", &endereco, &op) != EOF) {
        unsigned int end_base = endereco >> bits_offset;
        unsigned int id_conjunto = end_base % qtd_conjuntos;
        unsigned int tag = end_base / qtd_conjuntos;

        int inicio_conj = id_conjunto * cfg.vias_conjunto;
        int fim_conj = inicio_conj + cfg.vias_conjunto;

        if (op == 'R') met.leituras++; else met.escritas++;

        int indice_alvo = buscar_bloco(mem_cache, inicio_conj, fim_conj, tag);

        if (indice_alvo != -1) {
            if (op == 'R') met.hit_leitura++;
            else {
                met.hit_escrita++;
                if (cfg.pol_escrita == 0) met.acessos_mp_escrita++; // WT
                else mem_cache[indice_alvo].dirty = 1;              // WB
            }

            if (cfg.pol_subst == 0)
                atualizar_lru_bloco(mem_cache, inicio_conj, fim_conj, indice_alvo, cfg.vias_conjunto, 1);

        } else {
            if (op == 'R') met.miss_leitura++; else met.miss_escrita++;

            if (op == 'W' && cfg.pol_escrita == 0) {
                met.acessos_mp_escrita++;
            } else {
                indice_alvo = selecionar_bloco(mem_cache, inicio_conj, fim_conj, cfg.pol_subst, cfg.vias_conjunto);

                if (cfg.pol_escrita == 1 && mem_cache[indice_alvo].valido && mem_cache[indice_alvo].dirty) {
                    met.acessos_mp_escrita++;
                }

                met.acessos_mp_leitura++;
                mem_cache[indice_alvo].valido = 1;
                mem_cache[indice_alvo].rotulo = tag;
                mem_cache[indice_alvo].dirty = (op == 'W') ? 1 : 0;

                if (cfg.pol_subst == 0)
                    atualizar_lru_bloco(mem_cache, inicio_conj, fim_conj, indice_alvo, cfg.vias_conjunto, 0);
            }
        }
    }

    limpar_cache_wb(mem_cache, &cfg, &met);
    fclose(arquivo);

    gerar_resultados(&cfg, &met);

    free(mem_cache);
    return 0;
}

# mini-c-compiler

Compilador para MiniC, uma linguagem pequena com sintaxe de C. O compilador é escrito em C++17, gera assembly x86-64 (sintaxe AT&T, System V ABI) e usa um runtime em C para entrada e saída. O `cc` do sistema monta e linka o executável final.

Fiz o projeto para praticar as fases que uma disciplina de Compiladores cobre, do texto-fonte até código de máquina, sem LLVM nem gerador de parser.

## A linguagem

```c
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int main() {
    int n = read();
    print(fact(n));
    return 0;
}
```

- Tipos: `int` (inteiro de 64 bits com sinal), `bool` e `void` (só como retorno)
- Funções com até 6 parâmetros, recursão e chamadas entre funções em qualquer ordem
- `if`/`else`, `while`, `return`, blocos com escopo próprio e sombreamento de variáveis
- Operadores: `+ - * / %`, `== != < <= > >=`, `&& || !` (com curto-circuito) e `-` unário
- Funções embutidas: `print(int)` e `read()`
- Comentários `//` e `/* */`
- Variáveis declaradas sem valor começam em zero; overflow aritmético dá a volta (complemento de dois)

`int` e `bool` não se convertem um no outro: `if (1)` é erro de tipo.

## Fases do compilador

| Fase | Arquivo | O que faz |
|------|---------|-----------|
| Análise léxica | `src/lexer.cpp` | Transforma o texto em tokens, guardando a linha de cada um para as mensagens de erro |
| Análise sintática | `src/parser.cpp` | Parser descendente recursivo que monta a AST; a precedência dos operadores binários sai de uma tabela de níveis |
| Análise semântica | `src/sema.cpp` | Tabela de símbolos com pilha de escopos, checagem de tipos, aridade de chamadas e `return`. Também renomeia cada variável para um nome único (`a` vira `a.3`), o que resolve o sombreamento antes das fases seguintes |
| Código intermediário | `src/ir.cpp` | Gera código de três endereços (TAC) com temporários, rótulos e saltos. `&&` e `||` viram desvios, por isso o lado direito só roda quando precisa |
| Otimização | `src/optimizer.cpp` | Propagação e dobra de constantes dentro de blocos básicos, eliminação de código inalcançável, de temporários mortos, de saltos para a linha seguinte e de rótulos sem uso. Repete até nada mudar |
| Geração de código | `src/codegen.cpp` | Traduz o TAC para x86-64. Cada variável e temporário ganha um slot na pilha; argumentos vão em `rdi, rsi, rdx, rcx, r8, r9` e a pilha fica alinhada em 16 bytes nas chamadas |
| Runtime | `runtime/runtime.c` | `mc_print` e `mc_read`, em C, linkados junto ao programa |
| Driver | `src/main.cpp` | Lê os argumentos, chama as fases em ordem e invoca o `cc` para montar e linkar |

## Compilando

Precisa de `g++`, `gcc` e `make` (no Ubuntu: `sudo apt install build-essential`).

```sh
make          # gera ./minicc e build/runtime.o
make test     # roda a suíte em tests/
```

## Uso

```sh
./minicc tests/ok/factorial.mc -o fact && ./fact
./minicc -O tests/ok/primes.mc -o primes     # com otimizações

./minicc --tokens prog.mc   # lista de tokens
./minicc --ast prog.mc      # AST em formato de S-expression
./minicc --ir prog.mc       # código de três endereços
./minicc --ir -O prog.mc    # TAC depois do otimizador
./minicc -S prog.mc         # assembly x86-64
```

Os erros saem no formato `arquivo:linha: error: mensagem`:

```
tests/err/arity.mc:3: error: function 'add' expects 2 argument(s) but got 1
```

## Exemplo de otimização

Para este programa:

```c
int main() {
    int x = 10;
    int y = x * 2 + 1;
    if (y > 20) print(y); else print(0);
    return 0;
}
```

`--ir` mostra o TAC gerado direto da AST:

```
function main()
  x.1 = 10
  %t1 = x.1 * 2
  %t2 = %t1 + 1
  y.2 = %t2
  %t3 = y.2 > 20
  ifz %t3 goto .L1
  call print(y.2)
  goto .L2
.L1:
  call print(0)
.L2:
  return 0
  return 0
```

E `--ir -O` mostra o que sobra depois das otimizações:

```
function main()
  x.1 = 10
  y.2 = 21
  call print(21)
  return 0
```

## Testes

`tests/run.sh` compila cada programa de `tests/ok/` com e sem `-O`, executa e compara a saída com o arquivo `.out` correspondente (o `.in`, quando existe, vira a entrada padrão). Os programas de `tests/err/` têm que falhar, e a mensagem de erro precisa conter o texto da linha `// expect:`.

## Limitações conhecidas

- Sem arrays, ponteiros, strings ou variáveis globais.
- Alocação de registradores ingênua: todo valor passa pela pilha. Um alocador por coloração de grafo ou linear scan seria o próximo passo.
- A propagação de constantes é local a cada bloco básico. Para levar constantes através de laços e desvios seria preciso análise de fluxo de dados (alcance de definições) sobre o grafo de fluxo de controle.
- Atribuições a variáveis que nunca são lidas continuam no código; remover isso pede análise de vivacidade.
- Função não-`void` que chega ao fim sem `return` devolve 0, sem aviso.
- Só roda em Linux x86-64.

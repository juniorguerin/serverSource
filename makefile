# Makefile dos projetos

# Variavel que define o compilador
CC = clang

# Variavel de opcoes de compilacao e bibliotecas estaticas
CFLAGS = -Werror -Wall -Wextra -g 

# Variaveis de paths
INCLUDE = ./include
OBJ = ./obj
VPATH = ./src

.PHONY: clean all

REC_WEB_FILES = $(addprefix $(OBJ)/, client.o recuperadorweb.o)
SERV_FILES = $(addprefix $(OBJ)/, server.o servidorweb.o)

all: recuperadorweb servidorweb

recuperadorweb: $(REC_WEB_FILES)
	$(CC) $^ -o recuperadorweb

servidorweb: $(SERV_FILES)
	$(CC) $^ -o servidorweb

# Gera os .o para o projeto
$(OBJ)/%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $^ -o $@

clean:
	rm -f $(OBJ)/*.o recuperadorweb servidorweb

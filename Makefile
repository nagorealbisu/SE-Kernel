# Variables
CC = gcc                      # Compilador
CFLAGS = -pthread -lm         # Banderas adicionales

# Archivos fuente
SRC1 = 1.zatia.c
SRC2 = 2.zatia.c
SRC3 = 3.zatia.c

# Ejecutables
TARGET1 = 1_zatia
TARGET2 = 2_zatia
TARGET3 = 3_zatia

all: $(TARGET1) $(TARGET2) $(TARGET3)

# Reglas principales
$(TARGET1): $(SRC1)
	$(CC) -o $(TARGET1) $(SRC1) $(CFLAGS)

$(TARGET2): $(SRC2)
	$(CC) -o $(TARGET2) $(SRC2) $(CFLAGS)

$(TARGET3): $(SRC3)
	$(CC) -o $(TARGET3) $(SRC3) $(CFLAGS)

# Limpiar archivos generados
clean:
	rm -f $(TARGET1) $(TARGET2) $(TARGET3)

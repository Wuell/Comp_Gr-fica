# Makefile do Peixe Faminto (arquivo unico: main.cpp)

# --- Linux ---  (gera o executavel "peixe")
all:
	g++ -std=c++17 -O2 main.cpp -o peixe -lGL -lGLU -lglut -lm

# --- Windows ---  (gera "peixe.exe", compilado a partir do Linux)
windows:
	x86_64-w64-mingw32-g++ -std=c++17 -O2 -fno-stack-protector main.cpp -o peixe.exe -lopengl32 -lglu32 -lfreeglut -mwindows

# Compila e ja roda (Linux)
run: all
	./peixe

# Apaga os executaveis gerados
clean:
	rm -f peixe peixe.exe

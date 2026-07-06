# Sapo Surfista 3D 🐸

Jogo *endless runner* (estilo Subway Surfers) feito em **C++ com OpenGL** (pipeline
de função fixa) + **GLUT**, para a disciplina de Computação Gráfica.

Um sapo corre sozinho sobre um píer de madeira de 3 pistas em cima da lagoa.
Você desvia de obstáculos e come moscas. A velocidade aumenta com o tempo.

---

## Como compilar e rodar (Linux)

Precisa ter os pacotes de desenvolvimento do OpenGL/GLUT. No Arch/CachyOS:

```bash
sudo pacman -S --needed freeglut mesa glu glew
```

Depois, dentro da pasta do projeto:

```bash
./compilar.sh      # ou:  g++ sapo.cpp -o sapo -lGLEW -lGL -lGLU -lglut -lm
./sapo
```

## Controles

| Tecla                | Ação                                             |
|----------------------|--------------------------------------------------|
| Seta ← / →  (ou A/D) | trocar de pista                                  |
| Seta ↑ / Espaço (W)  | **pular** (fast-fall com ↓/S no ar)              |
| Seta ↓ / S           | **rolar** por baixo (ou mergulhar rápido no ar)  |
| **T**                | liga/desliga os **shaders cartoon** (toon)       |
| R                    | reiniciar após game over                         |
| ESC / Q              | sair                                             |

### Os 3 tipos de obstáculo
- **Buraco na ponte** → *pule* por cima (ou desvie trocando de pista).
- **Tronco atravessado** → *role* por baixo.
- **Pedra** → *desvie* trocando de pista.

No **pulo**, apertar ↓/S faz o sapo **mergulhar rápido** (fast-fall) pro chão.

---

## Onde cada requisito do projeto está no código

Procure pelas etiquetas `[REQ:...]` dentro de `sapo.cpp`.

| Requisito (do PDF)                | Onde está / como foi feito                                                                 |
|-----------------------------------|--------------------------------------------------------------------------------------------|
| **Modelagem geométrica 3D**       | `desenhaSapo`, `desenhaMosca`, `caixa`, obstáculos — objetos montados a partir de esferas (`glutSolidSphere`), cubos (`glutSolidCube`) e quads. |
| **Transformação: translação**     | pulo (física com gravidade), troca de pista, mundo rolando em direção ao jogador.          |
| **Transformação: rotação**        | patas do sapo balançando, asas da mosca batendo, inclinação ao rolar.                      |
| **Transformação: escala**         | corpo do sapo (esfera → elipsoide via `glScalef`), a língua que estica ao comer, o "achatar" ao rolar. |
| **Iluminação**                    | `GL_LIGHTING` + `GL_LIGHT0` (o "sol") com componentes ambiente/difusa/especular (Phong).   |
| **Cor / material**                | `glColor` + `GL_COLOR_MATERIAL`; verde do sapo, madeira, água, obstáculos.                  |
| **Textura**                       | 3 texturas **geradas por código** (madeira, água, caixa) — sem arquivos externos.          |
| **Sombra**                        | `matrizSombra` projeta o sapo achatado no plano do chão (sombra planar), desenhado translúcido. |
| **Visibilidade (pontual)**        | Z-buffer: `glEnable(GL_DEPTH_TEST)`.                                                        |
| **Visibilidade (geométrico)**     | Back-face culling: `glEnable(GL_CULL_FACE)` + `glCullFace(GL_BACK)`.                        |

---

## Estrutura do código (`sapo.cpp`)

1. Constantes e estado (jogador, jogo, listas de obstáculos/moscas).
2. Texturas procedurais.
3. `initGL` — liga iluminação, z-buffer, culling, cria texturas.
4. Funções de desenho: `caixa`, `desenhaSapo`, `desenhaMosca`, obstáculos, cenário.
5. `matrizSombra` / `desenhaSombraDoSapo` — a sombra projetada.
6. `desenhaHUD` — placar (texto 2D).
7. `display` — monta a cena (câmera + todos os objetos).
8. `geraLinha` / `atualiza` — geração infinita do mundo + física + colisões.
9. `main` + callbacks do GLUT (teclado, timer ~60 fps).

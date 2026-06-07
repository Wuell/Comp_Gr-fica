/*
 * ============================================================================
 *  PEIXE FAMINTO  -  Projeto de Computacao Grafica (OpenGL / GLUT)
 *  Tudo em um unico arquivo.
 *
 *  IDEIA: voce controla o peixe LARANJA com as setas. Coma os peixes
 *  MENORES (voce cresce a cada um). Se encostar num peixe MAIOR (olhos
 *  vermelhos), ele te come e o jogo acaba. Comeu todos = vitoria.
 *
 *  As 3 transformacoes geometricas exigidas estao na funcao desenhaPeixe():
 *     glTranslatef -> mover     glRotatef -> girar     glScalef -> redimensionar
 *
 *  CONTROLES:  Setas = nadar    R = reiniciar    ESC ou Q = sair
 *
 *  Compilar (Linux):   make
 *  Compilar (Windows): make windows   ->  peixe.exe
 * ============================================================================
 */

#define _USE_MATH_DEFINES   // ajuda o Windows a conhecer o numero PI (M_PI)
#include <GL/glut.h>        // OpenGL + GLUT (janela, desenho, teclado)
#include <cmath>            // funcoes matematicas: sin, cos...
#include <cstdlib>          // rand (numeros aleatorios), exit
#include <cstdio>           // snprintf (montar textos)
#include <ctime>            // time (semente do aleatorio)

#ifndef M_PI
#define M_PI 3.14159265358979323846   // valor de PI (alguns compiladores nao tem)
#endif


/* ===========================================================================
 *  CONSTANTES E VARIAVEIS GLOBAIS
 * ======================================================================== */

const int W = 900;   // largura da janela (em pixels)
const int H = 600;   // altura  da janela

// "Cerca invisivel": ate onde o peixe do jogador pode nadar.
const float MIN_X = 60,  MAX_X = W - 60;   // bordas esquerda / direita
const float MIN_Y = 100, MAX_Y = H - 40;   // chao (acima da areia) / topo

float T = 0.0f;   // relogio do mundo: aumenta sempre e anima tudo (ondas, giros)


/* ===========================================================================
 *  1) PRIMITIVAS  -  as formas basicas que desenham tudo
 * ======================================================================== */

// Desenha uma elipse (ou circulo) preenchida, ponto a ponto ao redor do centro.
void elipse(float cx, float cy, float rx, float ry, int n = 40) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);                                     // centro
    for (int i = 0; i <= n; i++) {
        float a = 2.0f * M_PI * i / n;                     // angulo (volta inteira)
        glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a));  // ponto na borda
    }
    glEnd();
}

// Um circulo e so uma elipse com largura igual a altura.
void circulo(float cx, float cy, float r, int n = 32) {
    elipse(cx, cy, r, r, n);
}

// Escreve um texto na posicao (x, y).
void texto(float x, float y, const char* s, void* fonte) {
    glRasterPos2f(x, y);
    for (const char* p = s; *p; ++p)
        glutBitmapCharacter(fonte, *p);
}

// Mede a largura de um texto em pixels (usado para centralizar).
int larguraTexto(void* fonte, const char* s) {
    int w = 0;
    for (const char* p = s; *p; ++p) w += glutBitmapWidth(fonte, *p);
    return w;
}

// Escreve um texto centralizado na horizontal.
void textoCentralizado(float y, const char* s, void* fonte) {
    float x = (W - larguraTexto(fonte, s)) * 0.5f;
    texto(x, y, s, fonte);
}


/* ===========================================================================
 *  2) O PEIXE  -  dados + desenho (AQUI ESTAO AS 3 TRANSFORMACOES!)
 * ======================================================================== */

// Ficha de dados de um peixe.
struct Peixe {
    float x, y;              // posicao na tela
    float vx;                // velocidade horizontal
    float baseY, amp, fase;  // para nadar em "onda" (sobe e desce)
    float esc;               // tamanho (escala)
    float r, g, b;           // cor
    bool  viva;              // ainda esta na cena?
    bool  dir;               // esta virado para a direita?
};

// Desenha UM peixe na posicao / tamanho / cor pedidos.
// Se 'perigoso' (maior que o jogador), os olhos ficam vermelhos como aviso.
void desenhaPeixe(float x, float y, float escala,
                  float r, float g, float b,
                  float inclinacao, bool direita, bool perigoso) {
    glPushMatrix();   // salva o sistema de coordenadas atual

        // >>> AS TRES TRANSFORMACOES GEOMETRICAS <<<
        glTranslatef(x, y, 0.0f);                            // 1) MOVER ate (x, y)
        glRotatef(inclinacao, 0.0f, 0.0f, 1.0f);            // 2) GIRAR (inclina ao subir/descer)
        glScalef(direita ? escala : -escala, escala, 1.0f); // 3) ESCALAR (tamanho; X negativo = espelhar)

        // Corpo
        glColor3f(r, g, b);
        elipse(0, 0, 33, 15);

        // Cauda
        glColor3f(r * 0.72f, g * 0.72f, b * 0.72f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-28, 0); glVertex2f(-50, 20); glVertex2f(-50, -20);
        glEnd();

        // Nadadeiras
        glBegin(GL_TRIANGLES);
        glVertex2f(-8, 15); glVertex2f(12, 15); glVertex2f(4, 30);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(5, -12); glVertex2f(20, -12); glVertex2f(12, -24);
        glEnd();

        // Olho
        if (perigoso) {
            glColor3f(1.0f, 0.13f, 0.10f); circulo(17, 5, 5.5f); // olho vermelho
            glColor3f(1.0f, 0.65f, 0.60f); circulo(19, 6, 1.6f); // brilho
        } else {
            glColor3f(1, 1, 1);             circulo(17, 5, 5.5f);
            glColor3f(0.08f, 0.08f, 0.08f); circulo(18, 5, 2.8f);
            glColor3f(1, 1, 1);             circulo(19, 6, 1.0f);
        }

        // Boca
        glColor3f(r * 0.5f, 0.05f, 0.05f);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 8; i++) {
            float a = -M_PI / 5.0f + M_PI / 2.5f * i / 8.0f;
            glVertex2f(30 + 4 * cosf(a), -1 + 4 * sinf(a));
        }
        glEnd();

    glPopMatrix();   // restaura as coordenadas (nao afeta os outros desenhos)
}


/* ===========================================================================
 *  3) CENARIO  -  fundo do mar (agua, areia, algas, submarino, bolhas)
 * ======================================================================== */

// Agua: um retangulo com gradiente (escuro embaixo, mais claro em cima).
void desenhaFundo() {
    glBegin(GL_QUADS);
    glColor3f(0.0f, 0.06f, 0.30f);  glVertex2f(0, 0); glVertex2f(W, 0);
    glColor3f(0.05f, 0.28f, 0.62f); glVertex2f(W, H); glVertex2f(0, H);
    glEnd();
}

// Raios de luz descendo da superficie (transparentes).
void desenhaRaiosDeLuz() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float offsets[] = {120, 280, 460, 640, 820};
    for (float rx : offsets) {
        float w = 15.0f + 8.0f * sinf(T * 0.4f + rx * 0.005f);
        glColor4f(0.55f, 0.82f, 1.0f, 0.07f);
        glBegin(GL_TRIANGLES);
        glVertex2f(rx - w, H); glVertex2f(rx + w, H); glVertex2f(rx + 60, 0);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(rx - w, H); glVertex2f(rx + w, H); glVertex2f(rx - 60, 0);
        glEnd();
    }
    glDisable(GL_BLEND);
}

// Uma rocha (pedra cinza no fundo).
void desenhaRocha(float x, float y, float w, float h) {
    glBegin(GL_POLYGON);
    glVertex2f(x, y); glVertex2f(x + w*0.4f, y);
    glVertex2f(x + w, y + h*0.35f); glVertex2f(x + w*0.9f, y + h);
    glVertex2f(x + w*0.1f, y + h); glVertex2f(x - w*0.1f, y + h*0.5f);
    glEnd();
}

// Um coral (varias elipses em volta de um centro).
void desenhaCoral(float x, float y, float r, float rg, float rb) {
    glColor3f(r, rg, rb);
    for (int i = 0; i < 6; i++) {
        float a = 2.0f * M_PI * i / 6;
        elipse(x + 12 * cosf(a), y + 12 * sinf(a), 7, 12);
    }
    circulo(x, y, 8);
}

// Areia do fundo + dunas + rochas + corais.
void desenhaAreia() {
    glColor3f(0.78f, 0.71f, 0.48f);
    glBegin(GL_QUADS);
    glVertex2f(0,0); glVertex2f(W,0); glVertex2f(W,72); glVertex2f(0,72);
    glEnd();

    glColor3f(0.86f, 0.79f, 0.57f);
    float dx[] = {60, 240, 420, 600, 790};
    for (float x : dx) {
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, 72);
        for (int i = 0; i <= 32; i++) {
            float a = M_PI * i / 32;
            glVertex2f(x + 95 * cosf(a), 72 + 24 * sinf(a));
        }
        glEnd();
    }

    glColor3f(0.36f, 0.36f, 0.40f);
    desenhaRocha(155, 60, 52, 42);
    desenhaRocha(530, 63, 44, 36);
    desenhaRocha(725, 59, 62, 48);

    desenhaCoral(350, 75, 0.95f, 0.2f, 0.3f);
    desenhaCoral(680, 72, 0.85f, 0.4f, 0.1f);
}

// Uma alga que balanca: cada pedaco gira um pouco em relacao ao anterior.
void desenhaAlga(float baseX, float baseY, float altura, float oscMax, float fase) {
    const int segs = 7;
    float segH = altura / segs;
    glColor3f(0.07f, 0.52f, 0.17f);
    glPushMatrix();
        glTranslatef(baseX, baseY, 0.0f);
        for (int i = 0; i < segs; i++) {
            float angulo = oscMax * sinf(T * 1.1f + fase + i * 0.45f);
            glRotatef(angulo, 0.0f, 0.0f, 1.0f);   // ROTACAO encadeada
            glBegin(GL_QUADS);
            glVertex2f(-4.0f, 0.0f); glVertex2f(4.0f, 0.0f);
            glVertex2f(3.0f, segH);  glVertex2f(-3.0f, segH);
            glEnd();
            glTranslatef(0.0f, segH, 0.0f);
        }
    glPopMatrix();
}

// Submarino com janela e helice girando.
void desenhaSubmarino(float x, float y) {
    glPushMatrix();
        glTranslatef(x, y, 0.0f);   // TRANSLACAO: atravessa a tela

        glColor3f(0.72f, 0.62f, 0.04f);
        elipse(0, 0, 82, 30);

        glColor3f(0.62f, 0.53f, 0.04f);
        glBegin(GL_QUADS);
        glVertex2f(8,28); glVertex2f(36,28); glVertex2f(36,55); glVertex2f(8,55);
        glEnd();
        elipse(22, 55, 14, 8);

        glColor3f(0.50f, 0.50f, 0.52f);
        glLineWidth(3.5f);
        glBegin(GL_LINES);
        glVertex2f(28, 63); glVertex2f(28, 80);
        glVertex2f(28, 80); glVertex2f(44, 80);
        glEnd();
        glLineWidth(1.0f);
        circulo(46, 80, 5);

        glColor3f(0.45f, 0.82f, 1.0f);  circulo(18, 0, 13);
        glColor3f(0.28f, 0.62f, 0.88f); circulo(18, 0, 9);
        glColor3f(0.75f, 0.93f, 1.0f);  circulo(14, 4, 3.5f);

        // Helice (ROTACAO continua, sem parar)
        glPushMatrix();
            glTranslatef(-82, 0, 0);
            glRotatef(T * 280.0f, 0.0f, 0.0f, 1.0f);
            glColor3f(0.52f, 0.52f, 0.56f);
            for (int p = 0; p < 3; p++) {
                glBegin(GL_QUADS);
                glVertex2f(-3, 2); glVertex2f(3, 2);
                glVertex2f(2, 24); glVertex2f(-2, 24);
                glEnd();
                glRotatef(120, 0, 0, 1);
            }
        glPopMatrix();

        glColor3f(1.0f, 1.0f, 0.6f);
        circulo(80, 14, 5);
        circulo(80, -14, 5);
    glPopMatrix();
}

// Uma bolha que cresce conforme sobe.
void desenhaBolha(float x, float y, float r) {
    glPushMatrix();
        glTranslatef(x, y, 0.0f);
        glScalef(r, r, 1.0f);   // ESCALA: a bolha cresce conforme sobe

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(0.72f, 0.90f, 1.0f, 0.30f);
        circulo(0, 0, 1.0f);

        glColor4f(0.88f, 0.96f, 1.0f, 0.70f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 32; i++) {
            float a = 2.0f * M_PI * i / 32;
            glVertex2f(cosf(a), sinf(a));
        }
        glEnd();

        glColor4f(1, 1, 1, 0.80f);
        circulo(-0.32f, 0.38f, 0.20f);
        glDisable(GL_BLEND);
    glPopMatrix();
}

// Listas de bolhas e algas (posicoes fixas no cenario).
struct Bolha { float x, baseY, vel, wobble, tamanho; };
Bolha bolhas[] = {
    { 110, 10, 30, 0.0f, 6 }, { 240, 40, 38, 1.2f, 4 },
    { 390, 20, 25, 0.8f, 8 }, { 550, 60, 33, 2.1f, 5 },
    { 700, 10, 28, 1.5f, 7 }, { 180, 50, 42, 3.0f, 3 },
    { 500, 30, 35, 0.4f, 6 }, { 820, 15, 29, 1.8f, 4 },
};

struct Alga { float x, h, osc, fase; };
Alga algas[] = {
    {  55, 95, 13, 0.0f }, { 190, 115, 11, 0.8f },
    { 345, 88, 17, 1.6f }, { 480, 105, 10, 0.4f },
    { 645, 98, 19, 2.2f }, { 800, 82, 14, 1.1f },
};

float subX = -200.0f;   // posicao do submarino na horizontal

// Avanca a animacao do cenario (so o submarino se move por conta propria).
void atualizaCenario(float dt) {
    subX += 0.45f;
    if (subX > W + 220.0f) subX = -220.0f;   // sumiu na direita? volta pela esquerda
}

// Desenha o cenario inteiro, de tras para a frente.
void desenhaCenario() {
    desenhaFundo();
    desenhaRaiosDeLuz();
    desenhaAreia();

    for (Alga& a : algas)
        desenhaAlga(a.x, 72, a.h, a.osc, a.fase);

    desenhaSubmarino(subX, 450);

    for (Bolha& b : bolhas) {
        float y   = fmodf(b.baseY + b.vel * T, H + 20.0f);
        float x   = b.x + 9.0f * sinf(T * 1.4f + b.wobble);
        float esc = b.tamanho * (0.4f + 0.6f * y / H);
        desenhaBolha(x, y, esc);
    }
}


/* ===========================================================================
 *  4) JOGO  -  regras (jogador, peixes, colisoes, vitoria / derrota)
 * ======================================================================== */

// Estado do peixe do jogador
float px = W * 0.5f, py = H * 0.5f;   // posicao (comeca no meio da tela)
float pvx = 0.0f, pvy = 0.0f;          // velocidade
float pesc = 0.5f;                     // tamanho (comeca pequeno e cresce)
bool  facingRight = true;              // virado para a direita?
int   comidos = 0;                     // quantos peixes ja comeu
bool  fimDeJogo = false;
bool  venceu = false;

// Qual seta esta apertada agora
bool kLeft = false, kRight = false, kUp = false, kDown = false;

// Catalogo dos peixes: cada um tem so um TAMANHO e uma COR.
// (posicao, velocidade etc. sao sorteadas quando o jogo comeca)
// O jogador comeca com tamanho 0.50, entao os 3 primeiros sao menores
// (da pra comer) e os outros 13 sao MAIORES = perigo (olhos vermelhos).
struct Modelo { float esc, r, g, b; };
Modelo CATALOGO[] = {
    { 0.30f, 0.20f, 0.70f, 0.65f },
    { 0.38f, 0.65f, 0.35f, 0.80f },
    { 0.46f, 0.40f, 0.70f, 0.90f },
    { 0.55f, 0.35f, 0.78f, 0.45f },
    { 0.64f, 0.92f, 0.50f, 0.72f },
    { 0.73f, 0.45f, 0.62f, 0.82f },
    { 0.82f, 0.50f, 0.45f, 0.85f },
    { 0.92f, 0.25f, 0.68f, 0.58f },
    { 1.02f, 0.58f, 0.50f, 0.78f },
    { 1.12f, 0.30f, 0.72f, 0.78f },
    { 1.22f, 0.62f, 0.42f, 0.66f },
    { 1.35f, 0.35f, 0.50f, 0.80f },
    { 1.48f, 0.45f, 0.30f, 0.62f },
    { 1.60f, 0.25f, 0.50f, 0.40f },
    { 1.72f, 0.35f, 0.45f, 0.58f },
    { 1.80f, 0.30f, 0.35f, 0.50f },
};
const int N_PEIXES = sizeof(CATALOGO) / sizeof(CATALOGO[0]);

Peixe peixes[N_PEIXES];   // os peixes "de verdade" durante o jogo
int   vivos = N_PEIXES;   // quantos ainda estao vivos

// Sorteia um numero entre a e b.
float aleatorio(float a, float b) {
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

// (Re)comeca a partida: zera o jogador e re-sorteia todos os peixes.
void jogoReinicia() {
    px = W * 0.5f; py = H * 0.5f;
    pvx = pvy = 0.0f;
    pesc = 0.5f;
    facingRight = true;
    comidos = 0;
    fimDeJogo = false;
    venceu = false;
    vivos = N_PEIXES;

    for (int i = 0; i < N_PEIXES; i++) {
        Peixe& p = peixes[i];           // & = mexe no peixe de verdade
        p.esc = CATALOGO[i].esc;        // tamanho e cor vem do catalogo
        p.r   = CATALOGO[i].r;
        p.g   = CATALOGO[i].g;
        p.b   = CATALOGO[i].b;
        p.amp  = aleatorio(12.0f, 28.0f);
        p.fase = aleatorio(0.0f, 2.0f * M_PI);
        float vel = aleatorio(60.0f, 120.0f);
        p.vx  = (rand() % 2) ? vel : -vel;   // para a direita ou para a esquerda
        p.dir = (p.vx > 0);
        p.viva = true;
        // Nasce longe do jogador (para nao morrer assim que comeca).
        // A faixa de altura 160..540 mantem o peixe acima da areia.
        do {
            p.x     = aleatorio(120.0f, W - 120.0f);
            p.baseY = aleatorio(160.0f + p.amp, 540.0f - p.amp);
        } while ((p.x - px)*(p.x - px) + (p.baseY - py)*(p.baseY - py) < 150.0f*150.0f);
        p.y = p.baseY;
    }
}

// Chamada uma unica vez, no inicio do programa.
void jogoInicia() {
    srand((unsigned)time(NULL));   // sorteio diferente a cada vez que abre
    jogoReinicia();
}

// Move o peixe do jogador conforme as setas (com inercia, como na agua).
void atualizaJogador(float dt) {
    const float ACEL = 1800.0f;   // forca das setas
    const float DAMP = 0.89f;     // "freio" que cria a inercia

    if (kRight) pvx += ACEL * dt;
    if (kLeft)  pvx -= ACEL * dt;
    if (kUp)    pvy += ACEL * dt;
    if (kDown)  pvy -= ACEL * dt;

    pvx *= DAMP;
    pvy *= DAMP;

    px += pvx * dt;
    py += pvy * dt;

    // Nao deixa o peixe sair da "cerca"
    if (px < MIN_X) { px = MIN_X; pvx = 0; }
    if (px > MAX_X) { px = MAX_X; pvx = 0; }
    if (py < MIN_Y) { py = MIN_Y; pvy = 0; }
    if (py > MAX_Y) { py = MAX_Y; pvy = 0; }

    if (pvx >  15.0f) facingRight = true;
    if (pvx < -15.0f) facingRight = false;
}

// Move todos os peixes e resolve as colisoes com o jogador.
void atualizaPeixes(float dt) {
    for (Peixe& p : peixes) {
        if (!p.viva) continue;

        // Nada na horizontal e volta ao bater nas bordas
        p.x += p.vx * dt;
        if (p.x < 50)     { p.x = 50;     p.vx =  fabsf(p.vx); }
        if (p.x > W - 50) { p.x = W - 50; p.vx = -fabsf(p.vx); }
        p.dir = (p.vx > 0);
        p.y = p.baseY + p.amp * sinf(T * 0.75f + p.fase);  // sobe e desce (onda)

        // Encostou no jogador?
        float dx = px - p.x, dy = py - p.y;
        float alcance = 24.0f * pesc + 14.0f * p.esc;
        if (dx*dx + dy*dy < alcance * alcance) {
            if (pesc > p.esc) {
                // Jogador maior: COME o peixe e cresce um pouco
                p.viva = false;
                vivos--;
                comidos++;
                pesc += 0.09f;
                if (pesc > 3.0f) pesc = 3.0f;
            } else if (pesc < p.esc) {
                // Peixe maior: o jogador e comido -> DERROTA
                fimDeJogo = true;
                venceu = false;
                break;
            }
        }
    }
}

// Avanca um passo do jogo (chamada a cada quadro).
void jogoAtualiza(float dt) {
    if (fimDeJogo) return;
    atualizaJogador(dt);
    atualizaPeixes(dt);
    if (!fimDeJogo && vivos == 0) {   // comeu todos -> VITORIA
        fimDeJogo = true;
        venceu = true;
    }
}

// Placar e instrucoes no canto da tela.
void desenhaHUD() {
    char buf[64];

    glColor3f(1.0f, 1.0f, 1.0f);
    snprintf(buf, sizeof(buf), "Peixes comidos: %d / %d", comidos, N_PEIXES);
    texto(18, H - 28, buf, GLUT_BITMAP_HELVETICA_18);

    snprintf(buf, sizeof(buf), "Tamanho: x%.2f", pesc);
    texto(18, H - 50, buf, GLUT_BITMAP_HELVETICA_18);

    glColor3f(0.80f, 0.90f, 1.0f);
    texto(18, 30, "Coma os peixes menores. Fuja dos maiores (olhos vermelhos)!",
          GLUT_BITMAP_HELVETICA_12);
    texto(18, 14, "Setas: nadar   |   R: reiniciar   |   ESC: sair",
          GLUT_BITMAP_HELVETICA_12);
}

// Tela escura de fim de jogo (vitoria ou derrota).
void desenhaTelaFim() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.62f);
    glBegin(GL_QUADS);
    glVertex2f(0,0); glVertex2f(W,0); glVertex2f(W,H); glVertex2f(0,H);
    glEnd();
    glDisable(GL_BLEND);

    if (venceu) {
        glColor3f(1.0f, 0.85f, 0.25f);
        textoCentralizado(H * 0.5f + 40, "FIM DE JOGO", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1.0f, 1.0f, 1.0f);
        textoCentralizado(H * 0.5f, "Voce comeu todos os peixes! VOCE VENCEU!",
                          GLUT_BITMAP_HELVETICA_18);
    } else {
        glColor3f(1.0f, 0.35f, 0.30f);
        textoCentralizado(H * 0.5f + 40, "FIM DE JOGO", GLUT_BITMAP_TIMES_ROMAN_24);
        glColor3f(1.0f, 1.0f, 1.0f);
        textoCentralizado(H * 0.5f, "Um peixe maior comeu voce! VOCE PERDEU!",
                          GLUT_BITMAP_HELVETICA_18);
    }

    glColor3f(0.80f, 0.90f, 1.0f);
    textoCentralizado(H * 0.5f - 40,
                      "Pressione R para jogar de novo  ou  ESC para sair",
                      GLUT_BITMAP_HELVETICA_18);
}

// Desenha o jogo inteiro: peixes, jogador, placar e (se acabou) a tela de fim.
void jogoDesenha() {
    // Peixes; os maiores que o jogador ganham olhos vermelhos (perigo).
    for (Peixe& p : peixes) {
        if (!p.viva) continue;
        float inc = (p.dir ? 1.0f : -1.0f) * 5.0f * cosf(T * 0.75f + p.fase);
        desenhaPeixe(p.x, p.y, p.esc, p.r, p.g, p.b, inc, p.dir, pesc < p.esc);
    }

    // Peixe do jogador (laranja, para destacar)
    float tilt = pvy * 0.05f;
    if (tilt >  20.0f) tilt =  20.0f;
    if (tilt < -20.0f) tilt = -20.0f;
    float inclinacao = (facingRight ? 1.0f : -1.0f) * tilt;
    desenhaPeixe(px, py, pesc, 1.0f, 0.42f, 0.08f, inclinacao, facingRight, false);

    desenhaHUD();
    if (fimDeJogo) desenhaTelaFim();
}

// Guarda qual seta foi apertada (true) ou solta (false).
void jogoSeta(int key, bool pressionada) {
    if (key == GLUT_KEY_LEFT)  kLeft  = pressionada;
    if (key == GLUT_KEY_RIGHT) kRight = pressionada;
    if (key == GLUT_KEY_UP)    kUp    = pressionada;
    if (key == GLUT_KEY_DOWN)  kDown  = pressionada;
}


/* ===========================================================================
 *  5) PROGRAMA PRINCIPAL  -  liga o OpenGL e roda o jogo
 * ======================================================================== */

// Desenha um quadro inteiro (cenario + jogo).
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    desenhaCenario();
    jogoDesenha();
    glutSwapBuffers();   // mostra o quadro pronto de uma vez (sem piscar)
}

// O "motor" da animacao: roda ~60 vezes por segundo.
void update(int) {
    float dt = 0.016f;             // ~16 milissegundos por quadro
    T += dt;                       // adianta o relogio do mundo
    atualizaCenario(dt);
    jogoAtualiza(dt);
    glutPostRedisplay();           // pede para redesenhar a tela
    glutTimerFunc(16, update, 0);  // se reagenda -> vira um loop
}

// Ajusta a area de desenho quando a janela muda de tamanho.
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, W, 0, H);   // plano 2D: x 0..900, y 0..600 (origem embaixo-esq)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Teclas normais: ESC ou Q saem, R reinicia.
void teclado(unsigned char key, int, int) {
    if (key == 27 || key == 'q' || key == 'Q') exit(0);
    if (key == 'r' || key == 'R') jogoReinicia();
}

// Setas (teclas especiais): avisam quando sao apertadas / soltas.
void setaDown(int key, int, int) { jogoSeta(key, true);  }
void setaUp  (int key, int, int) { jogoSeta(key, false); }

int main(int argc, char** argv) {
    jogoInicia();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);   // buffer duplo (animacao suave)
    glutInitWindowSize(W, H);
    glutCreateWindow("Peixe Faminto - Computacao Grafica");

    glClearColor(0.0f, 0.06f, 0.30f, 1.0f);   // cor de fundo (azul escuro)
    glLineWidth(1.5f);

    glutDisplayFunc(display);     // quem desenha
    glutReshapeFunc(reshape);     // ao redimensionar a janela
    glutKeyboardFunc(teclado);    // teclas normais
    glutSpecialFunc(setaDown);    // setas apertadas
    glutSpecialUpFunc(setaUp);    // setas soltas
    glutTimerFunc(16, update, 0); // inicia o loop de animacao

    glutMainLoop();               // entrega o controle ao GLUT (roda ate sair)
    return 0;
}

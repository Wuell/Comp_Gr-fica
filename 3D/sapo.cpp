// ============================================================================
//  SAPO SURFISTA 3D  -  Endless runner estilo "Subway Surfers"
//  Disciplina: Computacao Grafica
//  Feito em C++ com OpenGL (pipeline de funcao fixa) + GLUT
//
//  Como compilar (Linux):
//      g++ sapo.cpp -o sapo -lGL -lGLU -lglut -lm
//  Como rodar:
//      ./sapo
//
//  CONTROLES:
//      MENU: digite o NOME e tecle ENTER para jogar
//      Seta ESQUERDA / DIREITA  -> troca de pista
//      Seta CIMA  ou  ESPACO    -> pula (passa por cima de obstaculo baixo)
//      Seta BAIXO ou  S         -> rola/abaixa (passa por baixo do obstaculo alto)
//      Coma MOSCAS para nao zerar a barra de FOME (senao: game over)
//      ENTER (no game over)     -> volta ao menu   |   ESC -> menu   |   Q -> sai
//
//  MAPA DOS REQUISITOS DO PROJETO (procure pelas tags [REQ:...] no codigo):
//      [REQ:MODELAGEM]   modelagem geometrica dos objetos 3D
//      [REQ:TRANSFORM]   translacao, rotacao e escala
//      [REQ:ILUMINACAO]  modelo de iluminacao (Phong via GL_LIGHT0)
//      [REQ:COR]         cores / materiais
//      [REQ:TEXTURA]     texturas (geradas por codigo, sem arquivos externos)
//      [REQ:SOMBRA]      sombra projetada no plano (planar shadow)
//      [REQ:VISIBILIDADE] z-buffer (pontual) + back-face culling (geometrico)
// ============================================================================

#include <GL/glew.h>     // precisa vir ANTES do glut (carrega as funcoes de shader)
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <map>
#include <algorithm>   // std::sort (recorte dos vaos no deck / placar)
#include <cctype>      // toupper (entrada de nome no menu)

// ----------------------------------------------------------------------------
//  CONSTANTES DO MUNDO
// ----------------------------------------------------------------------------
static const float PI = 3.14159265f;

// As 3 pistas ficam nestas posicoes X (esquerda, centro, direita).
static const float POS_PISTA[3] = { -2.4f, 0.0f, 2.4f };
static const float LARG_PISTA   = 2.2f;   // largura de cada tabua/pista

static const float Z_SPAWN = -85.0f;  // onde os objetos nascem (longe, na frente)
static const float Z_MORTE =  10.0f;  // depois disso (atras da camera) a gente recicla
static const float ESPACO_LINHA = 9.0f; // distancia entre "linhas" de obstaculos

static const float GRAVIDADE   = 26.0f;  // puxa o sapo pra baixo durante o pulo
static const float FORCA_PULO   = 9.0f;   // velocidade vertical inicial do pulo
static const float ALTURA_LIVRE = 0.9f;   // acima disso o sapo "passa por cima" do baixo

// ----------------------------------------------------------------------------
//  ESTADO DO JOGADOR (o sapo)
// ----------------------------------------------------------------------------
int   pista       = 1;       // 0=esq, 1=centro, 2=dir
float sapoX       = 0.0f;    // posicao X atual (desliza suave ate a pista alvo)
float sapoY       = 0.0f;    // altura (0 = no chao; sobe ao pular)  [REQ:TRANSFORM]
float velY        = 0.0f;    // velocidade vertical (fisica do pulo)
bool  pulando     = false;
bool  rolando     = false;
float tempoRola   = 0.0f;    // quanto tempo ainda fica abaixado
float lingua      = 0.0f;    // 0..1 -> quanto a lingua esta esticada (ao comer) [REQ:TRANSFORM/escala]

// ----------------------------------------------------------------------------
//  ESTADO GERAL DO JOGO
// ----------------------------------------------------------------------------
float velocidade  = 14.0f;   // velocidade que o mundo vem em nossa direcao
float distancia   = 0.0f;    // metros percorridos (rola a textura do piso)
int   pontos      = 0;       // moscas comidas
float tempo       = 0.0f;    // relogio global (segundos) para animacoes
int   larguraTela = 1000, alturaTela = 700;

// ---- MAQUINA DE ESTADOS: menu -> jogando -> game over ----
enum EstadoJogo { MENU, JOGANDO, GAMEOVER };
EstadoJogo estado = MENU;
std::string nomeJogador = "";          // digitado no menu

// ---- BARRA DE FOME: cai com o tempo; comer mosca reenche; zerou = fim ----
float fome = 1.0f;                                  // 0..1
static const float FOME_TAXA  = 1.0f / 26.0f;       // esvazia em ~26s sem comer
static const float FOME_GANHO = 0.10f;              // cada mosca reenche 10%

// ---- PLACAR (salvo em placar.txt) ----
struct Score { std::string nome; int moscas; int dist; int pts; };
std::vector<Score> placar;

// ----------------------------------------------------------------------------
//  OBSTACULOS e MOSCAS  (listas que crescem/reciclam -> mundo infinito)
// ----------------------------------------------------------------------------
enum TipoObst { OBST_BAIXO, OBST_ALTO, OBST_BLOCO };
// OBST_BAIXO : caixa baixa    -> PULE por cima
// OBST_ALTO  : barra suspensa -> ROLE por baixo
// OBST_BLOCO : bloco alto     -> DESVIE trocando de pista

struct Obstaculo { TipoObst tipo; int pista; float z; };
struct Mosca     { int pista; float z; float altura; };

std::vector<Obstaculo> obstaculos;
std::vector<Mosca>     moscas;
float distProxLinha = 0.0f;      // distancia (m) em que a proxima linha sera criada

// ----------------------------------------------------------------------------
//  IDs das TEXTURAS (geradas por codigo em criaTexturas())  [REQ:TEXTURA]
// ----------------------------------------------------------------------------
GLuint texMadeira, texAgua, texCaixa, texGrama;
GLuint texBranca = 0;         // textura 1x1 branca (p/ objetos SEM textura no shader)

// ---- SHADERS (objetos + agua) ----
GLuint progToon = 0, progAgua = 0;
bool   shadersOK  = false;    // false -> cai no pipeline classico (fallback seguro)
bool   usarShader = true;     // tecla T liga/desliga

// Esfera unitaria compilada UMA vez (reusada por sapo/sombra/moscas -> rapido).
GLuint listaEsfera = 0;
GLuint listaEsferaLo = 0;   // esfera de BAIXA resolucao p/ pecas pequenas (mosca)

// Display list do corpo do sapo (do sapo.obj, ou do fallback se faltar).
GLuint listaSapo = 0;
bool   sapoDeOBJ = false;    // true se carregou de sapo.obj (senao, malha gerada)
GLuint listaMosca = 0;       // modelo .obj da mosca (opcional)
bool   moscaDeOBJ = false;   // true se carregou moscas.obj (senao, mosca procedural)

// AJUSTES do modelo do SAPO importado (mexa aqui se ele nascer torto/afundado):
static const float SAPO_OBJ_GIRA_Y = -90.0f; // graus em Y p/ alinhar a "frente"
static const float SAPO_OBJ_PISO   = -0.42f; // altura dos "pes" no modelo local
static const float SAPO_OBJ_TAM    =  1.7f;  // tamanho no maior eixo

// Forward-declarations (initGL, la embaixo, precisa chamar estas antes da
// definicao delas mais adiante no arquivo):
// carregaOBJ agora e GENERICO: le qualquer .obj e RETORNA uma display list
// (0 = falhou). Parametros orientam/escalam o modelo (sapo, mosca, etc.).
GLuint carregaOBJ(const char* caminho, float giraY, float piso, float tam);
void geraMalhaSapo();                    // fallback do sapo (se sapo.obj faltar)
void geraLinha(float z);                 // spawn de obstaculos/moscas

// ============================================================================
//  1) TEXTURAS PROCEDURAIS
//     Em vez de carregar imagens de arquivo (o que exigiria bibliotecas
//     externas), a gente CALCULA os pixels. Cada textura eh uma matriz RGB.
// ============================================================================

// pequeno "ruido" pseudo-aleatorio estavel, so pra dar textura
float ruido(int x, int y) {
    int n = x * 57 + y * 131;
    n = (n << 13) ^ n;
    return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}

GLuint fazTextura(int lado, void (*corDoPixel)(int, int, unsigned char*)) {
    std::vector<unsigned char> img(lado * lado * 3);
    for (int y = 0; y < lado; y++)
        for (int x = 0; x < lado; x++)
            corDoPixel(x, y, &img[(y * lado + x) * 3]);

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    // GL_REPEAT permite "rolar" a textura (importante pro piso do runner)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // MIPMAP trilinear no MIN: usa as versoes reduzidas ja geradas abaixo ->
    // a madeira/agua distante para de "chiar" (menos aliasing, praticamente de graca).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, lado, lado, GL_RGB, GL_UNSIGNED_BYTE, img.data());
    return id;
}

// MADEIRA CARTOON (textura 128): tabuas com dois tons chapados alternados e
// juntas escuras NITIDAS. Pouco ruido -> visual limpo, estilo desenho.
void pixelMadeira(int x, int y, unsigned char* p) {
    const int bw = 32;                       // largura da tabua (px)
    int board = x / bw, bx = x % bw;
    float tone = (board & 1) ? 1.00f : 0.87f;         // 2 tons chapados
    tone *= 0.95f + 0.05f * sinf(y * 0.20f + board * 2.0f);  // grao bem suave
    float b = tone;
    if (bx < 2 || bx > bw - 3)      b = 0.42f;         // junta vertical (escura)
    else if (bx < 4 || bx > bw - 5) b *= 0.80f;        // sombrinha ao lado da junta
    if (y % 64 < 2)                 b = 0.42f;          // junta de topo ocasional
    p[0] = (unsigned char)fminf(255, 208 * b);
    p[1] = (unsigned char)fminf(255, 150 * b);
    p[2] = (unsigned char)fminf(255,  90 * b);
}

// AGUA CARTOON: ondas suaves em faixas + brilhinhos, azul saturado.
void pixelAgua(int x, int y, unsigned char* p) {
    float w  = sinf(x * 0.10f) * cosf(y * 0.11f);
    float w2 = sinf((x + y) * 0.055f);
    float t  = 0.5f + 0.34f * w + 0.16f * w2;
    float br = (t > 0.82f) ? 1.0f : 0.0f;              // faisca DOURADA do sol
    p[0] = (unsigned char)fminf(255,  18 + 42  * t + 190 * br);  // R baixo (agua) + ouro no brilho
    p[1] = (unsigned char)fminf(255,  95 + 70  * t + 140 * br);
    p[2] = (unsigned char)fminf(255, 120 + 55  * t +  55 * br);
}

// ENGRADADO CARTOON: moldura escura grossa + tabuas em cruz + grao leve.
void pixelCaixa(int x, int y, unsigned char* p) {
    bool moldura = (x < 6 || x > 121 || y < 6 || y > 121);
    bool cruzV   = (x > 59 && x < 69);
    bool cruzH   = (y > 59 && y < 69);
    float b;
    if (moldura)            b = 0.45f;
    else if (cruzV || cruzH) b = 0.58f;
    else                     b = 0.86f + 0.06f * sinf(x * 0.35f);
    p[0] = (unsigned char)fminf(255, 205 * b);
    p[1] = (unsigned char)fminf(255, 152 * b);
    p[2] = (unsigned char)fminf(255,  88 * b);
}

// GRAMA CARTOON: verde com manchas de tom (relva) + pontinhos claros/escuros.
void pixelGrama(int x, int y, unsigned char* p) {
    float n  = ruido(x, y) * 0.5f + ruido(x/3, y/3) * 0.5f;   // relva em 2 escalas
    float t  = 0.80f + 0.20f * n;
    float tuft = (ruido(x/2, y/2) > 0.75f) ? 0.18f : 0.0f;    // tufos mais claros
    p[0] = (unsigned char)fminf(255,  70 * t + 25*tuft);
    p[1] = (unsigned char)fminf(255, 150 * t + 55*tuft);
    p[2] = (unsigned char)fminf(255,  60 * t + 20*tuft);
}

void criaTexturas() {
    texMadeira = fazTextura(128, pixelMadeira);
    texAgua    = fazTextura(128, pixelAgua);
    texCaixa   = fazTextura(128, pixelCaixa);
    texGrama   = fazTextura(128, pixelGrama);
    // textura 1x1 branca: objetos sem textura (sapo, obstaculos) amostram ela
    // no shader e a cor final vira apenas a cor do vertice.
    unsigned char branco[3] = { 255, 255, 255 };
    glGenTextures(1, &texBranca);
    glBindTexture(GL_TEXTURE_2D, texBranca);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, branco);
}

// ============================================================================
//  1b) SHADERS GLSL 1.20 (sombreado flat dos objetos + agua). Leem o que o
//      pipeline fixo ja envia; se falharem, shadersOK=false e roda no modo classico.
// ============================================================================

// --- VERTEX comum (toon/agua de objetos): passa normal, posicao-olho, cor, uv
static const char* VS_TOON =
"#version 120\n"
"varying vec3 N; varying vec3 P;\n"
"void main(){\n"
"  N = gl_NormalMatrix * gl_Normal;\n"
"  P = vec3(gl_ModelViewMatrix * gl_Vertex);\n"
"  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
"  gl_FrontColor = gl_Color;\n"
"  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
"}\n";

// --- FRAGMENT (estilo FLAT / low-poly limpo): difusa SUAVE (sem faixas duras),
//     cor solida chapada, sem brilho de plastico, sem rim. So a forma + nevoa.
static const char* FS_TOON =
"#version 120\n"
"varying vec3 N; varying vec3 P;\n"
"uniform sampler2D tex;\n"
"uniform vec3 fogColor; uniform float fogStart, fogEnd;\n"
"void main(){\n"
"  vec3 n = normalize(N);\n"
"  vec3 L = normalize(gl_LightSource[0].position.xyz - P);\n"
"  float d = max(dot(n,L), 0.0);\n"
"  float lit = 0.62 + 0.38*d;\n"                             // sombreado SUAVE e claro
"  vec4 t = texture2D(tex, gl_TexCoord[0].st);\n"
"  vec3 base = gl_Color.rgb * t.rgb;\n"
"  vec3 col = base * lit;\n"                                 // cor solida, chapada
"  float dist = -P.z;\n"
"  float f = clamp((fogEnd - dist)/(fogEnd - fogStart), 0.0, 1.0);\n"
"  col = mix(fogColor, col, f);\n"
"  gl_FragColor = vec4(col, gl_Color.a);\n"
"}\n";


// --- AGUA: uma onda desloca o vertice e gera a normal analitica (relevo) ---
static const char* VS_AGUA =
"#version 120\n"
"uniform float tempo;\n"
"varying vec3 N; varying vec3 P; varying float H; varying float WX;\n"
"void main(){\n"
"  vec4 v = gl_Vertex;\n"
"  float a=0.12, k=0.40;\n"
"  float w = a*sin(v.x*k + v.z*0.5 + tempo*1.4);\n"
"  v.y += w; H = w; WX = v.x;\n"
"  float dwdx = a*k*cos(v.x*k + v.z*0.5 + tempo*1.4);\n"
"  N = gl_NormalMatrix * normalize(vec3(-dwdx, 1.0, -dwdx*0.8));\n"
"  P = vec3(gl_ModelViewMatrix * v);\n"
"  gl_Position = gl_ModelViewProjectionMatrix * v;\n"
"}\n";
static const char* FS_AGUA =
"#version 120\n"
"varying vec3 N; varying vec3 P; varying float H; varying float WX;\n"
"uniform vec3 fogColor; uniform float fogStart, fogEnd; uniform float tempo;\n"
"void main(){\n"
"  vec3 n = normalize(N);\n"
"  vec3 L = normalize(gl_LightSource[0].position.xyz - P);\n"
"  vec3 V = normalize(-P);\n"
"  vec3 Hh = normalize(L+V);\n"
"  float sp = pow(max(dot(n,Hh),0.0), 60.0);\n"
"  float h = clamp(H*3.2 + 0.5, 0.0, 1.0);\n"          // cava escura -> crista clara
"  vec3 fundo  = vec3(0.03,0.15,0.23);\n"                   // cava: petroleo fundo
"  vec3 crista = vec3(0.13,0.44,0.52);\n"                   // crista: turquesa
"  vec3 col = mix(fundo, crista, h);\n"
"  float foam = smoothstep(0.07, 0.11, H);\n"              // espuma nas cristas
"  col = mix(col, vec3(0.80,0.92,0.95), foam*0.4);\n"
"  col += sp * vec3(1.0,0.92,0.72);\n"                      // brilho do sol
"  float dist = -P.z;\n"
"  float horiz = clamp((dist-30.0)/40.0, 0.0, 1.0);\n"     // reflexo dourado no horizonte
"  float shim = 0.5 + 0.5*sin(WX*2.5 + dist*0.6 - tempo*3.0);\n"
"  col += vec3(1.0,0.72,0.40) * horiz*horiz * 0.4 * shim;\n"
"  float f = clamp((fogEnd - dist)/(fogEnd - fogStart), 0.0, 1.0);\n"
"  col = mix(fogColor, col, f);\n"
"  gl_FragColor = vec4(col, 1.0);\n"
"}\n";

GLuint compilaShader(GLenum tipo, const char* src, const char* nome) {
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, 1024, NULL, log);
        printf("[SHADER] ERRO ao compilar %s:\n%s\n", nome, log);
        glDeleteShader(s); return 0;
    }
    return s;
}
GLuint linkaPrograma(const char* vs, const char* fs, const char* nome) {
    GLuint v = compilaShader(GL_VERTEX_SHADER,   vs, nome);
    GLuint f = compilaShader(GL_FRAGMENT_SHADER, fs, nome);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(v); glDeleteShader(f);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p,1024,NULL,log);
               printf("[SHADER] ERRO ao linkar %s:\n%s\n", nome, log); return 0; }
    return p;
}
void initShaders() {
    progToon = linkaPrograma(VS_TOON, FS_TOON, "toon");
    progAgua = linkaPrograma(VS_AGUA, FS_AGUA, "agua");
    shadersOK = (progToon && progAgua);
    if (shadersOK) {
        glUseProgram(progToon);
        glUniform1i(glGetUniformLocation(progToon, "tex"), 0);  // sampler na unidade 0
        glUseProgram(0);
    }
}

// passa fog (mesmos valores da nevoa fixa) para um programa
void enviaFog(GLuint prog) {
    glUniform3f(glGetUniformLocation(prog,"fogColor"), 0.99f,0.78f,0.60f);
    glUniform1f(glGetUniformLocation(prog,"fogStart"), 34.0f);
    glUniform1f(glGetUniformLocation(prog,"fogEnd"),   92.0f);
}

// ============================================================================
//  2) INICIALIZACAO DO OPENGL
// ============================================================================
// Cor do horizonte/neblina: a agua e o ceu fundem nesta cor la longe.
static const GLfloat COR_NEVOA[4] = { 0.99f, 0.78f, 0.60f, 1.0f };

void initGL() {
    // GLEW carrega as funcoes de shader. Se falhar, seguimos sem shaders.
    GLenum ge = glewInit();
    if (ge != GLEW_OK) printf("[GLEW] falhou (%s) -> modo classico\n", glewGetErrorString(ge));

    glClearColor(COR_NEVOA[0], COR_NEVOA[1], COR_NEVOA[2], 1.0f);

    // ---- NEVOA (fog) atmosferica ----
    // Faz os objetos distantes desbotarem ate a cor do horizonte. Isso: (1) da
    // profundidade/atmosfera, (2) ESCONDE a "costura" dura onde a agua encontra
    // o ceu e (3) mascara o "pop-in" dos obstaculos que nascem la longe.
    glEnable(GL_FOG);
    glFogfv(GL_FOG_COLOR, COR_NEVOA);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 34.0f);
    glFogf(GL_FOG_END,   92.0f);
    glHint(GL_FOG_HINT, GL_NICEST);

    // Suaviza as bordas serrilhadas das linhas/pontos (o multisample da janela
    // cuida dos poligonos; isto ajuda no restante).
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glEnable(GL_MULTISAMPLE);

    // ---- [REQ:VISIBILIDADE] ----
    // Z-BUFFER (algoritmo PONTUAL / por pixel): pra cada pixel guarda a
    // profundidade e so pinta o fragmento mais proximo -> quem esta atras
    // fica escondido corretamente.
    glEnable(GL_DEPTH_TEST);
    // BACK-FACE CULLING (algoritmo GEOMETRICO / por poligono): descarta as
    // faces que apontam pro lado contrario da camera. Economiza e evita
    // desenhar o "avesso" dos objetos solidos.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // ---- [REQ:ILUMINACAO] ----
    // Modelo de iluminacao de Phong do OpenGL fixo, com uma fonte (o "sol").
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    GLfloat luzAmbiente[]  = { 0.42f, 0.34f, 0.38f, 1.0f }; // fill quente/rosado (por do sol)
    GLfloat luzDifusa[]    = { 1.05f, 0.82f, 0.55f, 1.0f }; // sol DOURADO quente
    GLfloat luzEspecular[] = { 1.0f,  0.92f, 0.75f, 1.0f }; // brilho ambar
    glLightfv(GL_LIGHT0, GL_AMBIENT,  luzAmbiente);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  luzDifusa);
    glLightfv(GL_LIGHT0, GL_SPECULAR, luzEspecular);

    // ATENUACAO pela distancia: a luz enfraquece longe da fonte. Isso deixa a
    // madeira clara perto do sapo e escura ao fundo -> o efeito da luz fica
    // MUITO mais visivel na tabua (antes parecia chapada).  [REQ:ILUMINACAO]
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION,  1.0f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION,    0.006f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0004f);

    // Deixa o glColor(...) definir a cor do material (mais simples que
    // ficar chamando glMaterialfv o tempo todo).  [REQ:COR]
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    GLfloat brilho[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, brilho);
    glMateriali(GL_FRONT, GL_SHININESS, 40);

    // Como usamos glScalef (escala) nos modelos, as normais precisam ser
    // renormalizadas, senao a iluminacao fica errada.  [REQ:TRANSFORM]
    glEnable(GL_NORMALIZE);

    glShadeModel(GL_SMOOTH);              // interpola cor entre vertices (Gouraud)
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // textura * luz

    criaTexturas();

    // Compila a esfera unitaria (raio 1, 24x18) numa display list reutilizavel.
    listaEsfera = glGenLists(1);
    glNewList(listaEsfera, GL_COMPILE);
      glutSolidSphere(1.0, 24, 18);
    glEndList();
    listaEsferaLo = glGenLists(1);
    glNewList(listaEsferaLo, GL_COMPILE);
      glutSolidSphere(1.0, 10, 8);      // poucos poligonos -> barato p/ a mosca
    glEndList();

    // Sapo: modelo sapo.obj (ou fallback simples se faltar). Mosca: moscas.obj.
    listaSapo = carregaOBJ("sapo.obj", SAPO_OBJ_GIRA_Y, SAPO_OBJ_PISO, SAPO_OBJ_TAM);
    if (listaSapo) sapoDeOBJ = true; else geraMalhaSapo();
    listaMosca = carregaOBJ("moscas.obj", 0.0f, -0.18f, 0.36f);
    moscaDeOBJ = (listaMosca != 0);

    initShaders();
    srand((unsigned)time(NULL));
}

// ============================================================================
//  3) PRIMITIVAS AUXILIARES DE DESENHO
// ============================================================================

// Uma caixa (paralelepipedo) com base no y=0, centrada em X e Z.
// Desenhada face a face com NORMAIS (pra iluminar) e COORDENADAS DE TEXTURA.
// [REQ:MODELAGEM] geometria (vertices) + topologia (como formam as 6 faces)
void caixa(float lx, float ly, float lz, bool comTextura) {
    float x = lx * 0.5f, z = lz * 0.5f, y = ly;
    if (comTextura) { glEnable(GL_TEXTURE_2D); glColor3f(1, 1, 1); }
    glBegin(GL_QUADS);
      // frente (+Z)
      glNormal3f(0, 0, 1);
      glTexCoord2f(0,0); glVertex3f(-x,0, z); glTexCoord2f(1,0); glVertex3f( x,0, z);
      glTexCoord2f(1,1); glVertex3f( x,y, z); glTexCoord2f(0,1); glVertex3f(-x,y, z);
      // tras (-Z)
      glNormal3f(0, 0, -1);
      glTexCoord2f(0,0); glVertex3f( x,0,-z); glTexCoord2f(1,0); glVertex3f(-x,0,-z);
      glTexCoord2f(1,1); glVertex3f(-x,y,-z); glTexCoord2f(0,1); glVertex3f( x,y,-z);
      // direita (+X)
      glNormal3f(1, 0, 0);
      glTexCoord2f(0,0); glVertex3f( x,0, z); glTexCoord2f(1,0); glVertex3f( x,0,-z);
      glTexCoord2f(1,1); glVertex3f( x,y,-z); glTexCoord2f(0,1); glVertex3f( x,y, z);
      // esquerda (-X)
      glNormal3f(-1, 0, 0);
      glTexCoord2f(0,0); glVertex3f(-x,0,-z); glTexCoord2f(1,0); glVertex3f(-x,0, z);
      glTexCoord2f(1,1); glVertex3f(-x,y, z); glTexCoord2f(0,1); glVertex3f(-x,y,-z);
      // topo (+Y)
      glNormal3f(0, 1, 0);
      glTexCoord2f(0,0); glVertex3f(-x,y, z); glTexCoord2f(1,0); glVertex3f( x,y, z);
      glTexCoord2f(1,1); glVertex3f( x,y,-z); glTexCoord2f(0,1); glVertex3f(-x,y,-z);
    glEnd();
    if (comTextura) glDisable(GL_TEXTURE_2D);
}

// ============================================================================
//  4) O SAPO  [REQ:MODELAGEM] - montado com varios elipsoides: corpo largo e
//     achatado, olhos esbugalhados no topo, boca larga, pernas dianteiras finas
//     e pernas TRASEIRAS dobradas (a "cara de sapo").
//     [REQ:TRANSFORM] cada parte usa translate / rotate / scale.
// ============================================================================
const float VERDE_SAPO[3]  = { 0.36f, 0.66f, 0.30f };  // verde base do dorso
const float VERDE_CLARO[3] = { 0.55f, 0.80f, 0.42f };  // laterais/cabeca (destaque)
const float VERDE_ESCURO[3]= { 0.22f, 0.44f, 0.20f };  // manchas de sapo no dorso
const float CREME_SAPO[3]  = { 0.86f, 0.90f, 0.62f };  // barriga/queixo claro

// Ajusta o brilho especular do material atual. O sapo fica com brilho SUAVE
// (menos "plastico"); depois restauramos para os obstaculos/agua brilharem.
void brilhoMaterial(float esp, int shininess) {
    GLfloat s[] = { esp, esp, esp, 1.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, s);
    glMateriali(GL_FRONT, GL_SHININESS, shininess);
}

// Desenha uma esfera esticada (elipsoide) -> o "tijolo" que monta o sapo.
// slices/stacks altos (24x18) = MAIS POLIGONOS = superficie bem mais lisa.
void blob(float x, float y, float z, float sx, float sy, float sz, float r) {
    glPushMatrix();
      glTranslatef(x, y, z);
      // ESCALA -> deforma a esfera unitaria (da display list) em elipsoide.
      // GL_NORMALIZE ja corrige as normais depois da escala nao-uniforme.
      glScalef(sx * r, sy * r, sz * r);
      glCallList(listaEsfera);       // reusa a malha compilada (rapido)
    glPopMatrix();
}

// versao de BAIXA resolucao (pecas pequenas: mosca) -> mais leve
void blobLo(float x, float y, float z, float sx, float sy, float sz, float r) {
    glPushMatrix();
      glTranslatef(x, y, z);
      glScalef(sx * r, sy * r, sz * r);
      glCallList(listaEsferaLo);
    glPopMatrix();
}

// ----------------------------------------------------------------------------
//  Utilitarios de vetor (usados pelo loader de .obj)
// ----------------------------------------------------------------------------
struct Vec3 { float x, y, z; };
static Vec3 vsub(Vec3 a, Vec3 b){ return { a.x-b.x, a.y-b.y, a.z-b.z }; }
static Vec3 vcross(Vec3 a, Vec3 b){ return { a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x }; }
static Vec3 vnorm(Vec3 v){ float m=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z); if(m<1e-9f)m=1; return {v.x/m,v.y/m,v.z/m}; }

// ============================================================================
//  4a) LOADER DE .OBJ (+ .mtl)  -  modelo poligonal externo, sem bibliotecas.
//      Le v/vn/f (triangula faces), pega a cor Kd de cada material, normaliza
//      orientacao/escala e compila numa display list.
// ============================================================================

static Vec3 rotYv(Vec3 p, float deg){
    float a=deg*PI/180.0f, c=cosf(a), s=sinf(a);
    return { p.x*c + p.z*s, p.y, -p.x*s + p.z*c };
}
// interpreta um nome de material como cor hexadecimal "RRGGBB" (fallback do .mtl)
static bool corHex(const char* s, Vec3& c){
    int r,g,b;
    if (strlen(s)>=6 && sscanf(s,"%2x%2x%2x",&r,&g,&b)==3){ c={r/255.f,g/255.f,b/255.f}; return true; }
    return false;
}
// le Kd de cada material do .mtl para o mapa nome->cor
static void carregaMTL(const char* fn, std::map<std::string,Vec3>& mats){
    FILE* f=fopen(fn,"r"); if(!f) return;
    char l[512]; std::string cur;
    while (fgets(l,sizeof l,f)){
        char nome[256]; float r,g,b;
        if (sscanf(l," newmtl %255s", nome)==1) cur=nome;
        else if (!cur.empty() && sscanf(l," Kd %f %f %f",&r,&g,&b)==3) mats[cur]={r,g,b};
    }
    fclose(f);
}

GLuint carregaOBJ(const char* caminho, float giraY, float piso, float tam) {
    FILE* fp = fopen(caminho, "r");
    if (!fp) return 0;
    std::vector<Vec3> vs, ns;
    struct Tri { int v[3]; int n[3]; Vec3 col; };
    std::vector<Tri> tris;
    std::map<std::string,Vec3> mats;
    Vec3 corAtual = { VERDE_SAPO[0], VERDE_SAPO[1], VERDE_SAPO[2] };
    char linha[1024];
    while (fgets(linha, sizeof linha, fp)) {
        if (linha[0]=='v' && linha[1]==' ') {
            Vec3 p; if (sscanf(linha+2, "%f %f %f", &p.x,&p.y,&p.z)==3) vs.push_back(p);
        } else if (linha[0]=='v' && linha[1]=='n') {
            Vec3 p; if (sscanf(linha+3, "%f %f %f", &p.x,&p.y,&p.z)==3) ns.push_back(p);
        } else if (strncmp(linha,"mtllib",6)==0) {
            char fn[256]; if (sscanf(linha+6," %255s", fn)==1) carregaMTL(fn, mats);
        } else if (strncmp(linha,"usemtl",6)==0) {
            char nome[256];
            if (sscanf(linha+6," %255s", nome)==1) {
                auto it=mats.find(nome);
                if (it!=mats.end()) corAtual=it->second;              // cor do .mtl
                else if (!corHex(nome, corAtual))                    // ou hex do nome
                    corAtual = { VERDE_SAPO[0], VERDE_SAPO[1], VERDE_SAPO[2] };
            }
        } else if (linha[0]=='f' && linha[1]==' ') {
            int vi[32], ni[32], cnt=0;
            for (char* tok = strtok(linha+2, " \t\r\n"); tok && cnt<32; tok = strtok(NULL," \t\r\n")) {
                int a=0,b=0,c=0;
                if      (sscanf(tok,"%d/%d/%d",&a,&b,&c)==3) {}
                else if (sscanf(tok,"%d//%d",&a,&c)==2)      {}
                else if (sscanf(tok,"%d/%d",&a,&b)==2)       { c=0; }
                else    { sscanf(tok,"%d",&a); c=0; }
                vi[cnt] = a>0 ? a-1 : (int)vs.size()+a;                 // negativo = relativo ao fim
                ni[cnt] = c>0 ? c-1 : (c<0 ? (int)ns.size()+c : -1);
                cnt++;
            }
            for (int k=1; k+1<cnt; k++) {                              // leque de triangulos
                Tri t; t.v[0]=vi[0]; t.v[1]=vi[k]; t.v[2]=vi[k+1];
                       t.n[0]=ni[0]; t.n[1]=ni[k]; t.n[2]=ni[k+1];
                       t.col = corAtual;
                tris.push_back(t);
            }
        }
    }
    fclose(fp);
    if (vs.empty() || tris.empty()) return 0;

    // 1) normaliza ORIENTACAO (gira em Y) nos vertices e nas normais
    for (Vec3& p : vs) p = rotYv(p, giraY);
    for (Vec3& p : ns) p = rotYv(p, giraY);

    // 2) centraliza em X/Z, escala uniforme, e apoia a base em 'piso'
    Vec3 mn=vs[0], mx=vs[0];
    for (Vec3& p : vs) {
        mn.x=fminf(mn.x,p.x); mn.y=fminf(mn.y,p.y); mn.z=fminf(mn.z,p.z);
        mx.x=fmaxf(mx.x,p.x); mx.y=fmaxf(mx.y,p.y); mx.z=fmaxf(mx.z,p.z);
    }
    float cx=(mn.x+mx.x)*0.5f, cz=(mn.z+mx.z)*0.5f;
    float ext = fmaxf(mx.x-mn.x, fmaxf(mx.y-mn.y, mx.z-mn.z));
    float esc = tam / (ext>1e-6f ? ext : 1.0f);
    for (Vec3& p : vs) {
        p.x = (p.x-cx)*esc;
        p.y = (p.y-mn.y)*esc + piso;   // min y -> piso
        p.z = (p.z-cz)*esc;
    }

    GLuint lista = glGenLists(1);
    glNewList(lista, GL_COMPILE);
      glBegin(GL_TRIANGLES);
      for (Tri& t : tris) {
          Vec3 fn = vnorm(vcross(vsub(vs[t.v[1]],vs[t.v[0]]), vsub(vs[t.v[2]],vs[t.v[0]])));
          glColor3f(t.col.x, t.col.y, t.col.z);                    // cor do material
          for (int k=0;k<3;k++) {
              if (t.n[k]>=0 && t.n[k]<(int)ns.size()) glNormal3f(ns[t.n[k]].x,ns[t.n[k]].y,ns[t.n[k]].z);
              else                                    glNormal3f(fn.x,fn.y,fn.z);
              glVertex3f(vs[t.v[k]].x, vs[t.v[k]].y, vs[t.v[k]].z);
          }
      }
      glEnd();
    glEndList();
    return lista;
}

// Fallback do sapo: so e usado se sapo.obj faltar (corpo + cabeca em elipsoides).
void geraMalhaSapo() {
    listaSapo = glGenLists(1);
    glNewList(listaSapo, GL_COMPILE);
      glColor3f(VERDE_SAPO[0], VERDE_SAPO[1], VERDE_SAPO[2]);
      blob(0, 0.02f, -0.03f, 0.60f, 0.40f, 0.66f, 1.0f);   // tronco
      blob(0, 0.07f,  0.56f, 0.44f, 0.39f, 0.42f, 1.0f);   // cabeca
    glEndList();
}

// Transformacao do sapo no mundo (posicao + giro + squash/rolagem).
void transformaSapo() {
    float fase = tempo * 10.0f;
    float bob  = fabsf(sinf(fase)) * 0.10f;
    float sqz  = 1.0f + sinf(fase) * 0.04f;
    glTranslatef(sapoX, sapoY + bob + 0.5f, 0.0f);  // +0.5 levanta o modelo
    glRotatef(180.0f, 0, 1, 0);                     // cabeca +Z -> corre p/ -Z
    if (rolando) {
        glTranslatef(0, -0.15f, 0);
        glScalef(1.1f, 0.45f, 1.1f);                // ESCALA (baixinho)
        glRotatef(20.0f, 1, 0, 0);                  // ROTACAO (inclina)
    } else {
        glScalef(1.0f/sqz, sqz, 1.0f/sqz);          // ESCALA anima (squash&stretch)
    }
}

void desenhaSapo() {
    glPushMatrix();
      transformaSapo();

      // ---- CORPO: a malha do .obj (normais/cores ja na display list) ----
      brilhoMaterial(0.30f, 20);
      glDisable(GL_CULL_FACE);
      if (listaSapo) glCallList(listaSapo);
      glEnable(GL_CULL_FACE);

      // ---- OLHOS: so no fallback (o .obj ja traz os olhos) ----
      if (!sapoDeOBJ) {
        brilhoMaterial(0.9f, 80);              // olho bem molhado/brilhante
        for (int lado = -1; lado <= 1; lado += 2) {
          glColor3f(0.93f, 0.76f, 0.18f);      // iris dourada (globo)
          blob(0.28f*lado, 0.33f, 0.55f, 1, 1, 1, 0.155f);
          glColor3f(0.04f, 0.04f, 0.04f);      // pupila HORIZONTAL, virada p/ cima-tras
          blob(0.28f*lado, 0.40f, 0.49f, 1.5f, 0.6f, 1.0f, 0.085f);
          glColor3f(1, 1, 1);                  // brilhinho (highlight)
          blob(0.31f*lado, 0.44f, 0.46f, 1, 1, 1, 0.032f);
        }
      }
      brilhoMaterial(0.30f, 20);

      // ---- LINGUA: so aparece (esticando) quando come uma mosca ----
      // A escala em Z cresce de 0 ate ~2 -> demonstra bem a transformacao ESCALA.
      if (lingua > 0.01f) {
        glColor3f(0.9f, 0.3f, 0.4f);
        glPushMatrix();
          glTranslatef(0, -0.05f, 0.95f);
          glScalef(0.12f, 0.12f, 2.0f * lingua);  // ESCALA animada
          glTranslatef(0, 0, 0.5f);
          glutSolidCube(1.0);
        glPopMatrix();
      }
    glPopMatrix();
    brilhoMaterial(1.0f, 40);   // restaura brilho padrao p/ os demais objetos
}

// ============================================================================
//  5) MOSCA  [REQ:MODELAGEM] corpo escuro + 2 asas que batem [REQ:TRANSFORM]
// ============================================================================
void desenhaMosca(float x, float y, float z) {
    float bater = sinf(tempo * 42.0f) * 38.0f;   // asas batendo
    glPushMatrix();
      glTranslatef(x, y, z);
      glTranslatef(0, sinf(tempo * 6.0f) * 0.04f, 0);   // flutua de leve

      // ---- MODELO .OBJ (moscas.obj) -> gira devagar ----
      if (moscaDeOBJ) {
        glRotatef(tempo * 60.0f, 0, 1, 0);       // giro lento p/ dar vida
        glDisable(GL_CULL_FACE);
        glCallList(listaMosca);
        glEnable(GL_CULL_FACE);
        glPopMatrix();
        return;
      }

      // ---- CORPO: uma "gotinha" escura (minimalista) ----
      glColor3f(0.12f, 0.12f, 0.15f);
      blobLo(0, 0, 0, 1.0f, 1.0f, 1.35f, 0.12f);
      // pontinhos de olho (bem discretos)
      glColor3f(0.55f, 0.10f, 0.08f);
      for (int s=-1;s<=1;s+=2) blobLo(0.055f*s, 0.03f, 0.11f, 1,1,1, 0.035f);

      // ---- 2 ASAS translucidas simples ----
      glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
      glColor4f(0.85f, 0.90f, 1.0f, 0.5f);
      for (int s=-1;s<=1;s+=2) {
        glPushMatrix();
          glTranslatef(0, 0.06f, -0.02f);
          glRotatef(bater*s, 0, 0, 1);
          blobLo(0.14f*s, 0.02f, 0, 1.25f, 0.09f, 0.7f, 0.13f);
        glPopMatrix();
      }
      glDepthMask(GL_TRUE); glDisable(GL_BLEND);
    glPopMatrix();
}

// ============================================================================
//  6) OBSTACULOS
// ============================================================================
void desenhaObstaculo(const Obstaculo& o) {
    glPushMatrix();
      glTranslatef(POS_PISTA[o.pista], 0.0f, o.z);
      if (o.tipo == OBST_BAIXO) {
        // ---- VAO NA PONTE (pule por cima, ou desvie de pista) ----
        //   O deck ja foi RECORTADO neste trecho (ver desenhaCenario), entao a
        //   agua do rio aparece por baixo de verdade. Aqui so caprichamos nas
        //   TABUAS QUEBRADAS/lascadas nas beiras do buraco.
        glColor3f(0.42f, 0.29f, 0.16f);
        for (int s = -1; s <= 1; s += 2) {
          glPushMatrix(); glTranslatef(0.75f*s, 0,  1.0f); glRotatef(-24.0f,1,0,0); caixa(0.55f,0.10f,0.30f,false); glPopMatrix();
          glPushMatrix(); glTranslatef(0.75f*s, 0, -1.0f); glRotatef( 24.0f,1,0,0); caixa(0.55f,0.10f,0.30f,false); glPopMatrix();
        }
      } else if (o.tipo == OBST_BLOCO) {
        // ---- PEDRA (desvie de pista) - minimalista: 2 seixos lisos, sem musgo ----
        glColor3f(0.56f, 0.56f, 0.60f);
        blob( 0.00f, 0.58f, 0.00f, 1.5f, 1.15f, 1.3f, 0.56f);    // seixo grande
        glColor3f(0.47f, 0.47f, 0.51f);
        blob( 0.22f, 1.12f, 0.08f, 0.9f, 0.72f, 0.85f, 0.44f);   // seixo menor por cima
      } else { // OBST_ALTO -> BARREIRA de madeira (role por baixo)
        // ---- minimalista: 2 postes limpos + 1 barra reta (sem toras/musgo) ----
        glColor3f(0.44f, 0.31f, 0.20f);
        glPushMatrix(); glTranslatef(-1.05f, 0, 0); caixa(0.16f, 1.12f, 0.16f, false); glPopMatrix();
        glPushMatrix(); glTranslatef( 1.05f, 0, 0); caixa(0.16f, 1.12f, 0.16f, false); glPopMatrix();
        glColor3f(0.52f, 0.37f, 0.24f);
        glPushMatrix(); glTranslatef(0, 1.12f, 0); caixa(2.5f, 0.24f, 0.24f, false); glPopMatrix();
      }
    glPopMatrix();
}

// ============================================================================
//  7) O PIER e a AGUA (o cenario que "rola" em direcao ao jogador)
//     A ilusao de movimento vem de DESLOCAR as coordenadas de textura com a
//     distancia percorrida -> [REQ:TRANSFORM/translacao] + [REQ:TEXTURA]
// ============================================================================
// Desenha um plano (chao) subdividido em nx x nz quadradinhos.
// POR QUE SUBDIVIDIR? A iluminacao do OpenGL fixo eh calculada POR VERTICE e
// interpolada entre eles. Um plano com so 4 cantos fica "chapado". Com uma
// grade de muitos vertices, a luz do "sol" (que eh posicional) cria um
// degrade suave e ate um brilho especular na superficie.  [REQ:ILUMINACAO]
void planoTex(float x0, float x1, float zNear, float zFar, float y,
              float repX, float repZ, float rolagem, int nx, int nz) {
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);                       // normal apontando pra cima
    for (int i = 0; i < nz; i++) {
        float t0 = i / (float)nz, t1 = (i + 1) / (float)nz;
        float za = zFar + (zNear - zFar) * t0, zb = zFar + (zNear - zFar) * t1;
        float v0 = rolagem + repZ * t0, v1 = rolagem + repZ * t1;
        for (int j = 0; j < nx; j++) {
            float s0 = j / (float)nx, s1 = (j + 1) / (float)nx;
            float xa = x0 + (x1 - x0) * s0, xb = x0 + (x1 - x0) * s1;
            float u0 = repX * s0, u1 = repX * s1;
            glTexCoord2f(u0, v0); glVertex3f(xa, y, za);
            glTexCoord2f(u1, v0); glVertex3f(xb, y, za);
            glTexCoord2f(u1, v1); glVertex3f(xb, y, zb);
            glTexCoord2f(u0, v1); glVertex3f(xa, y, zb);
        }
    }
    glEnd();
}

// Face lateral VERTICAL de uma tabua do pier (do topo y=0 ate yBot, dentro da
// agua). Subdividida em Z p/ a luz/nevoa interpolarem e a textura ROLAR junto
// com o mundo -> o pier ganha ESPESSURA visivel (antes parecia um decalque).
void paredePier(float x, float zNear, float zFar, float yTop, float yBot,
                float nx_, float rolagem, float repZ, int nz) {
    glNormal3f(nx_, 0, 0);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= nz; i++) {
        float t = i / (float)nz;
        float z = zFar + (zNear - zFar) * t;
        float v = rolagem + repZ * t;
        glTexCoord2f(0.0f, v); glVertex3f(x, yTop, z);
        glTexCoord2f(0.6f, v); glVertex3f(x, yBot, z);
    }
    glEnd();
}

// Uma ESTACA (pilar) de madeira que afunda na agua, sob a beira do pier.
void estacaPier(float x, float z) {
    glPushMatrix();
      glTranslatef(x, -1.7f, z);
      caixa(0.22f, 1.7f, 0.22f, false);   // vai de y=-1.7 (na agua) ate y=0 (deck)
    glPopMatrix();
}

// pseudo-random ESTAVEL por indice (0..1) -> variar arvores sem "piscar" ao rolar
static float hashIdx(int i, int sal) {
    int n = i * 73856093 ^ sal * 19349663;
    n = (n << 13) ^ n;
    return ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f * 0.5f;
}

// ARVORE cartoon: tronco marrom + copa em 2-3 bolhas verdes (reusa listaEsfera).
void desenhaArvore(float x, float z, float esc, float tom) {
    glPushMatrix();
      glTranslatef(x, -0.08f, z);
      glScalef(esc, esc, esc);
      // tronco ALTO e fino (arvore de dossel amazonico)
      glColor3f(0.38f, 0.27f, 0.18f);
      glPushMatrix(); glTranslatef(0, 0.0f, 0); caixa(0.22f, 2.3f, 0.22f, false); glPopMatrix();
      // copa: 2 bolhas verdes bem no ALTO (dossel), forma limpa/minimalista
      glColor3f(0.19f*tom, 0.45f*tom, 0.23f*tom);
      blob(0.0f, 2.8f, 0.0f, 1.55f, 1.35f, 1.55f, 0.72f);
      glColor3f(0.24f*tom, 0.53f*tom, 0.27f*tom);
      blob(0.18f, 3.25f, -0.12f, 1.0f, 1.0f, 1.0f, 0.55f);
    glPopMatrix();
}

// MOITA / arbusto: um aglomerado baixo de bolhas verdes.
void desenhaMoita(float x, float z, float esc) {
    glPushMatrix();
      glTranslatef(x, -0.12f, z);
      glScalef(esc, esc, esc);
      glColor3f(0.26f, 0.50f, 0.24f);
      blob( 0.0f, 0.18f, 0.0f, 1.3f, 0.9f, 1.2f, 0.34f);
      glColor3f(0.32f, 0.58f, 0.28f);
      blob(-0.24f, 0.12f, 0.12f, 0.9f, 0.8f, 0.9f, 0.26f);
      blob( 0.22f, 0.14f,-0.10f, 0.95f, 0.85f, 0.9f, 0.28f);
    glPopMatrix();
}

// JUNCOS: talos finos verticais na beira da agua (escondem a costura da margem).
bool usoShader() { return shadersOK && usarShader; }

void desenhaCenario() {
    // Rolagem da textura TRAVADA na velocidade real do mundo (mesmo sentido das
    // arvores/obstaculos). Modulo = 40/125: a textura repete 40x ao longo de 125
    // unidades ((Z_MORTE+10)-(Z_SPAWN-20)), entao esse passo cola cada "texel" no
    // ponto do mundo (1:1). SINAL NEGATIVO: com 'rolagem' crescente a textura
    // fluiria pro horizonte (contra as arvores); negativa faz o solo vir NA
    // direcao da camera, junto com as arvores.
    float rolagem = -distancia * (40.0f / 125.0f);
    glDisable(GL_CULL_FACE);             // planos de uma face so
    glColor3f(1, 1, 1);

    // ---- AGUA ----
    if (usoShader()) {
        glUseProgram(progAgua);                          // shader de agua (ondas + brilho)
        glUniform1f(glGetUniformLocation(progAgua,"tempo"), tempo);
        enviaFog(progAgua);
        glBindTexture(GL_TEXTURE_2D, texBranca);
        planoTex(-40, 40, Z_MORTE + 10, Z_SPAWN - 20, -0.2f, 16, 40, rolagem, 44, 90); // + subdiv p/ ondas
    } else {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texAgua);
        planoTex(-40, 40, Z_MORTE + 10, Z_SPAWN - 20, -0.2f, 16, 40, rolagem, 16, 50);
    }

    // ---- MARGENS (grama): transformam a lagoa num RIO com duas margens ----
    if (usoShader()) { glUseProgram(progToon); enviaFog(progToon); }
    else               glEnable(GL_TEXTURE_2D);
    glColor3f(1, 1, 1);
    glBindTexture(GL_TEXTURE_2D, texGrama);
    planoTex(-40.0f, -7.0f, Z_MORTE + 10, Z_SPAWN - 20, -0.10f, 10, 40, rolagem, 12, 60);
    planoTex(  7.0f, 40.0f, Z_MORTE + 10, Z_SPAWN - 20, -0.10f, 10, 40, rolagem, 12, 60);

    // ---- PONTE: um DECK UNICO e continuo (as 3 pistas viram UMA ponte so, sem
    //   vao entre elas). Os buracos sao RECORTADOS de verdade: nao desenhamos o
    //   deck no trecho do vao -> a agua do rio aparece por baixo. ----
    const float zN = Z_MORTE + 10.0f, zF = Z_SPAWN - 20.0f;   // extensao do deck em Z
    const float XB0 = -3.6f, XB1 = 3.6f;                       // bordas da ponte
    const float BX[4] = { XB0, -1.2f, 1.2f, XB1 };             // 3 faixas ADJACENTES
    const float repZtot = 40.0f, repXtot = 6.0f;               // textura continua
    const float HL = 1.0f;                                     // meia-largura do vao (Z)
    auto vOfZ = [&](float z){ return rolagem + repZtot*(z - zF)/(zN - zF); };
    auto uOfX = [&](float x){ return repXtot*(x - XB0)/(XB1 - XB0); };

    // pedaco SOLIDO do topo do deck [x0,x1]x[za,zb], subdividido p/ luz/nevoa
    auto deckTop = [&](float x0, float x1, float za, float zb){
        int nz = (int)ceilf(fabsf(zb - za) / 1.6f); if (nz < 1) nz = 1;
        glNormal3f(0, 1, 0);
        for (int i = 0; i < nz; i++) {
            float z0 = za + (zb - za) * (i / (float)nz);
            float z1 = za + (zb - za) * ((i+1) / (float)nz);
            float v0 = vOfZ(z0), v1 = vOfZ(z1);
            glBegin(GL_QUADS);
              glTexCoord2f(uOfX(x0), v0); glVertex3f(x0, 0.0f, z0);
              glTexCoord2f(uOfX(x1), v0); glVertex3f(x1, 0.0f, z0);
              glTexCoord2f(uOfX(x1), v1); glVertex3f(x1, 0.0f, z1);
              glTexCoord2f(uOfX(x0), v1); glVertex3f(x0, 0.0f, z1);
            glEnd();
        }
    };
    // parede de CORTE do vao: mostra a espessura da madeira na borda do buraco
    auto cutWall = [&](float x0, float x1, float z){
        glColor3f(0.30f, 0.21f, 0.12f);
        glNormal3f(0, 0, 1);
        glBegin(GL_QUADS);
          glTexCoord2f(uOfX(x0),0.0f); glVertex3f(x0,  0.0f, z);
          glTexCoord2f(uOfX(x1),0.0f); glVertex3f(x1,  0.0f, z);
          glTexCoord2f(uOfX(x1),0.5f); glVertex3f(x1, -0.20f, z);
          glTexCoord2f(uOfX(x0),0.5f); glVertex3f(x0, -0.20f, z);
        glEnd();
        glColor3f(1,1,1);
    };

    glBindTexture(GL_TEXTURE_2D, texMadeira);
    for (int bnd = 0; bnd < 3; bnd++) {
        float x0 = BX[bnd], x1 = BX[bnd+1];
        // vaos (buracos) desta faixa, ordenados de longe -> perto
        std::vector<std::pair<float,float> > vaos;
        for (auto& o : obstaculos)
            if (o.tipo == OBST_BAIXO && o.pista == bnd)
                vaos.push_back(std::make_pair(o.z - HL, o.z + HL));
        std::sort(vaos.begin(), vaos.end());
        // varre de zF (longe) ate zN (perto), desenhando o solido e PULANDO o vao
        float cursor = zF;
        for (auto& g : vaos) {
            float g0 = fmaxf(g.first, zF), g1 = fminf(g.second, zN);
            if (g1 <= cursor) continue;                     // fora do range / ja coberto
            if (g0 > cursor) deckTop(x0, x1, cursor, g0);   // trecho solido antes do vao
            cutWall(x0, x1, fmaxf(g0, cursor));             // borda de corte (longe)
            cutWall(x0, x1, g1);                            // borda de corte (perto)
            cursor = g1;
        }
        if (cursor < zN) deckTop(x0, x1, cursor, zN);
    }
    // ESPESSURA nas bordas EXTERNAS da ponte (desce ate dentro da agua)
    glColor3f(0.66f, 0.62f, 0.58f);
    paredePier(XB0, zN, zF, 0.0f, -0.42f, -1.0f, rolagem, 40, 70);
    paredePier(XB1, zN, zF, 0.0f, -0.42f,  1.0f, rolagem, 40, 70);
    glColor3f(1, 1, 1);

    // ---- ESTACAS sob as beiras externas (rolam junto com o mundo) ----
    glBindTexture(GL_TEXTURE_2D, texBranca);
    glColor3f(0.34f, 0.24f, 0.15f);
    float espac = 7.0f, fase = fmodf(distancia, espac);
    for (float z = Z_SPAWN + fase; z < Z_MORTE; z += espac) {
        estacaPier(XB0, z);
        estacaPier(XB1, z);
    }
    glColor3f(1, 1, 1);

    // ---- VEGETACAO nas margens: arvores, moitas e juncos (variados e rolando) ----
    // Selva DENSA: espacamento curto + 2 camadas de arvore por lado (perto/fundo)
    // -> as margens "fecham" como floresta amazonica, mas as copas sao altas e o
    // meio (a ponte) continua livre e legivel.
    float espV = 3.6f, faseV = fmodf(distancia, espV);
    for (float z = Z_SPAWN + faseV; z < Z_MORTE; z += espV) {
        int idx = (int)floorf((z - distancia) / espV + 0.5f);  // rotulo ESTAVEL do item
        for (int lado = -1; lado <= 1; lado += 2) {
            // arvore da FRENTE (encosta na margem, emoldura o rio)
            float xF = lado * (6.8f + hashIdx(idx, lado*3) * 3.0f);
            desenhaArvore(xF, z, 1.0f + hashIdx(idx, lado*5) * 0.7f,
                                  0.85f + hashIdx(idx, lado*9) * 0.4f);
            // arvore do FUNDO (mais longe e alta -> fecha o dossel ao fundo)
            float xB = lado * (13.0f + hashIdx(idx, lado*7) * 9.0f);
            desenhaArvore(xB, z + espV*0.5f, 1.3f + hashIdx(idx, lado*13) * 0.9f,
                                  0.78f + hashIdx(idx, lado*17) * 0.35f);
            // uma moita ocasional na beira da agua (esconde a costura da margem)
            if (hashIdx(idx, lado*2) < 0.5f)
                desenhaMoita(lado * (7.0f + hashIdx(idx, lado) * 1.2f), z + 1.0f, 1.0f);
        }
    }
    // deixa a textura BRANCA ligada -> objetos coloridos (sapo/obstaculos) ok no shader
    glBindTexture(GL_TEXTURE_2D, texBranca);
    if (!usoShader()) glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE);
}

// ============================================================================
//  8) SOMBRA PROJETADA  [REQ:SOMBRA]
//     Tecnica classica do OpenGL fixo: uma MATRIZ que "achata" o objeto sobre
//     o plano do chao (y=0), na direcao da luz. Desenhamos o sapo de novo,
//     escuro e achatado -> vira a sombra dele no pier.
// ============================================================================
void matrizSombra(GLfloat m[16], const GLfloat plano[4], const GLfloat luz[4]) {
    float d = plano[0]*luz[0] + plano[1]*luz[1] + plano[2]*luz[2] + plano[3]*luz[3];
    m[0]=d-luz[0]*plano[0]; m[4]= -luz[0]*plano[1]; m[8] = -luz[0]*plano[2]; m[12]= -luz[0]*plano[3];
    m[1]= -luz[1]*plano[0]; m[5]=d-luz[1]*plano[1]; m[9] = -luz[1]*plano[2]; m[13]= -luz[1]*plano[3];
    m[2]= -luz[2]*plano[0]; m[6]= -luz[2]*plano[1]; m[10]=d-luz[2]*plano[2]; m[14]= -luz[2]*plano[3];
    m[3]= -luz[3]*plano[0]; m[7]= -luz[3]*plano[1]; m[11]= -luz[3]*plano[2]; m[15]=d-luz[3]*plano[3];
}

GLfloat posLuz[4] = { -6.0f, 14.0f, 6.0f, 1.0f };  // posicao do "sol"

void desenhaSombraDoSapo() {
    GLfloat plano[4] = { 0, 1, 0, -0.02f };        // plano do chao y=0.02
    GLfloat m[16];
    matrizSombra(m, plano, posLuz);

    glDisable(GL_LIGHTING);                         // sombra nao recebe luz
    glEnable(GL_BLEND);                             // transparencia da sombra
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.35f);

    glPushMatrix();
      glMultMatrixf(m);                             // aplica o "achatamento"
      // reusa a geometria do sapo, mas sem cor/olhos (so a silhueta achatada)
      glTranslatef(sapoX, sapoY + 0.52f, 0.0f);   // +0.5 = mesma altura do sapo levantado
      glPushMatrix(); glScalef(1.32f, 0.80f, 1.18f); glutSolidSphere(0.52, 12, 10); glPopMatrix();
      glPushMatrix(); glTranslatef(0, 0, 0.58f); glScalef(1.0f, 0.8f, 0.86f); glutSolidSphere(0.40, 12, 10); glPopMatrix();
    glPopMatrix();

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// ============================================================================
//  9) TEXTO NA TELA (placar) - projecao ortografica 2D por cima da cena
// ============================================================================
void texto(float x, float y, const char* s, void* fonte) {
    glRasterPos2f(x, y);
    for (const char* c = s; *c; c++) glutBitmapCharacter(fonte, *c);
}
// largura em pixels de um texto bitmap (p/ centralizar)
int larguraTexto(const char* s, void* fonte) {
    return glutBitmapLength(fonte, (const unsigned char*)s);
}
// texto centralizado em torno de cx
void textoCentro(float cx, float y, const char* s, void* fonte) {
    texto(cx - larguraTexto(s, fonte) * 0.5f, y, s, fonte);
}
// entra/sai do modo 2D (projecao ortografica na tela) guardando o estado 3D
void inicio2D() {
    glDisable(GL_LIGHTING); glDisable(GL_DEPTH_TEST); glDisable(GL_FOG);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    gluOrtho2D(0, larguraTela, 0, alturaTela);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
}
void fim2D() {
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_FOG);
}
void retangulo(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
      glVertex2f(x, y); glVertex2f(x+w, y); glVertex2f(x+w, y+h); glVertex2f(x, y+h);
    glEnd();
}

// ---- FONTE VETORIAL (stroke) estilo GAMER: escalavel e com traco grosso ----
// As fontes bitmap do GLUT sao minusculas e fixas. As STROKE sao vetoriais:
// da pra escalar pra qualquer tamanho e engrossar o traco (glLineWidth).
// Usamos GLUT_STROKE_MONO_ROMAN (monoespacada -> cara de arcade / placar).
static const float STROKE_H = 119.05f;   // altura nominal de uma stroke font
void textoStroke(float cx, float y, float altura, float grossura,
                 const char* s, void* fonte) {
    float sc = altura / STROKE_H;
    float w  = glutStrokeLength(fonte, (const unsigned char*)s) * sc;
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(grossura);
    glPushMatrix();
      glTranslatef(cx - w * 0.5f, y, 0);   // centralizado horizontalmente
      glScalef(sc, sc, sc);
      for (const char* c = s; *c; c++) glutStrokeCharacter(fonte, *c);
    glPopMatrix();
    glLineWidth(1.0f);
    glDisable(GL_LINE_SMOOTH);
}

// HUD DE JOGO: score centralizado no topo + BARRA DE FOME no rodape.
void desenhaHUD() {
    inicio2D();
    float cx = larguraTela * 0.5f;
    char buf[128];

    // ---- SCORE centralizado (moscas + distancia) ----
    glColor3f(1, 1, 1);
    sprintf(buf, "%d m", (int)distancia);
    textoCentro(cx, alturaTela - 46, buf, GLUT_BITMAP_TIMES_ROMAN_24);
    sprintf(buf, "%d moscas", pontos);
    textoCentro(cx, alturaTela - 74, buf, GLUT_BITMAP_HELVETICA_18);

    // ---- BARRA DE FOME (rodape, centralizada): verde->amarelo->vermelho ----
    float bw = 420, bh = 24, bx = cx - bw*0.5f, by = 30;
    float ff = fome < 0 ? 0 : (fome > 1 ? 1 : fome);
    glColor3f(0.10f, 0.09f, 0.07f); retangulo(bx-3, by-3, bw+6, bh+6);   // moldura
    glColor3f(0.26f, 0.22f, 0.18f); retangulo(bx, by, bw, bh);          // fundo
    float cr = (ff < 0.5f) ? 1.0f : 2.0f*(1.0f-ff);                     // gradiente de cor
    float cg = (ff > 0.5f) ? 1.0f : 2.0f*ff;
    glColor3f(cr, cg, 0.16f);        retangulo(bx, by, bw*ff, bh);      // preenchimento
    glColor3f(1, 1, 1);
    textoCentro(cx, by + bh + 8, "FOME  (coma moscas!)", GLUT_BITMAP_HELVETICA_12);

    fim2D();
}

// TELA DE MENU (overlay 2D): titulo, entrada de nome e melhores scores.
void desenhaMenu() {
    inicio2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float cx = larguraTela * 0.5f;
    char buf[160];
    void* MONO = GLUT_STROKE_MONO_ROMAN;

    // ---- TITULO com sombra (destaque "gamer") ----
    glColor4f(0.20f, 0.06f, 0.0f, 0.6f);
    textoStroke(cx + 4, alturaTela - 104, 64, 5.5f, "SAPO SURFISTA", MONO);
    glColor3f(1.0f, 0.86f, 0.42f);
    textoStroke(cx,     alturaTela - 100, 64, 4.5f, "SAPO SURFISTA", MONO);

    // ---- CAIXA DO NOME ----
    glColor4f(0, 0, 0, 0.4f); retangulo(cx - 265, alturaTela - 190, 530, 48);
    glColor3f(1, 1, 1);
    sprintf(buf, "NOME: %s_", nomeJogador.c_str());
    textoStroke(cx, alturaTela - 180, 30, 3.0f, buf, MONO);
    glColor3f(0.95f, 0.9f, 0.78f);
    textoStroke(cx, alturaTela - 220, 17, 1.6f, "DIGITE O NOME E TECLE ENTER", MONO);

    // ---- PLACAR (painel translucido p/ legibilidade sobre o sapo) ----
    float py1 = 28, py2 = 250;
    glColor4f(0, 0, 0, 0.45f); retangulo(cx - 335, py1, 670, py2 - py1);
    glColor3f(1.0f, 0.85f, 0.40f);
    textoStroke(cx, py2 - 42, 30, 3.5f, "- MELHORES -", MONO);
    glColor3f(1, 1, 1);
    if (placar.empty()) {
        textoStroke(cx, py2 - 105, 22, 2.0f, "AINDA SEM RECORDES", MONO);
    } else {
        float y = py2 - 92;
        for (size_t i = 0; i < placar.size() && i < 5; i++) {
            sprintf(buf, "%d. %-12s %5d m", (int)i+1, placar[i].nome.c_str(), placar[i].dist);
            textoStroke(cx, y, 22, 2.2f, buf, MONO);
            y -= 32;
        }
    }
    glDisable(GL_BLEND);
    fim2D();
}

// TELA DE GAME OVER (overlay 2D).
void desenhaGameOver() {
    inicio2D();
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.55f); retangulo(0, 0, larguraTela, alturaTela);
    float cx = larguraTela * 0.5f, cy = alturaTela * 0.5f;
    char buf[128];
    void* MONO = GLUT_STROKE_MONO_ROMAN;

    glColor4f(0.25f, 0.0f, 0.0f, 0.7f); textoStroke(cx + 4, cy + 56, 72, 7.0f, "GAME OVER", MONO);
    glColor3f(1.0f, 0.40f, 0.34f);      textoStroke(cx,     cy + 60, 72, 5.5f, "GAME OVER", MONO);

    glColor3f(1, 1, 1);
    sprintf(buf, "%d M", (int)distancia);
    textoStroke(cx, cy - 8, 48, 4.0f, buf, MONO);
    sprintf(buf, "%s  -  %d MOSCAS", nomeJogador.empty()?"SAPO":nomeJogador.c_str(), pontos);
    textoStroke(cx, cy - 52, 22, 2.0f, buf, MONO);
    glColor3f(0.95f, 0.88f, 0.7f);
    textoStroke(cx, cy - 100, 24, 2.2f, "ENTER PARA O MENU", MONO);
    glDisable(GL_BLEND);
    fim2D();
}

// ============================================================================
//  9b) CEU EM GRADIENTE - um quad 2D ao fundo (azul no topo -> nevoa embaixo).
//      Desenhado antes de tudo, sem profundidade: a cena 3D pinta por cima e a
//      agua distante (que a nevoa desbota) funde suavemente no horizonte.
// ============================================================================
void desenhaCeu() {
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING);
    glDisable(GL_FOG);        glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0,1,0,1);
    glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();
    // ---- GRADIENTE DE POR DO SOL (horizonte y=0 -> topo y=1) ----
    // Varias faixas -> transicao suave pessego -> rosa -> laranja -> ember.
    const int NS = 5;
    static const float SKY[NS][4] = {
        {0.00f, 0.99f, 0.78f, 0.60f},   // horizonte: pessego (= COR_NEVOA, funde na agua)
        {0.24f, 1.00f, 0.70f, 0.50f},   // apricot
        {0.50f, 0.98f, 0.54f, 0.50f},   // rosa
        {0.76f, 0.96f, 0.48f, 0.34f},   // laranja
        {1.00f, 0.82f, 0.40f, 0.30f},   // ember (topo)
    };
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i < NS; i++) {
        glColor3f(SKY[i][1], SKY[i][2], SKY[i][3]);
        glVertex2f(0.0f, SKY[i][0]); glVertex2f(1.0f, SKY[i][0]);
    }
    glEnd();

    // ---- SOL: disco dourado + halo, logo acima do horizonte ----
    // asp mantem o sol REDONDO (o ortho 0..1 distorce com a razao da janela).
    const float solX = 0.5f, solY = 0.40f;
    float asp = (float)alturaTela / (larguraTela ? larguraTela : 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);              // additivo -> brilho que soma
    for (int r = 0; r < 3; r++) {                   // halo em 3 aneis suaves
        float rad = 0.11f + r * 0.07f;
        float a   = 0.17f - r * 0.05f;
        glBegin(GL_TRIANGLE_FAN);
          glColor4f(1.0f, 0.80f, 0.45f, a);   glVertex2f(solX, solY);
          glColor4f(1.0f, 0.80f, 0.45f, 0.0f);
          for (int k = 0; k <= 40; k++) { float a2 = k / 40.0f * 2.0f * PI;
              glVertex2f(solX + cosf(a2)*rad*asp, solY + sinf(a2)*rad); }
        glEnd();
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_TRIANGLE_FAN);                        // core do sol (disco solido)
      glColor4f(1.0f, 0.95f, 0.78f, 1.0f); glVertex2f(solX, solY);
      glColor4f(1.0f, 0.86f, 0.58f, 1.0f);
      for (int k = 0; k <= 48; k++) { float a2 = k / 48.0f * 2.0f * PI;
          glVertex2f(solX + cosf(a2)*0.075f*asp, solY + sinf(a2)*0.075f); }
    glEnd();
    glDisable(GL_BLEND);
    glColor3f(1,1,1);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glEnable(GL_FOG);        glEnable(GL_CULL_FACE);
}

// ============================================================================
//  10) DISPLAY - monta a cena inteira a cada quadro
// ============================================================================
// troca de programa de shader com seguranca (nao faz nada se GLEW falhou)
void prog(GLuint p) { if (shadersOK) glUseProgram(p); }

// ---- PLACAR: carrega/salva em placar.txt (nome moscas distancia por linha) ----
bool scoreMaior(const Score& a, const Score& b) { return a.pts > b.pts; }
// Mantem apenas O MELHOR resultado de cada NOME (sem repetidos), ordena e corta.
void dedupPlacar() {
    std::map<std::string, Score> melhor;
    for (Score& s : placar) {
        auto it = melhor.find(s.nome);
        if (it == melhor.end() || s.pts > it->second.pts) melhor[s.nome] = s;
    }
    placar.clear();
    for (auto& kv : melhor) placar.push_back(kv.second);
    std::sort(placar.begin(), placar.end(), scoreMaior);
    if (placar.size() > 10) placar.resize(10);
}
void salvaPlacar() {
    FILE* f = fopen("placar.txt", "w"); if (!f) return;
    for (Score& s : placar) fprintf(f, "%s %d %d\n", s.nome.c_str(), s.moscas, s.dist);
    fclose(f);
}
void carregaPlacar() {
    placar.clear();
    FILE* f = fopen("placar.txt", "r"); if (!f) return;
    char nome[64]; int mo, di;
    while (fscanf(f, "%63s %d %d", nome, &mo, &di) == 3) {
        Score s; s.nome = nome; s.moscas = mo; s.dist = di; s.pts = di;  // score = distancia
        placar.push_back(s);
    }
    fclose(f);
    dedupPlacar();          // limpa eventuais repetidos ja gravados
    salvaPlacar();          // reescreve o arquivo ja limpo
}
void registraScore() {
    Score s;
    s.nome   = nomeJogador.empty() ? "SAPO" : nomeJogador;
    s.moscas = pontos;
    s.dist   = (int)distancia;
    s.pts    = (int)distancia;          // SCORE = so a distancia (moscas so reabastecem)
    placar.push_back(s);
    dedupPlacar();          // guarda so o melhor de cada nome
    salvaPlacar();
}

// ---- transicoes de estado ----
void iniciaJogo() {                       // menu/gameover -> jogando (reseta tudo)
    obstaculos.clear(); moscas.clear();
    pista = 1; sapoX = 0; sapoY = 0; velY = 0;
    pulando = rolando = false; lingua = 0;
    velocidade = 14.0f; distancia = 0; pontos = 0; fome = 1.0f;
    for (float z = -15; z >= Z_SPAWN; z -= ESPACO_LINHA) geraLinha(z);
    distProxLinha = ESPACO_LINHA;
    estado = JOGANDO;
}
void morrer() { registraScore(); estado = GAMEOVER; }
void voltaMenu() {                        // gameover -> menu (cena limpa e estatica)
    estado = MENU;
    obstaculos.clear(); moscas.clear();
    sapoX = 0; distancia = 0;
}

// SAPO DO MENU: de FRENTE para a camera, em cima da ponte, com respiracao leve.
void desenhaSapoMenu() {
    glPushMatrix();
      float bob = fabsf(sinf(tempo * 3.0f)) * 0.06f;
      glTranslatef(0.0f, 0.5f + bob, -0.5f);     // em cima da ponte, um pouco a frente
      glScalef(1.2f, 1.2f, 1.2f);
      // SEM o giro de 180 -> a cabeca (+Z do modelo) fica virada para a camera
      brilhoMaterial(0.30f, 20);
      glDisable(GL_CULL_FACE);
      if (listaSapo) glCallList(listaSapo);
      glEnable(GL_CULL_FACE);
      if (!sapoDeOBJ) {                          // olhos p/ malha gerada (o .obj ja tem)
        brilhoMaterial(0.9f, 80);
        for (int lado = -1; lado <= 1; lado += 2) {
          glColor3f(0.93f, 0.76f, 0.18f); blob(0.28f*lado, 0.33f, 0.55f, 1,1,1, 0.155f);
          glColor3f(0.04f, 0.04f, 0.04f); blob(0.28f*lado, 0.40f, 0.49f, 1.5f,0.6f,1.0f, 0.085f);
          glColor3f(1, 1, 1);             blob(0.31f*lado, 0.44f, 0.46f, 1,1,1, 0.032f);
        }
      }
    glPopMatrix();
    brilhoMaterial(1.0f, 40);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    prog(0);
    desenhaCeu();                          // fundo em gradiente (pipeline fixo)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // CAMERA em 3a pessoa: mais ALTA e RECUADA -> vista aberta (menos apertada),
    // mostrando a floresta que fecha as margens.
    gluLookAt(sapoX * 0.4f, 3.0f, 9.0f,     // olho: mais alto e mais atras
              sapoX * 0.2f, 1.1f, -16.0f,   // alvo: mais longe (abre o campo)
              0, 1, 0);

    glLightfv(GL_LIGHT0, GL_POSITION, posLuz);  // luz (agora em coords de olho)

    desenhaCenario();                      // (deixa progToon ativo se shader on)

    if (estado == MENU) {
        // sapo de frente na ponte + overlay do menu
        if (usoShader()) { prog(progToon); enviaFog(progToon); glBindTexture(GL_TEXTURE_2D, texBranca); }
        desenhaSapoMenu();
        prog(0);
        desenhaMenu();
    } else {
        // JOGANDO ou GAMEOVER: cena de jogo (sombra + sapo + obstaculos + moscas)
        prog(0);
        desenhaSombraDoSapo();
        if (usoShader()) { prog(progToon); enviaFog(progToon); glBindTexture(GL_TEXTURE_2D, texBranca); }
        desenhaSapo();
        for (auto& o : obstaculos) desenhaObstaculo(o);
        for (auto& m : moscas)     desenhaMosca(POS_PISTA[m.pista], m.altura, m.z);

        prog(0);
        desenhaHUD();
        if (estado == GAMEOVER) desenhaGameOver();
    }
    glutSwapBuffers();
}

// ============================================================================
//  11) GERACAO DO MUNDO (spawn) - cria linhas de obstaculos/moscas na frente
// ============================================================================
void geraLinha(float z) {
    int pistaLivre = rand() % 3;          // garante 1 pista sempre passavel

    for (int p = 0; p < 3; p++) {
        if (p == pistaLivre) continue;
        // dificuldade cresce com a velocidade
        float chance = 0.35f + (velocidade - 14.0f) * 0.02f;
        if ((rand() / (float)RAND_MAX) < chance) {
            Obstaculo o;
            o.pista = p; o.z = z;
            int r = rand() % 3;
            o.tipo = (r == 0) ? OBST_BAIXO : (r == 1) ? OBST_ALTO : OBST_BLOCO;
            obstaculos.push_back(o);
        }
    }
    // moscas na pista livre (SO reabastecem a fome) -> poucas, esparsas
    if ((rand() / (float)RAND_MAX) < 0.4f) {
        int n = 1 + rand() % 2;                  // 1 ou 2 moscas
        float h = (rand() % 2) ? 0.6f : 1.6f;    // as altas exigem pular
        for (int i = 0; i < n; i++) {
            Mosca m; m.pista = pistaLivre; m.altura = h; m.z = z - i * 1.6f;
            moscas.push_back(m);
        }
    }
}

// ============================================================================
//  12) ATUALIZACAO (fisica + logica) - chamada pelo timer ~60x por segundo
// ============================================================================
void atualiza(float dt) {
    tempo += dt;                            // relogio sempre corre (anima menu/agua)
    if (estado != JOGANDO) return;          // menu/gameover: sem fisica

    velocidade += 0.22f * dt;               // acelera de forma mais SUAVE (era 0.35)
    float avanco = velocidade * dt;
    distancia += avanco;

    // --- desliza o sapo suavemente ate a pista alvo (TRANSLACAO) ---
    float alvoX = POS_PISTA[pista];
    sapoX += (alvoX - sapoX) * fminf(1.0f, 12.0f * dt);

    // --- fisica do pulo (TRANSLACAO vertical com GRAVIDADE) ---
    if (pulando) {
        sapoY += velY * dt;
        velY  -= GRAVIDADE * dt;
        if (sapoY <= 0.0f) { sapoY = 0.0f; velY = 0.0f; pulando = false; }
    }
    // --- tempo de rolagem/abaixar ---
    if (rolando) { tempoRola -= dt; if (tempoRola <= 0) rolando = false; }
    // --- lingua recolhe rapido ---
    if (lingua > 0) { lingua -= dt * 5.0f; if (lingua < 0) lingua = 0; }

    // --- move tudo em direcao ao jogador e recicla o que passou ---
    for (auto& o : obstaculos) o.z += avanco;
    for (auto& m : moscas)     m.z += avanco;

    // --- gera novas linhas la longe, a cada ESPACO_LINHA metros percorridos ---
    // Spawn baseado em DISTANCIA: garante que o mundo nunca "acabe" e que as
    // linhas fiquem sempre igualmente espacadas (mesmo com a velocidade subindo).
    while (distancia >= distProxLinha) {
        geraLinha(Z_SPAWN);
        distProxLinha += ESPACO_LINHA;
    }

    // --- COLISOES com obstaculos ---
    for (auto& o : obstaculos) {
        if (fabsf(o.z) > 0.7f) continue;                       // ainda nao chegou
        if (fabsf(sapoX - POS_PISTA[o.pista]) > 1.0f) continue; // outra pista -> ok
        bool bateu = false;
        if (o.tipo == OBST_BAIXO)      bateu = (sapoY < ALTURA_LIVRE);  // tinha que pular
        else if (o.tipo == OBST_ALTO)  bateu = (!rolando);              // tinha que rolar
        else /* OBST_BLOCO */          bateu = true;                    // tinha que desviar
        if (bateu) { morrer(); return; }
    }

    // --- FOME: cai com o tempo; se zerar, o sapo "morre de fome" ---
    // a fome cai MAIS RAPIDO conforme o jogo acelera (proporcional a velocidade)
    fome -= FOME_TAXA * (velocidade / 14.0f) * dt;
    if (fome <= 0.0f) { fome = 0.0f; morrer(); return; }

    // --- COLISOES com moscas (comer! -> reenche a fome) ---
    for (size_t i = 0; i < moscas.size(); ) {
        Mosca& m = moscas[i];
        float dx = sapoX - POS_PISTA[m.pista];
        float dy = (sapoY + 0.7f) - m.altura;   // 0.7 = altura da boca do sapo levantado
        float dz = m.z - 0.0f;
        if (dx*dx + dy*dy + dz*dz < 0.9f * 0.9f) {
            pontos++;
            lingua = 1.0f;                          // dispara a lingua (visual)
            fome = fminf(1.0f, fome + FOME_GANHO);  // comer reenche a barra
            moscas.erase(moscas.begin() + i);
        } else i++;
    }

    // --- limpeza: remove o que ja passou por tras da camera ---
    for (size_t i = 0; i < obstaculos.size(); )
        if (obstaculos[i].z > Z_MORTE) obstaculos.erase(obstaculos.begin() + i); else i++;
    for (size_t i = 0; i < moscas.size(); )
        if (moscas[i].z > Z_MORTE) moscas.erase(moscas.begin() + i); else i++;
}

// ============================================================================
//  13) TIMER e RESHAPE
// ============================================================================
int tempoAnterior = 0;
void timer(int) {
    int agora = glutGet(GLUT_ELAPSED_TIME);
    float dt = (agora - tempoAnterior) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;            // evita "pulo" grande se travar
    tempoAnterior = agora;

    atualiza(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);           // ~60 fps
}

void reshape(int w, int h) {
    larguraTela = w; alturaTela = h ? h : 1;
    glViewport(0, 0, w, alturaTela);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // PROJECAO PERSPECTIVA: da a sensacao 3D (objetos distantes ficam menores)
    gluPerspective(64.0, (double)w / alturaTela, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================================
//  14) ENTRADA (teclado)
// ============================================================================
void pular() {
    if (pulando) return;                         // ja esta no ar
    if (rolando) { rolando = false; tempoRola = 0; }  // CANCELA o rolamento e pula na hora
    pulando = true; velY = FORCA_PULO;
}
void rolar() {
    // FAST-FALL: se estiver no ar, mergulha rapido pro chao (como nesses jogos)
    if (pulando) { if (velY > -FORCA_PULO*2.0f) velY = -FORCA_PULO*2.2f; return; }
    if (!rolando) { rolando = true; tempoRola = 0.4f; }   // no chao: rola/abaixa (mais curto)
}

void teclas(unsigned char k, int, int) {
    // ---- MENU: digita o nome; ENTER comeca; ESC sai ----
    if (estado == MENU) {
        if (k == 13) { iniciaJogo(); return; }                        // ENTER
        if (k == 8 || k == 127) { if (!nomeJogador.empty()) nomeJogador.erase(nomeJogador.size()-1); return; }
        if (k == 27) { exit(0); }
        if (nomeJogador.size() < 12 &&
            ((k>='a'&&k<='z')||(k>='A'&&k<='Z')||(k>='0'&&k<='9')))
            nomeJogador.push_back((char)toupper(k));
        return;
    }
    // ---- GAME OVER: ENTER/R volta ao menu; ESC sai ----
    if (estado == GAMEOVER) {
        if (k == 13 || k=='r' || k=='R') voltaMenu();
        else if (k == 27 || k=='q' || k=='Q') exit(0);
        return;
    }
    // ---- JOGANDO ----
    switch (k) {
        case ' ': case 'w': case 'W': pular(); break;
        case 's': case 'S': rolar(); break;
        case 'a': case 'A': if (pista > 0) pista--; break;
        case 'd': case 'D': if (pista < 2) pista++; break;
        case 't': case 'T':                       // liga/desliga shaders
            usarShader = !usarShader;
            if (!shadersOK) printf("(shaders indisponiveis)\n");
            else printf("Shaders: %s\n", usarShader ? "ON" : "OFF (classico)");
            break;
        case 27: voltaMenu(); break;              // ESC -> volta ao menu
        case 'q': case 'Q': exit(0);
    }
}

void teclasEspeciais(int k, int, int) {
    if (estado != JOGANDO) return;
    switch (k) {
        case GLUT_KEY_LEFT:  if (pista > 0) pista--; break;
        case GLUT_KEY_RIGHT: if (pista < 2) pista++; break;
        case GLUT_KEY_UP:    pular(); break;
        case GLUT_KEY_DOWN:  rolar(); break;
    }
}

// ============================================================================
//  15) MAIN
// ============================================================================
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    // DOUBLE = duplo buffer (sem piscar); RGB = cor; DEPTH = z-buffer;
    // MULTISAMPLE = anti-aliasing (bordas lisas, sem serrilhado).
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(larguraTela, alturaTela);
    glutCreateWindow("Sapo Surfista 3D");

    initGL();
    carregaPlacar();                // le os recordes salvos (placar.txt)
    estado = MENU;                  // comeca na tela de menu

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(teclas);
    glutSpecialFunc(teclasEspeciais);

    tempoAnterior = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);

    printf("=== SAPO SURFISTA 3D ===\n");
    printf("MENU: digite o nome e tecle ENTER para jogar.\n");
    printf("Setas: trocar pista | Cima/Espaco: pular | Baixo/S: rolar\n");
    printf("Coma moscas p/ nao zerar a FOME! | ESC: menu\n");

    glutMainLoop();
    return 0;
}

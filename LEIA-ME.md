# Ja Mixei 1.4

**Criado por Ranariel.**

Sampler e remixador estilo MPC para Windows, feito em C++ (JUCE), com time-stretch profissional (Rubber Band).
Serve para remixar músicas, montar sequências, fatiar trechos para os pads e fazer transições suaves.

## Como abrir

Dê duplo clique em **`Ja Mixei.exe`**. Enquanto as ferramentas carregam, aparece a tela de inicialização. Não precisa instalar nada: é um arquivo único, sem DLLs extras.
Se o Windows SmartScreen avisar ("O Windows protegeu o computador"), clique em **Mais informações → Executar assim mesmo**.
O aviso aparece porque o programa não tem assinatura digital paga.

Na primeira execução o programa tenta usar o **modo de baixa latência do Windows**. Para trocar a placa de som ou
o tamanho do buffer, clique no ícone de ajustes no canto superior direito (**Configurações de áudio**).
Buffers menores deixam os pads com resposta mais rápida; se o som falhar ou estalar, aumente o buffer.

## Formatos de áudio

MP3, WAV, FLAC, OGG e AIFF funcionam direto. **M4A, AAC, MP4, WMA** (e WEBM/Opus, se o Windows tiver as extensões de mídia
instaladas) são lidos pelo decodificador do próprio Windows. Se algum arquivo baixado não abrir, converta para MP3 ou WAV.

## Fluxo rápido para remixar

1. **Navegador (esquerda):** clique em *Downloads* ou *Músicas*. Um clique toca uma prévia; duplo clique importa.
2. **Arranjo:** arraste a música para uma faixa de áudio. O BPM é detectado sozinho.
   A primeira música define o BPM do projeto, e as próximas entram sincronizadas (**Warp**).
3. **Cortar e editar:**
   - **Ctrl+E** corta no cursor; para mover o cursor, clique num espaço vazio ou na régua.
   - Arraste as **bordas** do clipe para aparar e o **meio** para mover. **Alt+arrastar** cria uma cópia.
   - As **bolinhas no topo** do clipe controlam o fade in e o fade out.
4. **Transições suaves:** coloque duas músicas **sobrepostas na mesma faixa** e o crossfade é automático (a área aparece marcada).
   Para transições de DJ, use o botão **A** da faixa: ele alterna entre automação de *Volume* e de *Filtro*.
   Clique para criar pontos, arraste para mover e dê duplo clique para apagar.
5. **Editor / Chop:** abra uma música (duplo clique no clipe), selecione um trecho e use **Fatiar**
   (por transientes, em partes iguais ou pela grade de BPM). Depois **Fatias → Pads**.
6. **Pads:** toque no teclado (layout MPC):

   ```
   1 2 3 4   → pads 13-16
   Q W E R   → pads 9-12
   A S D F   → pads 5-8
   Z X C V   → pads 1-4        F1-F4 = bancos A, B, C, D (64 pads)
   ```

   Cada pad tem volume, pan, tom, ataque e release, filtro, modo (one-shot, gate ou loop), grupo de corte e reverso.
7. **Sequenciador:** monte batidas clicando nos passos. **Tab** alterna entre MÚSICA (arranjo) e PADRÃO (loop estilo MPC).
   **Ctrl+R** grava o que você toca nos pads, quantizado em 1/16. Use *Inserir no arranjo* para colocar o padrão na música.
8. **Mixer:** cada faixa tem EQ de 3 bandas, filtro DJ, envios para Reverb e Delay ping-pong (sincronizado ao BPM), pan e volume.
   O master tem um limitador que impede distorção.
9. **Gravar áudio** (botão do microfone ou Ctrl+Shift+R):
   - **Mix (tudo que está tocando):** grava a música junto com os pads tocados ao vivo, efeitos e tudo mais, em MP3 ou WAV.
   - **Microfone / entrada:** grava voz ou instrumento. A gravação vai para uma faixa de áudio do arranjo, alinhada com a música.
     Use fone de ouvido se ligar a opção de ouvir a entrada.
   - Pare com Espaço ou no botão Parar. Os arquivos ficam em `Documentos\Ja Mixei\Gravações`.
10. **Exportar:** **Ctrl+Shift+E** gera **MP3 (128 a 320 kbps)**, WAV 16/24 bits, FLAC ou OGG, da música inteira ou só do loop.
   Em *Ferramentas → Reamostrar o loop para o pad*, o trecho do loop vira um sample novo (como no MPC).

## Como mixar os pads junto com a música e salvar

**Jeito 1: gravar ao vivo (mais rápido)**
1. Coloque a música no arranjo e prepare os pads (fatias ou samples).
2. Clique no botão do microfone (ou Ctrl+Shift+R), escolha **"Tudo que está tocando"** e o formato MP3, e clique em Gravar.
3. A música começa a tocar. Toque os pads no teclado por cima dela.
4. Aperte Espaço para parar. O resultado fica salvo em `Documentos\Ja Mixei\Gravações`.

**Jeito 2: programar os pads no arranjo (editável depois)**
1. Na aba **Sequenciador**, monte o padrão (ou grave ao vivo com Ctrl+R no modo PADRÃO).
2. Clique em **Inserir no arranjo**, ou dê duplo clique na faixa "Pads / Batida" do arranjo, no ponto da música desejado.
   Arraste a borda direita do clipe de padrão para repetir a batida pelo tempo que quiser.
3. Ajuste o volume dos pads e da música no **Mixer**.
4. Salve o projeto (Ctrl+S) para poder editar depois, e exporte em MP3 (Ctrl+Shift+E) para ter a música final.

## Atalhos principais

| Tecla | Ação |
|---|---|
| Espaço | Tocar / pausar |
| Enter | Parar e voltar ao início |
| Tab | Alternar MÚSICA / PADRÃO |
| Ctrl+Z / Ctrl+Y | Desfazer / refazer (até 200 passos) |
| Ctrl+E | Cortar clipe no cursor |
| Ctrl+D | Duplicar seleção |
| Ctrl+C / Ctrl+X / Ctrl+V | Copiar / recortar / colar no cursor |
| Delete | Apagar clipes selecionados |
| Ctrl+A | Selecionar todos os clipes |
| Ctrl+L / Ctrl+M / Ctrl+R | Loop / metrônomo / gravar pads |
| Ctrl+Shift+R | Gravar áudio (mix ou microfone) |
| Ctrl+S / Ctrl+O / Ctrl+N | Salvar / abrir / novo projeto |
| Ctrl+I | Importar áudio |
| Ctrl+Shift+E | Exportar mixagem |
| Ctrl+1..4 | Arranjo / Editor / Sequenciador / Mixer |
| Setas | Mover o cursor (com Ctrl, um compasso) |
| Ctrl+roda do mouse, + / - | Zoom do arranjo |
| Shift ao arrastar | Mover sem a grade |
| Esc | Parar todas as prévias e sons |

## Novidades da versão 1.4

- **Idiomas:** Português (Brasil, padrão), English, Español e 中文 (chinês simplificado).
  - Para trocar, use *Ferramentas → Idioma / Language*. O programa pergunta se pode reiniciar para aplicar.
  - O instalador também pergunta o idioma, e o programa já abre no idioma escolhido.
  - O chinês usa a fonte Noto Sans SC embutida; nomes de arquivo com ideogramas também aparecem certos nos outros idiomas.
- **Propriedades em português:** o instalador e o programa mostram as informações do arquivo (descrição, versão, direitos autorais) em português do Brasil.
- **Nome certo no Gerenciador de Tarefas:** o programa aparece como "Ja Mixei" (antes ainda aparecia "MeuPad Studio").

## Novidades da versão 1.3

- **Fontes novas e mais legíveis:** o programa traz as próprias fontes (Inter para textos e JetBrains Mono para os números), maiores e com o mesmo visual em qualquer PC.
- **Visual renovado:** ícones novos e padronizados (transporte, abas, navegador, arquivos de áudio e vídeo), barra de transporte organizada em grupos e menus com os atalhos alinhados à direita.
- **Animações e transições suaves:** botões com brilho ao passar o mouse, abas com indicador deslizante, telas que entram com transição, pads que acendem e apagam suavemente, zoom e rolagem do arranjo animados.
- **Aceleração por GPU:** a tela é desenhada pela placa de vídeo (OpenGL) quando disponível. O indicador "GPU ativa" aparece no canto inferior direito.
  Para desligar, use *Ferramentas → Usar a placa de vídeo (GPU) para desenhar a tela*. Se der problema ao abrir com a GPU, o programa volta sozinho para o modo normal na próxima abertura.
  O processamento do áudio continua no processador (é o padrão em programas de música, porque a placa de vídeo atrasaria o som).
- **Mais leve:** a tela só redesenha o que mudou (cursor, medidores, pads), a 60 quadros por segundo.

## Novidades da versão 1.2


- Novo nome: **Ja Mixei** (antes MeuPad Studio). Os projetos `.mpad` antigos abrem normalmente, e a pasta `Documentos\MeuPadStudio` é renomeada sozinha para `Documentos\Ja Mixei`.
- Tela de inicialização com o carregamento das ferramentas.
- Fontes maiores e mais fáceis de ler em todo o programa.
- Fechamento corrigido: ao fechar, o som e as gravações param na hora, e o programa encerra sem travar nem dar erro.

## Projetos

- Os projetos são salvos como `.mpad`. O arquivo guarda o caminho dos áudios usados.
- *Arquivo → Salvar copiando os áudios* cria uma pasta `NomeDoProjeto_audio` ao lado do projeto, com cópia de tudo.
  Use essa opção para levar o projeto a outro PC.
- O programa salva automaticamente a cada 2 minutos em `Documentos\Ja Mixei\Autosave`.
- As reamostragens ficam em `Documentos\Ja Mixei\Reamostragens`.

## Código-fonte

A pasta `codigo-fonte` tem o projeto completo (C++17, CMake):

- `Source/Audio`: motor de áudio, que inclui o sampler, a linha do tempo, o mixer, os efeitos, a detecção de BPM e transientes e o time-stretch.
- `Source/Model`: dados do projeto e salvamento.
- `Source/UI`: interface.
- `tests/`: testes automáticos do motor.

Para recompilar no Windows, instale o Visual Studio 2022 (com C++), o CMake e o Git e rode `compilar-windows.bat`.
O script baixa o JUCE 7.0.12, o Rubber Band e o LAME e gera o .exe.

Licenças: JUCE (GPLv3/comercial), Rubber Band Library (GPL), LAME (LGPL), fontes Inter, JetBrains Mono e Noto Sans SC (SIL Open Font License). O Ja Mixei é software livre sob a licença GPL v3 (arquivo LICENSE). Código-fonte: https://github.com/Ranariel/JaMixei

---
Ja Mixei © 2026 Ranariel.

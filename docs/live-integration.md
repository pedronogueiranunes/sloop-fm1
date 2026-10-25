# SLOOP PEEEO 6T8 + Ableton Live

A 6T8 acrescenta troca de seções A–D por **CC119 no canal REMOTE** (valores 0–3), com quantização no próximo compasso e cancelamento pelo STOP/SPP. O canal REMOTE continua sendo salvo no ajuste MIDI existente; Program Change continua selecionando presets. Seções vazias são ignoradas e uma SONG em andamento não é interrompida.

O pacote complementar inclui templates DAW/Capture com seis pistas musicais, uma pista para clips de seção, outra para o dispositivo Max for Live e um retorno estéreo. O **SLOOP Live Recall** captura os sons e efeitos no próprio Live Set e pode recuperá-los ao reabrir o projeto, incluindo os patches de trabalho FM6. Requer Max for Live e configuração das portas. Veja o [guia do pacote Ableton](../integrations/ableton/README.md) para instalação, escopo do snapshot e testes no aparelho. A validação da interface do Live e do hardware permanece pendente.

A 6T7 acrescenta saída de automações por CC, perfis de uso com DAW e sincronização por Song Position Pointer (SPP), mantendo as seis tracks e as oito vozes compartilhadas da 6T6.

## Configuração no FM-1

Abra **GLO → DAW**. Os quatro knobs editam:

| Knob | Tela | Opções |
| --- | --- | --- |
| 1 | MODE | STAND, DAW, CAPT |
| 2 | CCOUT | OFF / ON |
| 3 | SPP | OFF / ON |
| 4 | LOST | RUN, HOLD, STOP |

**STAND** mantém o roteamento habitual: SYNC, MIDI/OUT e IN continuam obedecendo aos ajustes existentes de GLO. **DAW** usa o clock USB, desliga a reprodução dos padrões internos e o som direto das teclas; as teclas continuam enviando notas. Os sons respondem às notas que voltam do Live nos canais atribuídos. O arp do preset continua disponível para notas recebidas; desligue-o se quiser que cada nota do clip seja tocada diretamente. REC no FM-1 lembra que a gravação deve ocorrer na DAW.

**CAPT** usa clock USB, toca os padrões locais e envia seu MIDI, incluindo arp e rolls locais. Ignora notas MIDI devolvidas pelo Live (note-on e note-off), evitando duplicação ou interrupção das notas locais. Continua recebendo CCs, Program Change e bend. Os ajustes standalone não são reescritos por esses perfis; voltar a STAND recupera seu comportamento. Trocar de perfil encerra notas e a reprodução atual.

**CCOUT ON** envia mudanças locais dos parâmetros suportados. Automação MIDI recebida, carregamento de presets e locks do sequencer não são reenviados. O padrão é OFF. **SPP ON** acompanha a posição enviada pela fonte de clock selecionada; o padrão é ON. Em DAW/CAPT a fonte efetiva é sempre USB, independentemente do SYNC guardado para STAND.

**LOST** controla a ausência de clock:

- **RUN** mantém o comportamento antigo: após a perda, o tempo interno continua. É o padrão de migração, preservando o uso existente.
- **HOLD** pausa o avanço musical e aguarda pulsos novamente.
- **STOP** para a reprodução quando uma fonte que estava enviando clock fica 500 ms sem pulsos. O perfil DAW/CAPT também libera notas de entrada e controladores. Retomar exige PLAY/START/CONTINUE.

START e CONTINUE externos aguardam o primeiro pulso antes de disparar um padrão, mesmo com LOST = RUN. SPP muda a posição sem iniciar a reprodução. START começa do zero; CONTINUE respeita a posição recebida ou a posição parada. Sem uma fonte externa selecionada, SPP é ignorado.

## Configuração no Live

Nas preferências MIDI da porta do FM-1:

| Direção | Track | Sync | Remote |
| --- | --- | --- | --- |
| Entrada do FM-1 | ON | OFF | OFF para começar |
| Saída para FM-1 | ON | ON | OFF para começar |

O Live será o mestre de tempo; não ative EXT. Para testar saltos no Arrangement, selecione **MIDI Clock Type = Song** na saída do FM-1. Para apenas tocar padrões em loop, Pattern continua sendo uma alternativa.

Crie seis pistas MIDI, com entrada e saída nos canais **1, 2, 3, 4, 5 e 10**, respectivamente. Esses são os padrões; se personalizar os canais no FM-1, use os mesmos no Live.

Para tocar e gravar com o Live, selecione **MODE = DAW, CCOUT = ON, SPP = ON, LOST = STOP**. Arme a pista desejada e use Monitor Auto ou In para devolver as notas ao FM-1: o local-off evita que as teclas soem duas vezes. Grave os movimentos dos knobs como CCs em clips MIDI e reproduza o clip para conferir o retorno dos parâmetros.

Para capturar os padrões internos, use **MODE = CAPT, CCOUT = ON, SPP = ON, LOST = STOP**. Arme as seis pistas para gravar e inicie o transporte no Live. Monitor Off é uma configuração simples para essa captura; mesmo se houver retorno de notas, o perfil CAPT o ignora.

O áudio USB continua sendo um único mix estéreo. Use um retorno de áudio para esse mix; seis pistas MIDI não criam seis retornos de áudio independentes. Esta versão não acrescenta controle dos botões/clip launch do Live nem um template de Live Set.

Referências do Ableton: [Track, Sync e Remote](https://www.ableton.com/en/manual/routing-and-i-o/), [MIDI Clock e tipos Song/Pattern](https://help.ableton.com/hc/en-us/articles/209071149-Synchronizing-Live-via-MIDI).

## Mapa de automação

O canal é o da track editada. Um CC aprendido para aquele parâmetro tem prioridade sobre o mapa padrão. O comportamento existente de MIDI LEARN permanece: uma atribuição aprendida aponta para uma track fixa, independentemente do canal recebido. Se um CC padrão estiver reservado por uma atribuição aprendida para outro alvo, o transmissor evita enviá-lo para o destino errado.

| CC | Parâmetro |
| --- | --- |
| 5 | Glide |
| 7 | Level; na bateria, volume real do mixer |
| 10 | Pan |
| 20–27 | Parâmetros EDIT 1–8 do engine atual, nas tracks synth |
| 70 | Sustain do envelope |
| 71 | Resonance/Q, quando existe no engine |
| 72 / 73 / 75 | Release / Attack / Decay |
| 74 | Cutoff/brightness do engine, ou filtro da track quando não existe cutoff dedicado |
| 76 | Drive/DIST |
| 77 | Filtro da track |
| 78 / 79 | Mix / tipo do INSERT |
| 80 / 81 / 82 | Parâmetros A / B / C do INSERT |
| 91 / 93 / 94 | Reverb / Chorus / Delay sends |

Na bateria são suportados os parâmetros aplicáveis ao mixer/processamento da track: level, pan, filtro, drive, INSERT, reverb e delay. Os CCs 20–27 são de synths. CC 64 continua sendo pedal e CC 11 continua sendo expressão de entrada. Os controles globais DUST, DUCK, filtro master, tempo e edição dos padrões não recebem novos CCs nesta etapa.

A saída usa 7 bits, limita atualizações a 100 Hz por controle e combina movimentos pendentes do mesmo controle. Há quatro posições pendentes, correspondentes aos quatro knobs, e oito posições da fila USB ficam reservadas para notas/transporte. Os hooks só acompanham edições locais; não varrem o estado modificado pelo áudio ou pela entrada MIDI.

## Compatibilidade e implementação

Os parâmetros G_DAW/G_CCTX/G_SPP/G_MLOSS foram acrescentados ao final dos IDs globais. Projetos continuam no formato FUN7. Ajustes de DAW são configurações do aparelho, preservadas ao criar/carregar um loop.

O objeto MIDI de backup cresce de 44 para **45 bytes**. O byte 44 contém MODE (bits 0–1), CCOUT (bit 2), SPP (bit 3) e LOST (bits 4–5). Os formatos anteriores de 32, 36, 40, 42 e 44 bytes continuam aceitos, usando os padrões STAND/OFF/ON/RUN para os novos ajustes. O editor transporta esse bloco sem alterar os bytes existentes.

- `seq.c`: perfis, destino comum de CCs de entrada/saída e processamento do transporte em ordem de chegada.
- `midi_automation.c`: alterações locais, fila curta, coalescência e limitação de frequência.
- `midi_transport.c`: liberação de notas, locate e retomada do SONG interno.
- `usb.c` / `midi_uart.c`: SPP F2, com CIN 3 em USB e cancelamento de running status no TRS.
- `arranger.h:arr_locate()`: posição absoluta de SONG convertida em seção, compasso e fase; respeita loop e fim de música.
- `params.c`, `panel.c`, `midi_settings.c`: página DAW e persistência.
- `ui_input.c`, `ui_layers.c`, `ui_studio.c`: hooks de edições feitas nos knobs.

## Testes no aparelho

- [ ] Instalar a 6T7 e confirmar a página GLO → DAW e os quatro controles legíveis.
- [ ] Em DAW, tocar uma tecla com Monitor Off: sai MIDI, sem som local. Com Monitor Auto/In e pista armada: a nota volta e soa uma vez. Testar também bateria.
- [ ] Gravar cutoff, envelope, level, pan, drive e sends em cada canal; reproduzir os clips e verificar os parâmetros e a ausência de eco.
- [ ] Repetir a automação com um parâmetro que tenha MIDI LEARN e com os knobs de mixer da bateria.
- [ ] Em CAPT, gravar as seis tracks, arp e rolls; habilitar retorno MIDI deliberadamente e confirmar que note-ons e note-offs devolvidos não afetam as notas locais.
- [ ] Com clock Song, começar em diferentes compassos do Arrangement, saltar para frente/trás e repetir um loop. Conferir padrões de comprimentos diferentes, START do zero e CONTINUE.
- [ ] Repetir SPP com SONG interno, incluindo seção intermediária, final sem loop e final com loop.
- [ ] Interromper o clock e testar RUN/HOLD/STOP. Com STOP, confirmar silêncio das notas encerradas e ausência de notas presas no Live.
- [ ] Trocar perfis com teclas/notas ativas; voltar a STAND e confirmar os ajustes anteriores de SYNC/IN/OUT.
- [ ] Desligar/ligar, restaurar backup 6T7 e restaurar backup 6T6/antigo; conferir os valores novos e os canais das seis tracks.

## Verificações desta compilação

- Teste DAW dedicado: passou, incluindo AddressSanitizer/UBSan (exceto operações de shift/wrap intencionais do DSP).
- Seek de SONG com projetos FUN7 reais, STOP/CONTINUE, START, loop/fim e restauração do loop de trabalho: passou com sanitizadores.
- MIDI de seis tracks, parsers USB/TRS, projetos, sequencer, escalas, FX, editor web e empacotamento do site: passaram.
- A suíte geral de UI mantém as três falhas LIVE do editor web já existentes na base; não apareceram novas falhas.
- Build JieLi: imagem 578944 bytes; RAM data/bss 98060/98304; lowram 22772/29488; pool 335436/344064. Checagens de limites, código em RAM e acesso HAL passaram.
- Firmware `sloop-6T7-live.fwsc`, pacote 610005 bytes. Verificação por reconstrução com app/loader recém-compilados, CRCs e atualização simulada 6T6 → 6T7 passaram.
- Teste físico no aparelho/Live permanece pendente.

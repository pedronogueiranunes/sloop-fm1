# MIDI nas seis tracks

Base analisada: `origin/six-tracks`, commit `ff16ec0`. A `main` do fork ainda tinha quatro tracks; `six-tracks-upstream` tinha o protótipo, anterior ao suporte CH4/CH5 da branch escolhida. Alterações locais na branch `midi-six-tracks`.

## Mapa do código

| Responsabilidade | Arquivo e símbolos |
| --- | --- |
| Número de tracks, posição da bateria, IDs dos parâmetros | `firmware/src/core.h`: `NPART`, `NTRK`, `TRK_DRUM`, `G_CH1..G_CH5`, `G_DRCH`; nova tabela comum `TRK_CH[NTRK]` |
| Padrões e páginas GLO | `firmware/src/params.c`: `GP`, `PAGES`, `page_desc()`, `page_id()`, `trk_view_top()` |
| Ajustes persistentes em RAM | `firmware/src/panel.c`: `MIDI_SET_DEF`, `midi_set`, `MIDI_SET_ID` |
| Ajustes ao ligar | `firmware/src/main.c`: `felucca_init()` |
| Saída das teclas | `firmware/src/seq.c`: `trk_midi_ch()`, `midi_out_note()`, `key_down()`, `key_up()` |
| Saída SEQ, arp e rolls | `firmware/src/seq.c`: `seq_out_on/off()`, `seq_out_track_off()`, `seq_out_all_off()`, `arp_play()`, `roll_hit()`, `seq_tick()`, `seq_stop()` |
| Entrada USB/TRS | `firmware/src/usb.c`: filas MIDI; `midi_uart.c`: parser TRS; ambos chegam a `seq.c:events_block()` |
| Escolha da track na entrada | `firmware/src/seq.c`: `midi_own()`, `midi_track()`, `midi_route()` |
| Program Change, pedal, bend | `seq.c`: `pc_req`, `pc_bank`, `midi_pedal()`, `midi_bend()`; `ui.c:pc_poll()` aplica o preset/kit |
| MIDI LEARN | `seq.c`: `ml_tab`, `ml_entry()`, `ml_track()`, `midi_learned()`; `ui.c`: `ml_learn()`, `ml_poll()` |
| Unicidade de canais | `firmware/src/ui.c`: `midi_chan_fix()`; a importação também normaliza colisões antes de publicar os ajustes |
| Tela e knobs | `ui_studio.c:studio_midi_draw()`; `ui_draw.c:ui_draw()`; `ui_input.c:edit_param()`; janela compartilhada com TRACKS |
| Gravação e leitura dos ajustes | `project.c`: `settings_write()`, `dsu_boot()`, `settings_tick()`; `midi_settings.c`: `midi_part_fill/read/default/size()` |
| Backup via SysEx | `editor.c`: objeto 10, `ed_bk_obj()`, `ed_bk_apply()`, `ed_backup()` |
| Arquivo de backup no navegador | `web/editor.html`: `backupCapture()`, `backupRestore()`, `bkDevFormat()`; aceita FUN7 quando INFO informa seis tracks |

Os canais já eram 1, 2, 3, 4, 5 e 10 na branch analisada. Entrada e saída CH4/CH5 já existiam. As lacunas eram detecção de mudança desses canais, aprendizado das tracks novas e restauração durante o uso. A revisão também encontrou uma exceção anterior ao anti-eco: o arp retransmitia notas alimentadas por MIDI. Agora a origem acompanha cada nota do acorde, inclusive HOLD; só pitches contribuídos por teclas locais saem pelo arp.

## Inventário das suposições de quatro tracks

### Caminho MIDI ativo

- `core.h`: `TRK_LEGACY` / `TRK_FROM_LEGACY` representam os slots históricos 0, 1, 2 e bateria em 3. Permanecem para interpretar dados antigos, não para limitar as tracks atuais.
- `seq.c:trk_midi_ch()` e `midi_own()`: seleção manual de CH1–3, CH4, CH5, DRUM. Substituída por `TRK_CH`.
- `seq.c:events_block()`: assinatura de canais de 32 bits só incluía CH1, CH2, CH3 e DRUM, além de OTHER e FX CH. CH4/CH5 não disparavam a liberação de notas/pedais/bend. Substituída por comparação dos `NTRK` canais.
- `seq.c:ml_tab` / `midi_learned()` e `ui.c:ml_learn/count/poll()`: dois bits de track, deslocamentos 7/9, máscara `& 3u`, rejeição de tracks sem slot legado. O mapa atual tem um bit adicional, com conversão explícita na persistência.
- `ui.c:midi_chan_fix()`: o upstream usava tabela e loops de quatro; a branch já usava `NTRK`. Agora compartilha a tabela de roteamento.
- `params.c:PAGES`: quatro IDs por página, com segunda página MIDI CH para as últimas tracks. Quatro continua sendo o número de knobs/linhas visíveis; `page_id()` resolve a track da janela e a lista cobre `NTRK`.
- `panel.c`, `main.c` e `project.c:settings_tick()`: índices manuais em `midi_set` (0–2 synths antigos, 3 bateria, 10/11 synths novos). O layout histórico continua; inicialização e espelhamento usam `MIDI_SET_ID`, sem repetir o comprimento 12.
- `project.c:midi_part_*()` original: os primeiros três canais e bateria eram o bloco histórico; dois bytes adicionais já guardavam CH4/CH5. Ao ler um bloco antigo não se restauravam explicitamente CH4/CH5 durante o uso. Agora recebem padrões e canais livres, sem mover o canal antigo da bateria para o synth 4.
- `editor.c:ed_bk_apply()` original: aceitava 32, 36 ou 42 bytes, rejeitando o formato upstream de 40 bytes. Agora compartilha a validação de tamanho com o carregamento da flash.
- `web/editor.html:bkDevFormat()`: máximo FUN6 pelo número de parâmetros, mesmo com seis tracks. INFO com seis tracks habilita FUN7. O mock também passa a aceitar objetos MIDI de 42/44 bytes.

### Formatos antigos e outros limites preservados

- `project.c`: `PROJ_NTRK = 4`, `PROJ_DRUM = 3`, arrays `t[PROJ_NTRK]`, loops/conversões dos formatos FUN1–FUN6, `proj_live()`, `proj_sel_live()`, `proj_from_v6()` e a antiga seleção de dois bits. São o formato no disco; alterar para seis quebraria backups antigos. FUN7 já representa as seis tracks.
- `voice.c`: `vis_hit`, `vis_note`, `TRK_LEGACY`; `ui_vis.c`: mapeamento `TRK_FROM_LEGACY`, medidores/formas de quatro tracks. São limites dos visualizadores existentes, fora do roteamento MIDI.
- `ui_studio.c` / `ui_layers.c`: quatro linhas e quatro knobs da janela TRACKS. São limites físicos da interface, preservados.
- `ui_draw.c:draw_tracks()` antigo: gráfico legado de quatro colunas de 60 pixels. A tela TRACKS atual usa `studio_tracks_draw()`; a nova tela MIDI usa o mesmo estilo de lista.
- `web/editor.html`: mock com quatro tracks, `GM_DRUM`, parser LIVE, controles e outras telas do editor ainda presumem quatro em vários pontos. Este trabalho corrige o transporte de backups; não converte essas telas para seis. A suíte de UI já falhava em três verificações LIVE na base, com os mesmos resultados após as alterações.
- `seq.c`: `mo_set[NTRK][4]`, `sus_wait[NTRK][4]`, `kh_set/kh_mo[NTRK][4]`, `mi_pen[4]`, `mi_held[4]`, `er_notes[4]`, loops de quatro palavras: representam as 128 notas MIDI, não quatro tracks.
- Arrays de quatro notas por step/acorde, `NROLL = 4`, quatro seções A–D, quatro kits/slots, valores de ratchet e máscaras `& 3u` desses campos não representam o número de tracks.

## Formato MIDI

O objeto 10 aceita 32, 36, 40, 42 e 44 bytes. Os 42 bytes existentes permanecem nas mesmas posições; dois bytes novos guardam um bit extra para cada entrada de MIDI LEARN. O slot legado 3 continua identificando bateria, agora track 6. MIDI LEARN mantém a regra original: o CC aprendido controla a track associada independentemente do canal de origem, sujeito ao filtro IN.

Um backup antigo repõe CH4/CH5 em 4/5. Se esses canais já pertencem a uma track antiga, escolhe os próximos livres; preserva primeiro os canais antigos, incluindo o canal da bateria. OFF pode ser compartilhado porque significa ausência de canal.

## Validação

- `midi_tracks_test`: roteamento, sequencer real com seis tracks, arp, rolls, note-off, STOP, anti-eco direto e do arp alimentado por MIDI (incluindo HOLD/oitavas), mudança de canal, sustain, bend, Program Change, learn, interface e migração de backups. Passou também com AddressSanitizer/UBSan (excluídas as operações de wrap/shift intencionais do DSP).
- `project_test`, `seq2_test` e `scale_test`: passaram.
- `ui_pages_test`: mesmas três falhas LIVE presentes em `ff16ec0`; nenhuma falha nova, incluindo rolagem/knobs MIDI.
- `web/test_web.mjs`: passou, incluindo os blocos de 42/44 bytes e o limite FUN7 condicionado a seis tracks.
- Firmware instalável gerado: `build/sloop-6T6-midi.fwsc`, versão **SLOOP 2.8 PEEEO 6T6**, 610035 bytes. Build JieLi validou imagem de 575000 bytes; RAM data/bss 98028/98304; pool 335436/344064; lowram 22772/29488. CRCs do pacote, correspondência com app/loader recém-compilados, testes de loader e instalação simulada passaram. Os três arquivos do SDK foram extraídos da release oficial SLOOP 2.8.1 e seus SHA-256 coincidem com os exigidos em `tools/build.py`. Manifesto em `build/sloop-6T6-midi-build.json`. Validação física permanece pendente.
- Comandos: compilação host conforme `tests/run_tests.sh`; o novo teste foi incluído nessa suíte. Cabeçalhos gerados por `tools/build.py:generate()`.

## Checklist no FM-1 / Ableton

- [ ] Fazer backup do aparelho antes de instalar. Compilar e conferir os limites de memória/flash no ambiente JieLi.
- [ ] Em GLO → MIDI, conferir tracks 1–6 em CH1, CH2, CH3, CH4, CH5 e CH10. Girar ALGORITHM até a bateria, conferir rolagem e rótulos dos quatro knobs. Editar track 5 e bateria.
- [ ] Tentar repetir canais, inclusive CH5 e CH10; confirmar avanço para canal livre. Testar OFF e restaurar os padrões.
- [ ] No Ableton, criar seis tracks MIDI recebendo do FM-1, cada uma filtrada em 1, 2, 3, 4, 5 ou 10. Selecionar GLO → SYSTEM → MIDI = SEQ.
- [ ] Gravar um padrão distinto de cada track; confirmar notas apenas na pista correspondente. Gravar as seis juntas e conferir bateria no 10.
- [ ] Testar arp e rolls na track 5 e bateria; conferir canal, note-off e encerramento com STOP. Testar trocar SEQ para KEYS enquanto há notas de sequência.
- [ ] Enviar teclado/DAW para cada canal. CH5 toca synth 5, CH10 toca bateria, independentemente da track selecionada quando IN = NOTES. Monitorar a saída do FM-1 e confirmar que notas MIDI recebidas não voltam ao Ableton, inclusive com arp, oitavas e HOLD. Testar também um acorde misto com teclas locais: só as contribuições locais saem pelo arp.
- [ ] Em cada synth, testar sustain, sua liberação e pitch bend. Testar MIDI LEARN na track 5 e bateria, Program Change/Bank Select nas novas tracks; bateria continua ignorando sustain/bend conforme comportamento original.
- [ ] Sustentar notas de sequência e entrada, mudar CH4/CH5 e conferir note-offs no canal antigo, pedal/bend limpos e novas notas no canal novo.
- [ ] Salvar canais personalizados e LEARN, desligar/ligar e restaurar um backup novo. Confirmar as seis atribuições e os controles aprendidos.
- [ ] Restaurar backup upstream antigo (inclusive objeto MIDI de 40 bytes) depois de personalizar canais novos. Confirmar canais antigos, bateria na track 6 e CH4/CH5 nos padrões livres. Repetir com backup antigo que já usa canais 4/5.
- [ ] Repetir PLAY/STOP, incluindo notas longas, arp e rolls; soltar teclas e pedal e verificar que não fica nenhuma nota presa no FM-1 nem no Ableton.
